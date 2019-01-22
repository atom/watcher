#include <memory>
#include <nan.h>
#include <string>
#include <utility>
#include <v8.h>

#include "hub.h"
#include "nan/all_callback.h"
#include "nan/async_callback.h"
#include "nan/options.h"

using std::endl;
using std::move;
using std::shared_ptr;
using std::string;
using std::unique_ptr;
using v8::Function;
using v8::FunctionTemplate;
using v8::Local;
using v8::Object;
using v8::String;
using v8::Value;

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
  uint_fast32_t worker_cache_size = 0;

  string polling_log_file;
  bool polling_log_disable = false;
  bool polling_log_stderr = false;
  bool polling_log_stdout = false;
  uint_fast32_t polling_interval = 0;
  uint_fast32_t polling_throttle = 0;

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
  if (!get_uint_option(options, "workerCacheSize", worker_cache_size)) return;

  if (!get_string_option(options, "pollingLogFile", polling_log_file)) return;
  if (!get_bool_option(options, "pollingLogDisable", polling_log_disable)) return;
  if (!get_bool_option(options, "pollingLogStderr", polling_log_stderr)) return;
  if (!get_bool_option(options, "pollingLogStdout", polling_log_stdout)) return;
  if (!get_uint_option(options, "pollingInterval", polling_interval)) return;
  if (!get_uint_option(options, "pollingThrottle", polling_throttle)) return;

  unique_ptr<AsyncCallback> callback(new AsyncCallback("@atom/watcher:configure", info[1].As<Function>()));
  shared_ptr<AllCallback> all = AllCallback::create(move(callback));

  Result<> r = ok_result();

  if (main_log_disable) {
    r &= Hub::get()->disable_main_log();
  } else if (!main_log_file.empty()) {
    r &= Hub::get()->use_main_log_file(move(main_log_file));
  } else if (main_log_stderr) {
    r &= Hub::get()->use_main_log_stderr();
  } else if (main_log_stdout) {
    r &= Hub::get()->use_main_log_stdout();
  }

  if (worker_log_disable) {
    r &= Hub::get()->disable_worker_log(all->create_callback("@atom/watcher:binding.configure.disable_worker_log"));
  } else if (!worker_log_file.empty()) {
    r &= Hub::get()->use_worker_log_file(
      move(worker_log_file), all->create_callback("@atom/watcher:binding.configure.use_worker_log_file"));
  } else if (worker_log_stderr) {
    r &=
      Hub::get()->use_worker_log_stderr(all->create_callback("@atom/watcher:binding.configure.use_worker_log_stderr"));
  } else if (worker_log_stdout) {
    r &=
      Hub::get()->use_worker_log_stdout(all->create_callback("@atom/watcher:binding.configure.use_worker_log_stdout"));
  }

  if (worker_cache_size > 0) {
    r &= Hub::get()->worker_cache_size(
      worker_cache_size, all->create_callback("@atom/watcher:binding.configure.worker_cache_size"));
  }

  if (polling_log_disable) {
    r &= Hub::get()->disable_polling_log(all->create_callback("@atom/watcher:binding.configure.disable_polling_log"));
  } else if (!polling_log_file.empty()) {
    r &= Hub::get()->use_polling_log_file(
      move(polling_log_file), all->create_callback("@atom/watcher:binding.configure.use_polling_log_file"));
  } else if (polling_log_stderr) {
    r &= Hub::get()->use_polling_log_stderr(
      all->create_callback("@atom/watcher:binding.configure.use_polling_log_stderr"));
  } else if (polling_log_stdout) {
    r &= Hub::get()->use_polling_log_stdout(
      all->create_callback("@atom/watcher:binding.configure.use_polling_log_stdout"));
  }

  if (polling_interval > 0) {
    r &= Hub::get()->set_polling_interval(
      polling_interval, all->create_callback("@atom/watcher:binding.configure.set_polling_interval"));
  }

  if (polling_throttle > 0) {
    r &= Hub::get()->set_polling_throttle(
      polling_throttle, all->create_callback("@atom/watcher:binding.configure.set_polling_throttle"));
  }

  all->set_result(move(r));
  all->fire_if_empty(true);
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
  bool recursive = true;
  if (!get_bool_option(options, "poll", poll)) return;
  if (!get_bool_option(options, "recursive", recursive)) return;

  unique_ptr<AsyncCallback> ack_callback(new AsyncCallback("@atom/watcher:binding.watch.ack", info[2].As<Function>()));
  unique_ptr<AsyncCallback> event_callback(
    new AsyncCallback("@atom/watcher:binding.watch.event", info[3].As<Function>()));

  Result<> r = Hub::get()->watch(move(root_str), poll, recursive, move(ack_callback), move(event_callback));
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
  auto channel_id = static_cast<ChannelID>(maybe_channel_id.FromJust());

  unique_ptr<AsyncCallback> ack_callback(new AsyncCallback("@atom/watcher:binding.unwatch", info[1].As<Function>()));

  Result<> r = Hub::get()->unwatch(channel_id, move(ack_callback));
  if (r.is_error()) {
    Nan::ThrowError(r.get_error().c_str());
  }
}

void status(const Nan::FunctionCallbackInfo<Value> &info)
{
  unique_ptr<AsyncCallback> callback(new AsyncCallback("@atom/watcher:binding.status", info[0].As<Function>()));
  Hub::get()->status(move(callback));
}

void initialize(Local<Object> exports)
{
  Logger::from_env("WATCHER_LOG_MAIN");

  LOGGER << "Initializing module" << endl;

  Nan::Set(exports,
    Nan::New<String>("configure").ToLocalChecked(),
    Nan::GetFunction(Nan::New<FunctionTemplate>(configure)).ToLocalChecked());
  Nan::Set(exports,
    Nan::New<String>("watch").ToLocalChecked(),
    Nan::GetFunction(Nan::New<FunctionTemplate>(watch)).ToLocalChecked());
  Nan::Set(exports,
    Nan::New<String>("unwatch").ToLocalChecked(),
    Nan::GetFunction(Nan::New<FunctionTemplate>(unwatch)).ToLocalChecked());
  Nan::Set(exports,
    Nan::New<String>("status").ToLocalChecked(),
    Nan::GetFunction(Nan::New<FunctionTemplate>(status)).ToLocalChecked());
}

NODE_MODULE(watcher, initialize);  // NOLINT
