#ifndef ALL_CALLBACK_H
#define ALL_CALLBACK_H

#include <forward_list>
#include <functional>
#include <list>
#include <memory>
#include <nan.h>
#include <v8.h>

#include "../result.h"
#include "async_callback.h"
#include "functional_callback.h"

class AllCallback
{
public:
  static std::shared_ptr<AllCallback> create(std::unique_ptr<AsyncCallback> &&done);

  ~AllCallback() = default;

  std::unique_ptr<AsyncCallback> create_callback(const char *async_name);

  void set_result(Result<> &&r);

  void fire_if_empty(bool sync);

  AllCallback(const AllCallback &) = delete;
  AllCallback(AllCallback &&) = delete;
  AllCallback &operator=(const AllCallback &) = delete;
  AllCallback &operator=(AllCallback &&) = delete;

private:
  explicit AllCallback(std::unique_ptr<AsyncCallback> &&done);

  void callback_complete(size_t callback_index, const Nan::FunctionCallbackInfo<v8::Value> &info);

  std::unique_ptr<AsyncCallback> done;
  bool fired;
  size_t total;
  size_t remaining;

  std::forward_list<FnCallback> functions;

  Nan::Persistent<v8::Value> error;
  Nan::Persistent<v8::Array> results;

  std::list<std::shared_ptr<AllCallback>>::iterator me;

  static std::list<std::shared_ptr<AllCallback>> retained;
};

#endif
