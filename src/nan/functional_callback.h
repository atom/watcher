#ifndef FUNCTIONAL_CALLBACK_H
#define FUNCTIONAL_CALLBACK_H

#include <functional>
#include <memory>
#include <nan.h>
#include <v8.h>

#include "async_callback.h"

using FnCallback = std::function<void(const Nan::FunctionCallbackInfo<v8::Value> &)>;

std::unique_ptr<AsyncCallback> fn_callback(const char *async_name, FnCallback &fn);

std::unique_ptr<AsyncCallback> noop_callback();

#endif
