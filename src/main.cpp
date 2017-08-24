#include <memory>
#include <iostream>
#include <string>
#include <sstream>
#include <functional>
#include <utility>
#include <unordered_map>
#include <vector>
#include <nan.h>
#include <v8.h>
#include <uv.h>

#include "log.h"
#include "queue.h"
#include "status.h"
#include "result.h"
#include "worker/worker_thread.h"

using v8::Local;
using v8::Value;
using v8::Object;
using v8::String;
using v8::Number;
using v8::Uint32;
using v8::Function;
using v8::FunctionTemplate;
using v8::Array;
using std::shared_ptr;
using std::unique_ptr;
using std::string;
using std::ostringstream;
using std::endl;
using std::unordered_map;
using std::vector;
using std::move;
using std::make_pair;

static void handle_events_helper(uv_async_t *handle);

class Main {
public:
  Main() : worker_thread{&event_handler}
  {
    int err;

    next_command_id = 0;
    next_channel_id = NULL_CHANNEL_ID + 1;

    err = uv_async_init(uv_default_loop(), &event_handler, handle_events_helper);
    if (err) return;

    worker_thread.run();
  }

  Result<> send_worker_command(
    const CommandAction action,
    const std::string &&root,
    unique_ptr<Nan::Callback> callback,
    ChannelID channel_id = NULL_CHANNEL_ID
  )
  {
    CommandID command_id = next_command_id;

    CommandPayload command_payload(next_command_id, action, move(root), channel_id);
    Message command_message(move(command_payload));

    pending_callbacks.emplace(command_id, move(callback));

    next_command_id++;

    LOGGER << "Sending command " << command_message << " to worker thread." << endl;
    return worker_thread.send(move(command_message));
  }

  void use_main_log_file(string &&main_log_file)
  {
    Logger::to_file(main_log_file.c_str());
  }

  void use_main_log_stderr()
  {
    Logger::to_stderr();
  }

  void use_main_log_stdout()
  {
    Logger::to_stdout();
  }

  void disable_main_log()
  {
    Logger::disable();
  }

  Result<> use_worker_log_file(string &&worker_log_file, unique_ptr<Nan::Callback> callback)
  {
    return send_worker_command(COMMAND_LOG_FILE, move(worker_log_file), move(callback));
  }

  Result<> use_worker_log_stderr(unique_ptr<Nan::Callback> callback)
  {
    return send_worker_command(COMMAND_LOG_STDERR, "", move(callback));
  }

  Result<> use_worker_log_stdout(unique_ptr<Nan::Callback> callback)
  {
    return send_worker_command(COMMAND_LOG_STDOUT, "", move(callback));
  }

  Result<> disable_worker_log(unique_ptr<Nan::Callback> callback)
  {
    return send_worker_command(COMMAND_LOG_DISABLE, "", move(callback));
  }

  Result<> watch(string &&root, unique_ptr<Nan::Callback> ack_callback, unique_ptr<Nan::Callback> event_callback)
  {
    ChannelID channel_id = next_channel_id;
    next_channel_id++;

    channel_callbacks.emplace(channel_id, move(event_callback));

    return send_worker_command(COMMAND_ADD, move(root), move(ack_callback), channel_id);
  }

  Result<> unwatch(ChannelID channel_id, unique_ptr<Nan::Callback> ack_callback)
  {
    string root;
    Result<> r = send_worker_command(COMMAND_REMOVE, move(root), move(ack_callback), channel_id);

    auto maybe_event_callback = channel_callbacks.find(channel_id);
    if (maybe_event_callback == channel_callbacks.end()) {
      LOGGER << "Channel " << channel_id << " already has no event callback." << endl;
      return ok_result();
    }
    channel_callbacks.erase(maybe_event_callback);
    return r;
  }

