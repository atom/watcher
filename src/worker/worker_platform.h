#ifndef WORKER_PLATFORM_H
#define WORKER_PLATFORM_H

#include <memory>
#include <string>
#include <utility>

#include "../errable.h"
#include "../message.h"
#include "../result.h"
#include "../status.h"
#include "worker_thread.h"

class WorkerPlatform : public Errable
{
public:
  static std::unique_ptr<WorkerPlatform> for_worker(WorkerThread *thread);

  WorkerPlatform() : Errable("platform"){};

  ~WorkerPlatform() override = default;

  virtual Result<> wake() = 0;

  virtual Result<> listen() = 0;

  virtual Result<bool> handle_add_command(CommandID command,
    ChannelID channel,
    const std::string &root_path,
    bool recursive) = 0;

  virtual Result<bool> handle_remove_command(CommandID command, ChannelID channel) = 0;

  virtual void populate_status(Status &status) {}

  Result<> handle_commands()
  {
    if (!is_healthy()) return health_err_result();

    return thread->handle_commands().propagate_as_void();
  }

  WorkerPlatform(const WorkerPlatform &) = delete;
  WorkerPlatform(WorkerPlatform &&) = delete;
  WorkerPlatform &operator=(const WorkerPlatform &) = delete;
  WorkerPlatform &operator=(WorkerPlatform &&) = delete;

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

  WorkerThread *thread{};
};

#endif
