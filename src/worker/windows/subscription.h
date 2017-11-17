#ifndef SUBSCRIPTION_H
#define SUBSCRIPTION_H

#include <memory>
#include <sstream>
#include <string>
#include <utility>

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

  void remember_old_path(std::string &&old_path, EntryKind kind)
  {
    this->old_path = std::move(old_path);
    this->old_path_kind = kind;
    this->old_path_seen = true;
  }

  void clear_old_path()
  {
    old_path.clear();
    old_path_kind = KIND_UNKNOWN;
    old_path_seen = false;
  }

  const std::string &get_old_path() const { return old_path; }

  const EntryKind &get_old_path_kind() const { return old_path_kind; }

  const bool &was_old_path_seen() const { return old_path_seen; }

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

  std::string old_path;
  EntryKind old_path_kind;
  bool old_path_seen;
};

#endif
