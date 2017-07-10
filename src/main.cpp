#include <nan.h>

namespace sfw {

using Nan::New;
using Nan::Set;
using Nan::GetFunction;
using v8::String;
using v8::FunctionTemplate;

NAN_METHOD(Ok) {
  info.GetReturnValue().Set(New<String>("whatever").ToLocalChecked());
}

NAN_MODULE_INIT(Initialize) {
  Set(target, New<String>("ok").ToLocalChecked(),
    GetFunction(New<FunctionTemplate>(Ok)).ToLocalChecked());
}

NODE_MODULE(sfw, Initialize)

} // namespace sfw
