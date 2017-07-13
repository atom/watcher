#ifndef PLATFORM_H
#define PLATFORM_H

#include <string>

#include "../message.h"

class WorkerThread;

class WorkerPlatform {
public:
  static WorkerPlatform *for_worker(WorkerThread *thread);

  virtual ~WorkerPlatform() {};

  virtual void wake() = 0;

  virtual void listen() = 0;
  virtual void handle_add_command(const std::string &root_path) = 0;
  virtual void handle_remove_command(const std::string &root_path) = 0;
};

#endif
