#ifndef ASYNC_CALLBACK_H
#define ASYNC_CALLBACK_H

#include <nan.h>
#include <v8.h>

// Wrap a v8::Function with an AsyncResource so that it fires async_hooks correctly.
class AsyncCallback : public Nan::AsyncResource
{
public:
  AsyncCallback(const char *name, v8::Local<v8::Function> fn);
  ~AsyncCallback();

  v8::MaybeLocal<v8::Value> Call(int argc, v8::Local<v8::Value> *argv);

  v8::MaybeLocal<v8::Value> SyncCall(int argc, v8::Local<v8::Value> *argv);

  AsyncCallback(const AsyncCallback &) = delete;
  AsyncCallback(AsyncCallback &&) = delete;
  AsyncCallback &operator=(const AsyncCallback &) = delete;
  AsyncCallback &operator=(AsyncCallback &&original) = delete;

private:
  Nan::Persistent<v8::Function> fn;
};

#endif
