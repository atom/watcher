#ifndef SUBSCRIPTION_H
#define SUBSCRIPTION_H

#include <string>
#include <sstream>
#include <memory>

#include "../../result.h"
#include "../../message.h"

class WindowsWorkerPlatform;

class Subscription {
public:
  Subscription(
    ChannelID channel,
    HANDLE root,
    const std::wstring &path,
    WindowsWorkerPlatform *platform
  );

  ~Subscription();

  Result<> schedule(LPOVERLAPPED_COMPLETION_ROUTINE fn);

  Result<> use_network_size();

  BYTE *get_written(DWORD written_size);

  std::wstring make_absolute(const std::wstring &sub_path);

  Result<> stop(const CommandID command);

  CommandID get_command_id() const
  {
    return command;
  }

  ChannelID get_channel() const
  {
    return channel;
  }

  WindowsWorkerPlatform* get_platform() const
  {
    return platform;
  }

private:
  CommandID command;
  ChannelID channel;
  WindowsWorkerPlatform *platform;

  std::wstring path;
  HANDLE root;
  OVERLAPPED overlapped;
  bool terminating;

  DWORD buffer_size;
  std::unique_ptr<BYTE[]> buffer;
  std::unique_ptr<BYTE[]> written;
};

#endif
