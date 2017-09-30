#ifndef OPTIONS_H
#define OPTIONS_H

#include <string>
#include <v8.h>

bool get_string_option(v8::Local<v8::Object>& options, const char *key_name, std::string &out);

bool get_bool_option(v8::Local<v8::Object>& options, const char *key_name, bool &out);

#endif
