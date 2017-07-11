#include <string>
#include <utility>
#include <uv.h>

#include "errable.h"

using std::string;
using std::move;

Errable::Errable() : healthy{true}, message{"ok"}
{
  //
}

bool Errable::is_healthy()
{
  return healthy;
}

void Errable::report_error(string &&message)
{
  healthy = false;
  this->message = move(message);
}

bool Errable::report_uv_error(int errCode)
{
  if (!errCode) {
    return false;
  }

  report_error(uv_strerror(errCode));
  return true;
}

string Errable::get_error() {
  return message;
}
