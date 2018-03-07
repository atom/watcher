#include <nan.h>
#include <v8.h>

#include "async_callback.h"

using Nan::AsyncResource;
using Nan::HandleScope;
using Nan::New;
using v8::Function;
using v8::Local;
using v8::MaybeLocal;
using v8::Object;
using v8::Value;

AsyncCallback::AsyncCallback(const char *name, Local<Function> fn) : AsyncResource(name), fn(fn)
{
  //
}

AsyncCallback::~AsyncCallback()
{
  fn.Reset();
}

MaybeLocal<Value> AsyncCallback::Call(int argc, Local<Value> *argv)
{
  HandleScope scope;
  Local<Function> localFn = New(fn);
  Local<Object> target = New<Object>();

  return runInAsyncScope(target, localFn, argc, argv);
}

MaybeLocal<Value> AsyncCallback::SyncCall(int argc, Local<Value> *argv)
{
  HandleScope scope;
  Local<Function> localFn = New(fn);
  Local<Object> target = New<Object>();

  return Nan::Call(localFn, target, argc, argv);
}
