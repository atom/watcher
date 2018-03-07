#include <functional>
#include <memory>
#include <nan.h>
#include <v8.h>

#include "async_callback.h"
#include "functional_callback.h"

using Nan::FunctionCallback;
using Nan::FunctionCallbackInfo;
using std::unique_ptr;
using v8::ArrayBuffer;
using v8::Function;
using v8::Isolate;
using v8::Local;
using v8::Value;
using Contents = v8::ArrayBuffer::Contents;

void _noop_callback_helper(const FunctionCallbackInfo<Value> & /*info*/)
{
  // Do nothing
}

void _fn_callback_helper(const FunctionCallbackInfo<Value> &info)
{
  Local<ArrayBuffer> cb_array = info.Data().As<ArrayBuffer>();
  Contents cb_contents = cb_array->GetContents();

  auto *payload = static_cast<intptr_t *>(cb_contents.Data());
  assert(cb_contents.ByteLength() == sizeof(FnCallback *));

  auto *fn = reinterpret_cast<FnCallback *>(*payload);

  delete payload;
  (*fn)(info);
}

unique_ptr<AsyncCallback> fn_callback(const char *async_name, FnCallback &fn)
{
  Nan::HandleScope scope;

  auto *payload = new intptr_t(reinterpret_cast<intptr_t>(&fn));

  Local<ArrayBuffer> fn_addr =
    ArrayBuffer::New(Isolate::GetCurrent(), static_cast<void *>(payload), sizeof(FnCallback *));
  Local<Function> wrapper = Nan::New<Function>(_fn_callback_helper, fn_addr);
  return unique_ptr<AsyncCallback>(new AsyncCallback(async_name, wrapper));
}

unique_ptr<AsyncCallback> noop_callback()
{
  Nan::HandleScope scope;

  Local<Function> wrapper = Nan::New<Function>(_noop_callback_helper);
  return unique_ptr<AsyncCallback>(new AsyncCallback("@atom/watcher:noop", wrapper));
}
