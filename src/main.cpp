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
#include "worker/worker_thread.h"

using v8::Local;
using v8::Value;
using v8::Object;
using v8::String;
using v8::Function;
using v8::FunctionTemplate;
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

    err = uv_async_init(uv_default_loop(), &event_handler, handle_events_helper);
    if (err) return;

    worker_thread.run();
  }

  void use_main_log_file(string &&main_log_file)
  {
    Logger::to_file(main_log_file.c_str());
  }

  void use_worker_log_file(string &&worker_log_file, unique_ptr<Nan::Callback> callback)
  {
    CommandID command_id = next_command_id;

    CommandPayload command_payload(next_command_id, COMMAND_LOG_FILE, move(worker_log_file));
    Message log_file_message(move(command_payload));

    pending_callbacks.emplace(command_id, move(callback));

    next_command_id++;

    LOGGER << "Sending command " << log_file_message << " to worker thread." << endl;
    worker_thread.send(move(log_file_message));
  }

  void watch(string &&root, unique_ptr<Nan::Callback> callback)
  {
    CommandID command_id = next_command_id;

    CommandPayload command_payload(next_command_id, COMMAND_ADD, move(root));
    Message add_root_message(move(command_payload));

    pending_callbacks.emplace(command_id, move(callback));

    next_command_id++;

    LOGGER << "Sending command " << add_root_message << " to worker thread." << endl;
    worker_thread.send(move(add_root_message));
  }

  void handle_events()
  {
    Nan::HandleScope scope;

    LOGGER << "Handling messages from the worker thread." << endl;

    unique_ptr<vector<Message>> accepted = worker_thread.receive_all();
    if (!accepted) {
      LOGGER << "No messages waiting." << endl;
      return;
    }

    LOGGER << accepted->size() << " messages to process." << endl;

    for (auto it = accepted->begin(); it != accepted->end(); ++it) {
      const AckPayload *ack_message = it->as_ack();
      if (ack_message) {
        auto maybe_callback = pending_callbacks.find(ack_message->get_key());
        if (maybe_callback == pending_callbacks.end()) {
          LOGGER << "Ignoring unexpected ack " << *it << endl;
          continue;
        }

        unique_ptr<Nan::Callback> callback = move(maybe_callback->second);
        pending_callbacks.erase(maybe_callback);

        callback->Call(0, nullptr);
        continue;
      }

      const FileSystemPayload *filesystem_message = it->as_filesystem();
      if (filesystem_message) {
        LOGGER << "Received filesystem event message " << *it << endl;
        continue;
      }

      LOGGER << "Received unexpected message " << *it << endl;
    }
  }

private:
  uv_async_t event_handler;

  WorkerThread worker_thread;

  CommandID next_command_id;
  unordered_map<CommandID, unique_ptr<Nan::Callback>> pending_callbacks;
};

static Main instance;

static void handle_events_helper(uv_async_t *handle)
{
  instance.handle_events();
}

static bool get_string_option(Local<Object>& options, const char *key_name, string &out)
{
  Nan::HandleScope scope;
  const Local<String> key = Nan::New<String>(key_name).ToLocalChecked();

  Nan::MaybeLocal<Value> as_maybe_value = Nan::Get(options, key);
  if (as_maybe_value.IsEmpty()) {
    return true;
  }
  Local<Value> as_value = as_maybe_value.ToLocalChecked();
  if (as_value->IsUndefined()) {
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
  string main_log_file;
  string worker_log_file;
  bool async = false;

  Nan::MaybeLocal<Object> maybe_options = Nan::To<Object>(info[0]);
  if (maybe_options.IsEmpty()) {
    Nan::ThrowError("configure() requires an option object");
    return;
  }

  Local<Object> options = maybe_options.ToLocalChecked();
  if (!get_string_option(options, "mainLogFile", main_log_file)) return;
  if (!get_string_option(options, "workerLogFile", worker_log_file)) return;

  unique_ptr<Nan::Callback> callback(new Nan::Callback(info[1].As<Function>()));

  if (!main_log_file.empty()) {
    instance.use_main_log_file(move(main_log_file));
  }

  if (!worker_log_file.empty()) {
    instance.use_worker_log_file(move(worker_log_file), move(callback));
    async = true;
  }

  if (!async) {
    callback->Call(0, 0);
  }
}

void watch(const Nan::FunctionCallbackInfo<Value> &info)
{
  if (info.Length() != 2) {
    return Nan::ThrowError("watch() requires two arguments");
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

  unique_ptr<Nan::Callback> callback(new Nan::Callback(info[1].As<Function>()));

  instance.watch(move(root_str), move(callback));
}

void unwatch(const Nan::FunctionCallbackInfo<Value> &info)
{
  if (info.Length() != 2) {
    return Nan::ThrowError("watch() requires two arguments");
  }
}

void initialize(Local<Object> exports)
{
  exports->Set(
    Nan::New<String>("configure").ToLocalChecked(),
    Nan::GetFunction(Nan::New<FunctionTemplate>(configure)).ToLocalChecked()
  );
  exports->Set(
    Nan::New<String>("watch").ToLocalChecked(),
    Nan::GetFunction(Nan::New<FunctionTemplate>(watch)).ToLocalChecked()
  );
  exports->Set(
    Nan::New<String>("unwatch").ToLocalChecked(),
    Nan::GetFunction(Nan::New<FunctionTemplate>(unwatch)).ToLocalChecked()
  );
}

NODE_MODULE(sfw, initialize);
