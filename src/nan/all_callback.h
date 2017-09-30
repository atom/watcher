#ifndef ALL_CALLBACK_H
#define ALL_CALLBACK_H

#include <functional>
#include <vector>
#include <memory>
#include <nan.h>
#include <v8.h>

#include "functional_callback.h"

class AllCallback {
public:
  explicit AllCallback(std::unique_ptr<Nan::Callback> done);
  AllCallback(const AllCallback &) = delete;
  AllCallback(AllCallback &&) = delete;
  ~AllCallback();

  AllCallback &operator=(const AllCallback &) = delete;
  AllCallback &operator=(AllCallback &&) = delete;

  std::unique_ptr<Nan::Callback> create_callback();

  void fire_if_empty();
private:
  void callback_complete(size_t callback_index, const Nan::FunctionCallbackInfo<v8::Value> &info);

  std::unique_ptr<Nan::Callback> done;
  size_t remaining;

  std::vector<FnCallback> functions;

  Nan::Persistent<v8::Value> error;
  Nan::Persistent<v8::Array> results;
};

#endif
