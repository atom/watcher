#ifndef THREAD_STARTER_H
#define THREAD_STARTER_H

#include <memory>
#include <vector>

#include "message.h"

class ThreadStarter
{
public:
  ThreadStarter() = default;
  ThreadStarter(const ThreadStarter &) = delete;
  ThreadStarter(ThreadStarter &&) = delete;
  ~ThreadStarter() = default;

  ThreadStarter &operator=(const ThreadStarter &) = delete;
  ThreadStarter &operator=(ThreadStarter &&) = delete;

  std::vector<Message> get_messages();

  void set_logging(const CommandPayload *payload) { set_command(logging, payload); }

protected:
  void set_command(std::unique_ptr<CommandPayload> &dest, const CommandPayload *src);

  Message wrap_command(std::unique_ptr<CommandPayload> &src);

private:
  std::unique_ptr<CommandPayload> logging;
};

#endif
