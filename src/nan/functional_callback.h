#ifndef FUNCTIONAL_CALLBACK_H
#define FUNCTIONAL_CALLBACK_H

#include <functional>
#include <memory>
#include <nan.h>
#include <v8.h>

using FnCallback = std::function<void(const Nan::FunctionCallbackInfo<v8::Value> &)>;

std::unique_ptr<Nan::Callback> fn_callback(FnCallback &fn);

std::unique_ptr<Nan::Callback> noop_callback();

#endif
