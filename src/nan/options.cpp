#include <string>
#include <sstream>
#include <v8.h>
#include <nan.h>

#include "options.h"

using v8::Local;
using v8::Value;
using v8::Object;
using v8::String;
using v8::Uint32;
using Nan::MaybeLocal;
using Nan::Maybe;
using std::string;
using std::ostringstream;

bool get_string_option(Local<Object>& options, const char *key_name, string &out)
{
  Nan::HandleScope scope;
  const Local<String> key = Nan::New<String>(key_name).ToLocalChecked();

  MaybeLocal<Value> as_maybe_value = Nan::Get(options, key);
  if (as_maybe_value.IsEmpty()) {
    return true;
  }
  Local<Value> as_value = as_maybe_value.ToLocalChecked();
  if (as_value->IsUndefined()) {
    return true;
  }

  if (!as_value->IsString()) {
    ostringstream message;
    message << "option " << key_name << " must be a String";
    Nan::ThrowError(message.str().c_str());
    return false;
  }

  Nan::Utf8String as_string(as_value);

  if (*as_string == nullptr) {
    ostringstream message;
    message << "option " << key_name << " must be a valid UTF-8 String";
    Nan::ThrowError(message.str().c_str());
    return false;
  }

  out.assign(*as_string, as_string.length());
  return true;
}

bool get_bool_option(Local<Object>& options, const char *key_name, bool &out)
{
  Nan::HandleScope scope;
  const Local<String> key = Nan::New<String>(key_name).ToLocalChecked();
  out = false;

  MaybeLocal<Value> as_maybe_value = Nan::Get(options, key);
  if (as_maybe_value.IsEmpty()) {
    return true;
  }
  Local<Value> as_value = as_maybe_value.ToLocalChecked();
  if (as_value->IsUndefined()) {
    return true;
  }

  if (!as_value->IsBoolean()) {
    ostringstream message;
    message << "configure() option " << key_name << " must be a Boolean";
    Nan::ThrowError(message.str().c_str());
    return false;
  }

  out = as_value->IsTrue();
  return true;
}

bool get_uint_option(v8::Local<v8::Object> &options, const char *key_name, uint_fast32_t &out)
{
  Nan::HandleScope scope;
  const Local<String> key = Nan::New<String>(key_name).ToLocalChecked();
  out = 0;

  MaybeLocal<Value> as_maybe_value = Nan::Get(options, key);
  if (as_maybe_value.IsEmpty()) {
    return true;
  }
  Local<Value> as_value = as_maybe_value.ToLocalChecked();
  if (as_value->IsUndefined()) {
    return true;
  }

  if (!as_value->IsUint32()) {
    ostringstream message;
    message << "configure() option " << key_name << " must be a non-negative integer";
    Nan::ThrowError(message.str().c_str());
    return false;
  }

  Maybe<uint32_t> as_maybe_uint = Nan::To<uint32_t>(as_value);
  if (as_maybe_uint.IsNothing()) {
    ostringstream message;
    message << "configure() option " << key_name << " must be a non-negative integer";
    Nan::ThrowError(message.str().c_str());
    return false;
  }

  out = as_maybe_uint.FromJust();
  return true;
}
