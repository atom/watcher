#include <vector>
#include <forward_list>
#include <utility>
#include <memory>
#include <nan.h>
#include <v8.h>

#include "functional_callback.h"
#include "all_callback.h"

using Nan::Callback;
using Nan::FunctionCallback;
using Nan::FunctionCallbackInfo;
using Nan::HandleScope;
using v8::Local;
using v8::Value;
using v8::Array;
using std::bind;
using std::move;
using std::forward_list;
using std::unique_ptr;
using std::placeholders::_1;

forward_list<AllCallback> AllCallback::retained;

AllCallback &AllCallback::create(unique_ptr<Callback> &&done)
{
  auto previous_begin = retained.begin();
  retained.emplace_front(move(done), internal());
  if (previous_begin != retained.end()) {
    previous_begin->before_it = retained.begin();
  }
  return retained.front();
}

AllCallback::AllCallback(unique_ptr<Callback> &&done, const internal &key) :
  done(move(done)),
  fired{false},
  total{0},
  remaining{0},
  error(Nan::Undefined()),
  results(Nan::New<Array>(0)),
  before_it{retained.before_begin()}
{
  //
}

unique_ptr<Callback> AllCallback::create_callback()
{
  size_t index = total;
  functions.emplace_front(bind(&AllCallback::callback_complete, this, index, _1));

  total++;
  remaining++;

  return fn_callback(functions.front());
}

void AllCallback::fire_if_empty()
{
  if (remaining > 0) return;
  if (fired) return;
  fired = true;

  HandleScope scope;
  Local<Value> l_error = Nan::New(error);
  Local<Array> l_results = Nan::New(results);

  Local<Value> argv[] = {l_error, l_results};
  done->Call(2, argv);

  retained.erase_after(before_it);
}

void AllCallback::callback_complete(size_t callback_index, const FunctionCallbackInfo<Value> &info)
{
  Local<Value> err = info[0];

  if (!err->IsNull() && !err->IsUndefined()) {
    if (Nan::New(error)->IsUndefined()) {
      error.Reset(err);
    }
  }

  Local<Array> rest = Nan::New<Array>(info.Length() - 1);
  for (int i = 1; i < info.Length(); i++) {
    Nan::Set(rest, i - 1, info[i]);
  }

  Local<Array> l_results = Nan::New(results);
  Nan::Set(l_results, callback_index, rest);

  remaining--;

  fire_if_empty();
}
