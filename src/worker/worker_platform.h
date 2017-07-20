#ifndef WORKER_PLATFORM_H
#define WORKER_PLATFORM_H

#include <memory>
#include <utility>
#include <string>

#include "worker_thread.h"
#include "../message.h"

class WorkerPlatform {
public:
  static std::unique_ptr<WorkerPlatform> for_worker(WorkerThread *thread);

  virtual ~WorkerPlatform() {};

  virtual void wake() = 0;

  virtual void listen() = 0;
  virtual void handle_add_command(const ChannelID channel, const std::string &root_path) = 0;
  virtual void handle_remove_command(const ChannelID channel) = 0;

  void handle_commands()
  {
    thread->handle_commands();
  }

protected:
  WorkerPlatform(WorkerThread *thread) : thread{thread}
  {
    //
  }

  void emit(Message &&message)
  {
    thread->emit(std::move(message));
  }

  template <class InputIt>
  void emit_all(InputIt begin, InputIt end)
  {
    thread->emit_all(begin, end);
  }

  WorkerThread *thread;
};

#endif
