#include <functional>
#include <memory>
#include <nan.h>
#include <v8.h>

#include "functional_callback.h"

using std::unique_ptr;
using std::function;
using Nan::Callback;
using Nan::FunctionCallback;
using Nan::FunctionCallbackInfo;
using v8::Local;
using v8::String;
using v8::Value;
using v8::ArrayBuffer;
using v8::Array;
using v8::Function;
using v8::Isolate;
using Contents = v8::ArrayBuffer::Contents;

void _fn_callback_helper(const FunctionCallbackInfo<Value> &info)
{
  Local<ArrayBuffer> cb_array = info.Data().As<ArrayBuffer>();
  Contents cb_contents = cb_array->GetContents();

  void *fn_addr = cb_contents.Data();
  assert(cb_contents.ByteLength() == sizeof(FnCallback*));

  FnCallback *fn = static_cast<FnCallback*>(fn_addr);
  (*fn)(info);
}

unique_ptr<Callback> fn_callback(FnCallback &fn)
{
  Nan::HandleScope scope;

  Local<ArrayBuffer> fn_addr = ArrayBuffer::New(Isolate::GetCurrent(), static_cast<void*>(&fn), sizeof(void*));
  Local<Function> wrapper = Nan::New<Function>(_fn_callback_helper, fn_addr);
  return unique_ptr<Callback>(new Callback(wrapper));
}
