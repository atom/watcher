#include <list>
#include <memory>
#include <nan.h>
#include <utility>
#include <v8.h>
#include <vector>

#include "all_callback.h"
#include "async_callback.h"
#include "functional_callback.h"

using Nan::FunctionCallback;
using Nan::FunctionCallbackInfo;
using Nan::HandleScope;
using std::bind;
using std::list;
using std::move;
using std::shared_ptr;
using std::unique_ptr;
using std::placeholders::_1;
using v8::Array;
using v8::Local;
using v8::Value;

list<shared_ptr<AllCallback>> AllCallback::retained;

shared_ptr<AllCallback> AllCallback::create(unique_ptr<AsyncCallback> &&done)
{
  shared_ptr<AllCallback> created(new AllCallback(move(done)));
  retained.emplace_front(created);
  retained.front()->me = retained.begin();
  return retained.front();
}

AllCallback::AllCallback(unique_ptr<AsyncCallback> &&done) :
  done(move(done)),
  fired{false},
  total{0},
  remaining{0},
  error(Nan::Undefined()),
  results(Nan::New<Array>(0)),
  me{retained.end()}
{
  //
}

unique_ptr<AsyncCallback> AllCallback::create_callback(const char *async_name)
{
  size_t index = total;
  functions.emplace_front(bind(&AllCallback::callback_complete, this, index, _1));

  total++;
  remaining++;

  return fn_callback(async_name, functions.front());
}

void AllCallback::set_result(Result<> &&r)
{
  if (r.is_ok()) return;

  if (Nan::New(error)->IsUndefined()) {
    HandleScope scope;
    Local<Value> l_error = Nan::Error(r.get_error().c_str());

    error.Reset(l_error);
  }
}

void AllCallback::fire_if_empty(bool sync)
{
  if (remaining > 0) return;
  if (fired) return;
  fired = true;

  HandleScope scope;
  Local<Value> l_error = Nan::New(error);
  Local<Array> l_results = Nan::New(results);

  Local<Value> argv[] = {l_error, l_results};
  if (sync) {
    done->SyncCall(2, argv);
  } else {
    done->Call(2, argv);
  }

  retained.erase(me);
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

  fire_if_empty(false);
}
