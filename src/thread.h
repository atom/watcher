#ifndef THREAD_H
#define THREAD_H

#include <string>
#include <memory>
#include <utility>
#include <functional>
#include <vector>
#include <uv.h>

#include "errable.h"
#include "result.h"
#include "queue.h"
#include "message.h"
#include "status.h"

void thread_callback_helper(void *arg);

class Thread : public SyncErrable {
public:
  template< class T >
  Thread(T* self, void (T::*fn)(), std::string name, uv_async_t *main_callback) :
    SyncErrable(name),
    in(name + " input queue"),
    out(name + " output queue"),
    main_callback{main_callback},
    work_fn{std::bind(std::mem_fn(fn), self)}
  {
    //
  };

  Result<> run();

  Result<> send(Message &&message);

  template <class InputIt>
  Result<> send_all(InputIt begin, InputIt end)
  {
    if (!is_healthy()) return health_err_result();

    Result<> qr = in.enqueue_all(begin, end);
    if (qr.is_error()) return qr;

    Result<> wr = wake();
    if (wr.is_error()) return wr;

    return ok_result();
  }

  Result< std::unique_ptr<std::vector<Message>> > receive_all();

  virtual void collect_status(Status &status) = 0;

protected:
  virtual Result<> wake();

  Result<> emit(Message &&message);

  template <class InputIt>
  Result<> emit_all(InputIt begin, InputIt end)
  {
    if (!is_healthy()) return health_err_result();

    Result<> qr = out.enqueue_all(begin, end);
    if (qr.is_error()) return qr;

    int uv_err = uv_async_send(main_callback);
    if (uv_err) {
      return error_result(uv_strerror(uv_err));
    }

    return ok_result();
  }

  Result< std::unique_ptr<std::vector<Message>> > process_all();

  std::string get_in_queue_error();
  size_t get_in_queue_size();
  std::string get_out_queue_error();
  size_t get_out_queue_size();

private:
  Queue in;
  Queue out;

  uv_async_t *main_callback;

  uv_thread_t uv_handle;
  std::function<void()> work_fn;
};

#endif
