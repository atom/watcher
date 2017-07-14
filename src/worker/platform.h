#ifndef PLATFORM_H
#define PLATFORM_H

#include <memory>
#include <string>

#include "worker_thread.h"
#include "../message.h"

class WorkerPlatform {
public:
  static std::unique_ptr<WorkerPlatform> for_worker(WorkerThread *thread);

  virtual ~WorkerPlatform() {};

  virtual void wake() = 0;

  virtual void listen() = 0;
  virtual void handle_add_command(const std::string &root_path) = 0;
  virtual void handle_remove_command(const std::string &root_path) = 0;

  void handle_commands()
  {
    thread->handle_commands();
  }

protected:
  WorkerPlatform(WorkerThread *thread) : thread{thread}
  {
    //
  }

  WorkerThread *thread;
};

#endif
