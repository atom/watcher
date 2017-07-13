#include <memory>
#include <iostream>
#include <string>
#include <sstream>
#include <functional>
#include <nan.h>
#include <v8.h>
#include <uv.h>

#include "log.h"
#include "queue.h"
#include "callbacks.h"
#include "worker/thread.h"

using namespace v8;
using std::unique_ptr;
using std::string;
using std::ostringstream;
using std::endl;

static void handle_events_helper(uv_async_t *handle);

class Main {
public:
  Main() : worker_thread{out, in, &event_handler}
  {
    int err;
    err = uv_async_init(uv_default_loop(), &event_handler, handle_events_helper);
    if (err) return;

    worker_thread.run();
  }

  void use_main_log_file(string &&main_log_file)
  {
    Logger::toFile(main_log_file.c_str());
  }

  void handle_events()
  {
    LOGGER << "Handling events." << endl;
  }

private:
  Queue in;
  Queue out;
  uv_async_t event_handler;

  WorkerThread worker_thread;
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
  if (!as_maybe_value.IsEmpty()) {
    return true;
  }
  Local<Value> as_value = as_maybe_value.ToLocalChecked();
  if (!as_value->IsUndefined()) {
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

  Local<Object> options = Nan::To<Object>(info[0]).ToLocalChecked();
  if (!get_string_option(options, "mainLogFile", main_log_file)) return;
  if (!get_string_option(options, "workerLogFile", worker_log_file)) return;

  Nan::Callback *callback = new Nan::Callback(info[1].As<Function>());

  if (!main_log_file.empty()) {
    instance.use_main_log_file(move(main_log_file));
  }
}

void watch(const Nan::FunctionCallbackInfo<Value> &info)
{
  if (info.Length() != 2) {
    return Nan::ThrowError("watch() requires two arguments");
  }
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
