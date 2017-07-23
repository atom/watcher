#include <string>

#include "../worker_platform.h"
#include "../../message.h"
#include "../../log.h"

using std::string;

class WindowsWorkerPlatform : public WorkerPlatform {
public:
  void wake() override
  {
    //
  }

  void listen() override
  {
    //
  }

  void handle_add_command(const ChannelID channel, const string &root_path)
  {
    //
  }

  void handle_remove_command(const ChannelID channel)
  {
    //
  }
}
