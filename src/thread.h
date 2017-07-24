#ifndef THREAD_H
#define THREAD_H

#include <string>
#include <memory>
#include <functional>
#include <vector>
#include <uv.h>

#include "errable.h"
#include "queue.h"
#include "message.h"
#include "status.h"

void thread_callback_helper(void *arg);

class Thread : public SyncErrable {
public:
  template< class T >
  Thread(T* self, void (T::*fn)(), uv_async_t *main_callback) :
    main_callback{main_callback},
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

  virtual void collect_status(Status &status) = 0;

protected:
  virtual void wake();

  void emit(Message &&message);

  template <class InputIt>
  void emit_all(InputIt begin, InputIt end)
  {
    if (!is_healthy()) return;
    out.enqueue_all(begin, end);
    uv_async_send(main_callback);
  }

  std::unique_ptr<std::vector<Message>> process_all();

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