  void handle_events()
  {
    Nan::HandleScope scope;

    Result< unique_ptr<vector<Message>> > rr = worker_thread.receive_all();
    if (rr.is_error()) {
      LOGGER << "Unable to receive messages from the worker thread: " << rr << "." << endl;
      return;
    }

    unique_ptr<vector<Message>> &accepted = rr.get_value();
    if (!accepted) {
      // No events to process.
      return;
    }

    unordered_map<ChannelID, vector<Local<Object>>> to_deliver;

    for (auto it = accepted->begin(); it != accepted->end(); ++it) {
      const AckPayload *ack_message = it->as_ack();
      if (ack_message) {
        LOGGER << "Received ack message " << *it << "." << endl;

        auto maybe_callback = pending_callbacks.find(ack_message->get_key());
        if (maybe_callback == pending_callbacks.end()) {
          LOGGER << "Ignoring unexpected ack " << *it << "." << endl;
          continue;
        }

        unique_ptr<Nan::Callback> callback = move(maybe_callback->second);
        pending_callbacks.erase(maybe_callback);

        ChannelID channel_id = ack_message->get_channel_id();
        if (channel_id != NULL_CHANNEL_ID) {
          if (ack_message->was_successful()) {
            Local<Value> argv[] = {Nan::Null(), Nan::New<Number>(channel_id)};
            callback->Call(2, argv);
          } else {
            Local<Value> err = Nan::Error(ack_message->get_message().c_str());
            Local<Value> argv[] = {err, Nan::Null()};
            callback->Call(2, argv);
          }
        } else {
          callback->Call(0, nullptr);
        }

        continue;
      }

      const FileSystemPayload *filesystem_message = it->as_filesystem();
      if (filesystem_message) {
        LOGGER << "Received filesystem event message " << *it << "." << endl;

        ChannelID channel_id = filesystem_message->get_channel_id();

        Local<Object> js_event = Nan::New<Object>();
        js_event->Set(
          Nan::New<String>("actionType").ToLocalChecked(),
          Nan::New<Number>(static_cast<int>(filesystem_message->get_filesystem_action()))
        );
        js_event->Set(
          Nan::New<String>("entryKind").ToLocalChecked(),
          Nan::New<Number>(static_cast<int>(filesystem_message->get_entry_kind()))
        );
        js_event->Set(
          Nan::New<String>("oldPath").ToLocalChecked(),
          Nan::New<String>(filesystem_message->get_old_path()).ToLocalChecked()
        );
        js_event->Set(
          Nan::New<String>("newPath").ToLocalChecked(),
          Nan::New<String>(filesystem_message->get_new_path()).ToLocalChecked()
        );

        to_deliver[channel_id].push_back(js_event);
        continue;
      }

      LOGGER << "Received unexpected message " << *it << "." << endl;
    }

    for (auto it = to_deliver.begin(); it != to_deliver.end(); ++it) {
      ChannelID channel_id = it->first;
      vector<Local<Object>> js_events = it->second;

      auto maybe_callback = channel_callbacks.find(channel_id);
      if (maybe_callback == channel_callbacks.end()) {
        LOGGER << "Ignoring unexpected filesystem event channel " << channel_id << "." << endl;
        continue;
      }
      shared_ptr<Nan::Callback> callback = maybe_callback->second;

      LOGGER << "Dispatching " << js_events.size()
        << " event(s) on channel " << channel_id << " to node callbacks." << endl;

      Local<Array> js_array = Nan::New<Array>(js_events.size());

      int index = 0;
      for (auto et = js_events.begin(); et != js_events.end(); ++et) {
        js_array->Set(index, *et);
        index++;
      }

      Local<Value> argv[] = {
        Nan::Null(),
        js_array
      };
      callback->Call(2, argv);
    }
  }

  void collect_status(Status &status)
  {
    status.pending_callback_count = pending_callbacks.size();
    status.channel_callback_count = channel_callbacks.size();

    worker_thread.collect_status(status);
  }

private:
  uv_async_t event_handler;

  WorkerThread worker_thread;

  CommandID next_command_id;
  ChannelID next_channel_id;

  unordered_map<CommandID, unique_ptr<Nan::Callback>> pending_callbacks;
  unordered_map<ChannelID, shared_ptr<Nan::Callback>> channel_callbacks;
};

static Main instance;

static void handle_events_helper(uv_async_t *handle)
{
  instance.handle_events();
}

static bool get_string_option(Local<Object>& options, const char *key_name, bool &null, string &out)
{
  Nan::HandleScope scope;
  null = false;
  const Local<String> key = Nan::New<String>(key_name).ToLocalChecked();

  Nan::MaybeLocal<Value> as_maybe_value = Nan::Get(options, key);
  if (as_maybe_value.IsEmpty()) {
    return true;
  }
  Local<Value> as_value = as_maybe_value.ToLocalChecked();
  if (as_value->IsUndefined()) {
    return true;
  }

  if (as_value->IsNull()) {
    null = true;
    return true;
  }

  if (!as_value->IsString()) {
    ostringstream message;
    message << "configure() option " << key_name << " must be a String";
    Nan::ThrowError(message.str().c_str());
    return false;
  }

  Nan::Utf8String as_string(as_value);

  if (*as_string == nullptr) {
    ostringstream message;
    message << "configure() option " << key_name << " must be a valid UTF-8 String";
    Nan::ThrowError(message.str().c_str());
    return false;
  }

  out.assign(*as_string, as_string.length());
  return true;
}

void configure(const Nan::FunctionCallbackInfo<Value> &info)
{
  bool main_log_file_null = false;
  string main_log_file;

  bool worker_log_file_null = false;
  string worker_log_file;

  bool async = false;

  Nan::MaybeLocal<Object> maybe_options = Nan::To<Object>(info[0]);
  if (maybe_options.IsEmpty()) {
    Nan::ThrowError("configure() requires an option object");
    return;
  }

  Local<Object> options = maybe_options.ToLocalChecked();
  if (!get_string_option(options, "mainLogFile", main_log_file_null, main_log_file)) return;
  if (!get_string_option(options, "workerLogFile", worker_log_file_null, worker_log_file)) return;

  unique_ptr<Nan::Callback> callback(new Nan::Callback(info[1].As<Function>()));

  if (!main_log_file.empty()) {
    instance.use_main_log_file(move(main_log_file));
  }
  if (main_log_file_null) {
    instance.disable_main_log();
  }

  if (!worker_log_file.empty()) {
    Result<> r = instance.use_worker_log_file(move(worker_log_file), move(callback));
    if (r.is_error()) {
      Nan::ThrowError(r.get_error().c_str());
      return;
    }
    async = true;
  }
  if (worker_log_file_null) {
    Result<> r = instance.disable_worker_log(move(callback));
    if (r.is_error()) {
      Nan::ThrowError(r.get_error().c_str());
      return;
    }
    async = true;
  }

  if (!async) {
    callback->Call(0, 0);
  }
}

