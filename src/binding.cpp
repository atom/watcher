#include <string>
#include <memory>
#include <utility>
#include <nan.h>
#include <v8.h>

#include "nan/all_callback.h"
#include "nan/options.h"
#include "hub.h"

using v8::Local;
using v8::Value;
using v8::Object;
using v8::String;
using v8::Uint32;
using v8::Boolean;
using v8::Function;
using v8::FunctionTemplate;
using std::string;
using std::unique_ptr;
using std::move;

void configure(const Nan::FunctionCallbackInfo<Value> &info)
{
  string main_log_file;
  bool main_log_disable = false;
  bool main_log_stderr = false;
  bool main_log_stdout = false;

  string worker_log_file;
  bool worker_log_disable = false;
  bool worker_log_stderr = false;
  bool worker_log_stdout = false;

  string polling_log_file;
  bool polling_log_disable = false;
  bool polling_log_stderr = false;
  bool polling_log_stdout = false;

  Nan::MaybeLocal<Object> maybe_options = Nan::To<Object>(info[0]);
  if (maybe_options.IsEmpty()) {
    Nan::ThrowError("configure() requires an option object");
    return;
  }

  Local<Object> options = maybe_options.ToLocalChecked();
  if (!get_string_option(options, "mainLogFile", main_log_file)) return;
  if (!get_bool_option(options, "mainLogDisable", main_log_disable)) return;
  if (!get_bool_option(options, "mainLogStderr", main_log_stderr)) return;
  if (!get_bool_option(options, "mainLogStdout", main_log_stdout)) return;

  if (!get_string_option(options, "workerLogFile", worker_log_file)) return;
  if (!get_bool_option(options, "workerLogDisable", worker_log_disable)) return;
  if (!get_bool_option(options, "workerLogStderr", worker_log_stderr)) return;
  if (!get_bool_option(options, "workerLogStdout", worker_log_stdout)) return;

  if (!get_string_option(options, "pollingLogFile", polling_log_file)) return;
  if (!get_bool_option(options, "pollingLogDisable", polling_log_disable)) return;
  if (!get_bool_option(options, "pollingLogStderr", polling_log_stderr)) return;
  if (!get_bool_option(options, "pollingLogStdout", polling_log_stdout)) return;

  unique_ptr<Nan::Callback> callback(new Nan::Callback(info[1].As<Function>()));
  AllCallback &all = AllCallback::create(move(callback));

  if (main_log_disable) {
    Hub::get().disable_main_log();
  } else if (!main_log_file.empty()) {
    Hub::get().use_main_log_file(move(main_log_file));
  } else if (main_log_stderr) {
    Hub::get().use_main_log_stderr();
  } else if (main_log_stdout) {
    Hub::get().use_main_log_stdout();
  }

  Result<> wr = ok_result();
  if (worker_log_disable) {
    wr = Hub::get().disable_worker_log(all.create_callback());
  } else if (!worker_log_file.empty()) {
    wr = Hub::get().use_worker_log_file(move(worker_log_file), all.create_callback());
  } else if (worker_log_stderr) {
    wr = Hub::get().use_worker_log_stderr(all.create_callback());
  } else if (worker_log_stdout) {
    wr = Hub::get().use_worker_log_stdout(all.create_callback());
  }

  Result<> pr = ok_result();
  if (polling_log_disable) {
    pr = Hub::get().disable_polling_log(all.create_callback());
  } else if (!polling_log_file.empty()) {
    pr = Hub::get().use_polling_log_file(move(polling_log_file), all.create_callback());
  } else if (polling_log_stderr) {
    pr = Hub::get().use_polling_log_stderr(all.create_callback());
  } else if (polling_log_stdout) {
    pr = Hub::get().use_polling_log_stdout(all.create_callback());
  }

  all.fire_if_empty();
}

void watch(const Nan::FunctionCallbackInfo<Value> &info)
{
  if (info.Length() != 4) {
    return Nan::ThrowError("watch() requires four arguments");
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

  Nan::MaybeLocal<Object> maybe_options = Nan::To<Object>(info[1]);
  if (maybe_options.IsEmpty()) {
    Nan::ThrowError("watch() requires an option object");
    return;
  }
  Local<Object> options = maybe_options.ToLocalChecked();

  bool poll = false;
  if (!get_bool_option(options, "poll", poll)) return;

  unique_ptr<Nan::Callback> ack_callback(new Nan::Callback(info[2].As<Function>()));
  unique_ptr<Nan::Callback> event_callback(new Nan::Callback(info[3].As<Function>()));

  Result<> r = Hub::get().watch(move(root_str), poll, move(ack_callback), move(event_callback));
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

  Result<> r = Hub::get().unwatch(channel_id, move(ack_callback));
  if (r.is_error()) {
    Nan::ThrowError(r.get_error().c_str());
  }
}

void status(const Nan::FunctionCallbackInfo<Value> &info)
{
  Status status;
  Hub::get().collect_status(status);

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
  Nan::Set(
    status_object,
    Nan::New<String>("pollingThreadActive").ToLocalChecked(),
    Nan::New<Boolean>(status.polling_thread_active)
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
