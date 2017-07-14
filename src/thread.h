#ifndef THREAD_H
#define THREAD_H

#include <memory>
#include <functional>
#include <vector>
#include <uv.h>

#include "errable.h"
#include "queue.h"
#include "message.h"

void thread_callback_helper(void *arg);

class Thread : Errable {
public:
  template< class T >
  Thread(T* self, void (T::*fn)()) :
    work_fn{std::bind(std::mem_fn(fn), self)}
  {
    //
  };

  void run();

  void send(Message &&message);

  template <class InputIt>
  void send_all(InputIt begin, InputIt end)
  {
    if (!is_healthy()) return;
    in.enqueue_all(begin, end);
    wake();
  }

  std::unique_ptr<std::vector<Message>> receive_all();

protected:
  virtual void wake();

  void emit(Message &&message);

  template <class InputIt>
  void emit_all(InputIt begin, InputIt end)
  {
    if (!is_healthy()) return;
    out.enqueue_all(begin, end);
    wake();
  }

  std::unique_ptr<std::vector<Message>> process_all();

private:
  Queue in;
  Queue out;

  uv_thread_t uv_handle;
  std::function<void()> work_fn;
};

#endif
