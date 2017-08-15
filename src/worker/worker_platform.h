#ifndef WORKER_PLATFORM_H
#define WORKER_PLATFORM_H

#include <memory>
#include <utility>
#include <string>

#include "worker_thread.h"
#include "../message.h"
#include "../result.h"

class WorkerPlatform {
public:
  static std::unique_ptr<WorkerPlatform> for_worker(WorkerThread *thread);

  virtual ~WorkerPlatform() {};

  virtual Result<> &&wake() = 0;

  virtual Result<> &&listen() = 0;
  virtual Result<> &&handle_add_command(const ChannelID channel, const std::string &root_path) = 0;
  virtual Result<> &&handle_remove_command(const ChannelID channel) = 0;

  Result<> &&handle_commands()
  {
    return thread->handle_commands();
  }

protected:
  WorkerPlatform(WorkerThread *thread) : thread{thread}
  {
    //
  }

  Result<> &&emit(Message &&message)
  {
    return thread->emit(std::move(message));
  }

  template <class InputIt>
  Result<> &&emit_all(InputIt begin, InputIt end)
  {
    return thread->emit_all(begin, end);
  }

  WorkerThread *thread;
};

#endif
