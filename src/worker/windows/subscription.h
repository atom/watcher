#ifndef SUBSCRIPTION_H
#define SUBSCRIPTION_H

#include <memory>
#include <sstream>
#include <string>

#include "../../message.h"
#include "../../result.h"

class WindowsWorkerPlatform;

class Subscription
{
public:
  Subscription(ChannelID channel,
    HANDLE root,
    const std::wstring &path,
    bool recursive,
    WindowsWorkerPlatform *platform);

  ~Subscription();

  Result<bool> schedule(LPOVERLAPPED_COMPLETION_ROUTINE fn);

  Result<> use_network_size();

  BYTE *get_written(DWORD written_size);

  Result<std::string> get_root_path();

  std::wstring make_absolute(const std::wstring &sub_path);

  Result<> stop(const CommandID command);

  const CommandID &get_command_id() const { return command; }

  const ChannelID &get_channel() const { return channel; }

  WindowsWorkerPlatform *get_platform() const { return platform; }

  const bool &is_recursive() const { return recursive; }

  const bool &is_terminating() const { return terminating; }

private:
  CommandID command;
  ChannelID channel;
  WindowsWorkerPlatform *platform;

  std::wstring path;
  HANDLE root;
  OVERLAPPED overlapped;
  bool recursive;
  bool terminating;

  DWORD buffer_size;
  std::unique_ptr<BYTE[]> buffer;
  std::unique_ptr<BYTE[]> written;
};

#endif
