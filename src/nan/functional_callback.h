#ifndef FUNCTIONAL_CALLBACK_H
#define FUNCTIONAL_CALLBACK_H

#include <functional>
#include <memory>
#include <nan.h>
#include <v8.h>

typedef std::function<void(const Nan::FunctionCallbackInfo<v8::Value>&)> FnCallback;

std::unique_ptr<Nan::Callback> fn_callback(FnCallback &fn);

#endif
