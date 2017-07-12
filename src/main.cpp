#include <iostream>
#include <string>
#include <nan.h>
#include <v8.h>
#include <uv.h>

#include "log.h"
#include "queue.h"
#include "worker/thread.h"

using namespace v8;
using std::string;
using std::endl;

class Main {
public:
  Main() : workerThread{out, in}
  {
    //
  }

  void mainLogFile(string &&mainLogFile)
  {
    Logger::toFile(mainLogFile.c_str());
  }

  void handleEvents()
  {
    //
  }

private:
  Queue in;
  Queue out;

  WorkerThread workerThread;
};

static Main instance;

static Nan::Persistent<String> mainLogFileKey;

void configure(const Nan::FunctionCallbackInfo<Value> &info)
{
  if (info.Length() != 1) {
    return Nan::ThrowError("configure() requires one argument");
  }

  Nan::MaybeLocal<Object> maybeConfObject = Nan::To<Object>(info[0]);
  if (maybeConfObject.IsEmpty()) {
    return Nan::ThrowError("configure() requires an option object");
  }
  Local<Object> conf = maybeConfObject.ToLocalChecked();

  Nan::MaybeLocal<Value> maybeMainLogFile = Nan::Get(conf, Nan::New(mainLogFileKey));
  if (!maybeMainLogFile.IsEmpty()) {
    Nan::MaybeLocal<String> maybeMainLogFileString = Nan::To<String>(maybeMainLogFile.ToLocalChecked());
    if (maybeMainLogFileString.IsEmpty()) {
      return Nan::ThrowError("options.mainLogFile must be a String");
    }
    Local<String> mainLogFileString = maybeMainLogFileString.ToLocalChecked();
    instance.mainLogFile(*String::Utf8Value(mainLogFileString));
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
  mainLogFileKey.Reset(Nan::New<String>("mainLogFile").ToLocalChecked());

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
