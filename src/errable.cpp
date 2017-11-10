#include <string>
#include <utility>
#include <uv.h>

#include "errable.h"
#include "result.h"

using std::move;
using std::string;

Result<> Errable::health_err_result() const
{
  if (message.empty()) return ok_result();
  return error_result(string(message));
}

void Errable::report_errable(const Errable &component)
{
  report_if_error(component.health_err_result());
}

void Errable::report_uv_error(int err_code)
{
  report_error(uv_strerror(err_code));
}

void Errable::report_error(string &&message)
{
  assert(!frozen);
  this->message = move(message);
}
