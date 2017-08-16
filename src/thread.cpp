#include <string>
#include <memory>
#include <functional>
#include <vector>
#include <utility>
#include <uv.h>

#include "thread.h"
#include "message.h"

using std::string;
using std::function;
using std::unique_ptr;
using std::vector;
using std::move;

void thread_callback_helper(void *arg)
{
  function<void()> *bound_fn = static_cast<std::function<void()>*>(arg);
  (*bound_fn)();
}

Result<> Thread::run()
{
  int err;

  err = uv_thread_create(&uv_handle, thread_callback_helper, &work_fn);
  if (err) {
    report_uv_error(err);
    return health_err_result();
  } else {
    return ok_result();
  }
}

Result<> Thread::send(Message &&message)
{
  if (!is_healthy()) return health_err_result();

  Result<> qr = in.enqueue(move(message));
  if (qr.is_error()) return qr;

  Result<> wr = wake();
  if (wr.is_error()) return wr;

  return ok_result();
}

Result< unique_ptr<vector<Message>> > Thread::receive_all()
{
  if (!is_healthy()) return health_err_result< unique_ptr<vector<Message>> >();

  return out.accept_all();
}

Result<> Thread::emit(Message &&message)
{
  if (!is_healthy()) return health_err_result();

  Result<> qr = out.enqueue(move(message));
  if (qr.is_error()) return qr;

  int uv_err = uv_async_send(main_callback);
  if (uv_err) {
    return error_result(uv_strerror(uv_err));
  }

  return ok_result();
}

Result< unique_ptr<vector<Message>> > Thread::process_all()
{
  if (!is_healthy()) return health_err_result< unique_ptr<vector<Message>> >();

  return in.accept_all();
}

Result<> Thread::wake()
{
  return ok_result();
}

string Thread::get_in_queue_error()
{
  return in.get_error();
}

size_t Thread::get_in_queue_size()
{
  return in.size();
}

string Thread::get_out_queue_error()
{
  return out.get_error();
}

size_t Thread::get_out_queue_size()
{
  return out.size();
}
