#ifndef WORKER_PLATFORM_H
#define WORKER_PLATFORM_H

#include <memory>
#include <utility>
#include <string>

#include "worker_thread.h"
#include "../message.h"
#include "../errable.h"
#include "../result.h"

class WorkerPlatform : public Errable {
public:
  static std::unique_ptr<WorkerPlatform> for_worker(WorkerThread *thread);

  WorkerPlatform() : Errable("platform") {};
  virtual ~WorkerPlatform() {};

  virtual Result<> wake() = 0;

  virtual Result<> listen() = 0;
  virtual Result<bool> handle_add_command(
    const CommandID command,
    const ChannelID channel,
    const std::string &root_path) = 0;
  virtual Result<bool> handle_remove_command(
    const CommandID command,
    const ChannelID channel) = 0;

  Result<> handle_commands()
  {
    if (!is_healthy()) return health_err_result();

    return thread->handle_commands();
  }

protected:
  WorkerPlatform(WorkerThread *thread) : Errable("platform"), thread{thread}
  {
    //
  }

  Result<> emit(Message &&message)
  {
    if (!is_healthy()) return health_err_result();

    return thread->emit(std::move(message));
  }

  template <class InputIt>
  Result<> emit_all(InputIt begin, InputIt end)
  {
    if (!is_healthy()) return health_err_result();

    return thread->emit_all(begin, end);
  }

  WorkerThread *thread;
};

#endif