void watch(const Nan::FunctionCallbackInfo<Value> &info)
{
  if (info.Length() != 3) {
    return Nan::ThrowError("watch() requires three arguments");
  }

  Nan::MaybeLocal<String> maybe_root = Nan::To<String>(info[0]);
  if (maybe_root.IsEmpty()) {
    Nan::ThrowError("watch() requires a string as argument one");
    return;
  }
  Local<String> root_v8_string = maybe_root.ToLocalChecked();
  Nan::Utf8String root_utf8(root_v8_string);
  if (*root_utf8 == nullptr) {
    Nan::ThrowError("watch() argument one must be a valid UTF-8 string");
    return;
  }
  string root_str(*root_utf8, root_utf8.length());

  unique_ptr<Nan::Callback> ack_callback(new Nan::Callback(info[1].As<Function>()));
  unique_ptr<Nan::Callback> event_callback(new Nan::Callback(info[2].As<Function>()));

  Result<> r = instance.watch(move(root_str), move(ack_callback), move(event_callback));
  if (r.is_error()) {
    Nan::ThrowError(r.get_error().c_str());
  }
}

void unwatch(const Nan::FunctionCallbackInfo<Value> &info)
{
  if (info.Length() != 2) {
    Nan::ThrowError("unwatch() requires two arguments");
    return;
  }

  Nan::Maybe<uint32_t> maybe_channel_id = Nan::To<uint32_t>(info[0]);
  if (maybe_channel_id.IsNothing()) {
    Nan::ThrowError("unwatch() requires a channel ID as its first argument");
    return;
  }
  ChannelID channel_id = static_cast<ChannelID>(maybe_channel_id.FromJust());

  unique_ptr<Nan::Callback> ack_callback(new Nan::Callback(info[1].As<Function>()));

  Result<> r = instance.unwatch(channel_id, move(ack_callback));
  if (r.is_error()) {
    Nan::ThrowError(r.get_error().c_str());
  }
}

void status(const Nan::FunctionCallbackInfo<Value> &info)
{
  Status status;
  instance.collect_status(status);

  Local<Object> status_object = Nan::New<Object>();
  Nan::Set(
    status_object,
    Nan::New<String>("pendingCallbackCount").ToLocalChecked(),
    Nan::New<Uint32>(static_cast<uint32_t>(status.pending_callback_count))
  );
  Nan::Set(
    status_object,
    Nan::New<String>("channelCallbackCount").ToLocalChecked(),
    Nan::New<Uint32>(static_cast<uint32_t>(status.channel_callback_count))
  );
  Nan::Set(
    status_object,
    Nan::New<String>("workerThreadOk").ToLocalChecked(),
    Nan::New<String>(status.worker_thread_ok).ToLocalChecked()
  );
  Nan::Set(
    status_object,
    Nan::New<String>("workerInSize").ToLocalChecked(),
    Nan::New<Uint32>(static_cast<uint32_t>(status.worker_in_size))
  );
  Nan::Set(
    status_object,
    Nan::New<String>("workerInOk").ToLocalChecked(),
    Nan::New<String>(status.worker_in_ok).ToLocalChecked()
  );
  Nan::Set(
    status_object,
    Nan::New<String>("workerOutSize").ToLocalChecked(),
    Nan::New<Uint32>(static_cast<uint32_t>(status.worker_out_size))
  );
  Nan::Set(
    status_object,
    Nan::New<String>("workerOutOk").ToLocalChecked(),
    Nan::New<String>(status.worker_out_ok).ToLocalChecked()
  );
  info.GetReturnValue().Set(status_object);
}

void initialize(Local<Object> exports)
{
  Nan::Set(
    exports,
    Nan::New<String>("configure").ToLocalChecked(),
    Nan::GetFunction(Nan::New<FunctionTemplate>(configure)).ToLocalChecked()
  );
  Nan::Set(
    exports,
    Nan::New<String>("watch").ToLocalChecked(),
    Nan::GetFunction(Nan::New<FunctionTemplate>(watch)).ToLocalChecked()
  );
  Nan::Set(
    exports,
    Nan::New<String>("unwatch").ToLocalChecked(),
    Nan::GetFunction(Nan::New<FunctionTemplate>(unwatch)).ToLocalChecked()
  );
  Nan::Set(
    exports,
    Nan::New<String>("status").ToLocalChecked(),
    Nan::GetFunction(Nan::New<FunctionTemplate>(status)).ToLocalChecked()
  );
}

NODE_MODULE(watcher, initialize);
