#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <uv.h>
#include <string>
#include <sstream>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "../worker_platform.h"
#include "../worker_thread.h"
#include "../../message.h"
#include "../../log.h"
#include "../../lock.h"

using std::string;
using std::wstring;
using std::ostringstream;
using std::wostringstream;
using std::unique_ptr;
using std::shared_ptr;
using std::default_delete;
using std::map;
using std::pair;
using std::make_pair;
using std::vector;
using std::move;
using std::endl;

const DWORD DEFAULT_BUFFER_SIZE = 1024 * 1024;
const DWORD NETWORK_BUFFER_SIZE = 64 * 1024;

static void CALLBACK command_perform_helper(__in ULONG_PTR payload);

static void CALLBACK event_helper(DWORD error_code, DWORD num_bytes, LPOVERLAPPED overlapped);

static Result<string> to_utf8(const wstring &in);

static Result<wstring> to_wchar(const string &in);

template < class V = void* >
static Result<V> windows_error_result(const string &prefix);

template < class V = void* >
static Result<V> windows_error_result(const string &prefix, DWORD error_code);

class WindowsWorkerPlatform;

class Subscription {
public:
  Subscription(
    ChannelID channel,
    HANDLE root,
    const wstring &path,
    WindowsWorkerPlatform *platform
  ) :
    channel{channel},
    platform{platform},
    path{path},
    root{root},
    buffer_size{DEFAULT_BUFFER_SIZE},
    buffer{new BYTE[buffer_size]},
    written{new BYTE[buffer_size]}
  {
    ZeroMemory(&overlapped, sizeof(OVERLAPPED));
    overlapped.hEvent = this;
  }

  ~Subscription()
  {
    CloseHandle(root);
  }

  Result<> schedule()
  {
    int success = ReadDirectoryChangesW(
      root, // root directory handle
      buffer.get(), // result buffer
      buffer_size, // result buffer size
      TRUE, // recursive
      FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_ATTRIBUTES
        | FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_LAST_ACCESS
        | FILE_NOTIFY_CHANGE_CREATION | FILE_NOTIFY_CHANGE_SECURITY, // change flags
      NULL, // bytes returned
      &overlapped, // overlapped
      &event_helper // completion routine
    );
    if (!success) {
      return windows_error_result<>("Unable to subscribe to filesystem events");
    }

    return ok_result();
  }

  Result<> use_network_size()
  {
    if (buffer_size <= NETWORK_BUFFER_SIZE) {
      ostringstream out("Buffer size of ");
      out
        << buffer_size
        << " is already lower than the network buffer size "
        << NETWORK_BUFFER_SIZE;
      return error_result(out.str());
    }

    buffer_size = NETWORK_BUFFER_SIZE;
    buffer.reset(new BYTE[buffer_size]);
    written.reset(new BYTE[buffer_size]);

    return ok_result();
  }

  ChannelID get_channel() const {
    return channel;
  }

  WindowsWorkerPlatform* get_platform() const {
    return platform;
  }

  BYTE *get_written(DWORD written_size) {
    memcpy(written.get(), buffer.get(), written_size);
    return written.get();
  }

  wstring make_absolute(const wstring &sub_path)
  {
    wostringstream out;

    out << path;
    if (path.back() != L'\\' && sub_path.front() != L'\\') {
      out << L'\\';
    }
    out << sub_path;

    return out.str();
  }

private:
  ChannelID channel;
  WindowsWorkerPlatform *platform;

  wstring path;
  HANDLE root;
  OVERLAPPED overlapped;

  DWORD buffer_size;
  unique_ptr<BYTE[]> buffer;
  unique_ptr<BYTE[]> written;
};

class WindowsWorkerPlatform : public WorkerPlatform {
public:
  WindowsWorkerPlatform(WorkerThread *thread) :
    WorkerPlatform(thread),
    thread_handle{0}
  {
    int err;

    err = uv_mutex_init(&thread_handle_mutex);
    if (err) {
      report_uv_error(err);
    }
  };

  ~WindowsWorkerPlatform() override
  {
    uv_mutex_destroy(&thread_handle_mutex);
  }

  Result<> wake() override
  {
    Lock lock(thread_handle_mutex);

    if (!thread_handle) {
      return ok_result();
    }

    BOOL success = QueueUserAPC(
      command_perform_helper,
      thread_handle,
      reinterpret_cast<ULONG_PTR>(this)
    );
    if (!success) {
      return windows_error_result<>("Unable to queue APC");
    }

    return ok_result();
  }

  Result<> listen() override
  {
    {
      Lock lock(thread_handle_mutex);

      HANDLE pseudo_handle = GetCurrentThread();
      BOOL success = DuplicateHandle(
        GetCurrentProcess(), // Source process
        pseudo_handle, // Source handle
        GetCurrentProcess(), // Destination process
        &thread_handle, // Destination handle
        0, // Desired access
        FALSE, // Inheritable by new processes
        DUPLICATE_SAME_ACCESS // options
      );
      if (!success) {
        Result<> r = windows_error_result<>("Unable to duplicate thread handle");
        report_error(r);
        return r;
      }
    }

    while (true) {
      SleepEx(INFINITE, true);
    }

    report_error("listen loop ended unexpectedly");
    return health_err_result();
  }

  Result<> handle_add_command(const ChannelID channel, const string &root_path)
  {
    // Convert the path to a wide-character string
    Result<wstring> convr = to_wchar(root_path);
    if (convr.is_error()) return convr.propagate<>();
    wstring &root_path_w = convr.get_value();

    // Open a directory handle
    HANDLE root = CreateFileW(
      root_path_w.c_str(), // null-terminated wchar file name
      FILE_LIST_DIRECTORY, // desired access
      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, // share mode
      NULL, // security attributes
      OPEN_EXISTING, // creation disposition
      FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, // flags and attributes
      NULL // template file
    );
    if (root == INVALID_HANDLE_VALUE) {
      return windows_error_result<>("Unable to open directory handle");
    }

    // Allocate and persist the subscription
    Subscription *sub = new Subscription(channel, root, root_path_w, this);
    auto insert_result = subscriptions.insert(make_pair(channel, sub));
    if (!insert_result.second) {
      delete sub;

      ostringstream msg("Channel collision: ");
      msg << channel;
      return error_result(msg.str());
    }

    LOGGER << "Added directory root " << root_path << "." << endl;

    return sub->schedule();
  }

  Result<> handle_remove_command(const ChannelID channel)
  {
    return ok_result();
  }

  Result<> handle_fs_event(DWORD error_code, DWORD num_bytes, Subscription* sub)
  {
    // Ensure that the subscription is valid.
    ChannelID channel = sub->get_channel();
    auto it = subscriptions.find(channel);
    if (it == subscriptions.end() || it->second != sub) {
      return ok_result();
    }

    // Handle errors.
    if (error_code == ERROR_OPERATION_ABORTED) {
      LOGGER << "Operation aborted." << endl;

      subscriptions.erase(it);
      delete sub;

      return ok_result();
    }

    if (error_code == ERROR_INVALID_PARAMETER) {
      Result<> resize = sub->use_network_size();
      if (resize.is_error()) return resize;

      return sub->schedule();
    }

    if (error_code == ERROR_NOTIFY_ENUM_DIR) {
      LOGGER << "Change buffer overflow. Some events may have been lost." << endl;
      return sub->schedule();
    }

    if (error_code != ERROR_SUCCESS) {
      return windows_error_result<>("Completion callback error", error_code);
    }

    // Schedule the next completion callback.
    BYTE *base = sub->get_written(num_bytes);
    Result<> next = sub->schedule();
    if (next.is_error()) {
      report_error(string(next.get_error()));
    }

    // Process received events.
    vector<Message> messages;
    bool old_path_seen = false;
    string old_path;

    while (true) {
      PFILE_NOTIFY_INFORMATION info = reinterpret_cast<PFILE_NOTIFY_INFORMATION>(base);

      Result<> pr = process_event_payload(info, sub, messages, old_path_seen, old_path);
      if (pr.is_error()) {
        LOGGER << "Skipping entry " << pr << "." << endl;
      }

      if (info->NextEntryOffset == 0) {
        break;
      }
      base += info->NextEntryOffset;
    }

    if (!messages.empty()) {
      Result<> er = emit_all(messages.begin(), messages.end());
      if (er.is_error()) {
        LOGGER << "Unable to emit messages: " << er << "." << endl;
      }
    }

    return next;
  }

private:
  Result<> process_event_payload(
    PFILE_NOTIFY_INFORMATION info,
    Subscription *sub,
    vector<Message> &messages,
    bool &old_path_seen,
    string &old_path)
  {
    EntryKind kind = KIND_UNKNOWN;
    ChannelID channel = sub->get_channel();
    wstring relpathw{info->FileName, info->FileNameLength / sizeof(WCHAR)};
    wstring pathw = sub->make_absolute(move(relpathw));

    DWORD attrs = GetFileAttributesW(pathw.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES) {
      DWORD attr_err = GetLastError();
      if (attr_err != ERROR_FILE_NOT_FOUND && attr_err != ERROR_PATH_NOT_FOUND) {
        return windows_error_result<>("GetFileAttributesW failed", attr_err);
      }
      Result<> t = windows_error_result<>("GetFileAttributesW reports");
      LOGGER << t << endl;
    } else if (attrs & FILE_ATTRIBUTE_DIRECTORY) {
      kind = KIND_DIRECTORY;
    } else {
      kind = KIND_FILE;
    }
    // TODO check against FILE_ATTRIBUTE_REPARSE_POINT to identify symlinks

    Result<string> u8r = to_utf8(pathw);
    if (u8r.is_error()) return u8r.propagate<>();
    string &path = u8r.get_value();

    switch (info->Action) {
    case FILE_ACTION_ADDED:
      {
        FileSystemPayload payload(channel, ACTION_CREATED, kind, move(path), "");
        Message message(move(payload));

        LOGGER << "Emitting filesystem message " << message << "." << endl;
        messages.push_back(move(message));
      }
      break;
    case FILE_ACTION_MODIFIED:
      {
        FileSystemPayload payload(channel, ACTION_MODIFIED, kind, move(path), "");
        Message message(move(payload));

        LOGGER << "Emitting filesystem message " << message << "." << endl;
        messages.push_back(move(message));
      }
      break;
    case FILE_ACTION_REMOVED:
      {
        FileSystemPayload payload(channel, ACTION_DELETED, kind, move(path), "");
        Message message(move(payload));

        LOGGER << "Emitting filesystem message " << message << "." << endl;
        messages.push_back(move(message));
      }
      break;
    case FILE_ACTION_RENAMED_OLD_NAME:
      old_path_seen = true;
      old_path = move(path);
      break;
    case FILE_ACTION_RENAMED_NEW_NAME:
      if (old_path_seen) {
        // Old name received first
        {
          FileSystemPayload payload(channel, ACTION_RENAMED, kind, move(old_path), move(path));
          Message message(move(payload));

          LOGGER << "Emitting filesystem message " << message << "." << endl;
          messages.push_back(move(message));
        }

        old_path_seen = false;
      } else {
        // No old name. Treat it as a creation
        {
          FileSystemPayload payload(channel, ACTION_CREATED, kind, move(path), "");
          Message message(move(payload));

          LOGGER << "Emitting filesystem message " << message << "." << endl;
          messages.push_back(move(message));
        }
      }
      break;
    default:
      {
        ostringstream out;
        out
          << "Unexpected action " << info->Action
          << " reported by ReadDirectoryChangesW for "
          << path;
        return error_result(out.str());
      }
      break;
    }

    return ok_result();
  }

  uv_mutex_t thread_handle_mutex;
  HANDLE thread_handle;

  map<ChannelID, Subscription*> subscriptions;
};

unique_ptr<WorkerPlatform> WorkerPlatform::for_worker(WorkerThread *thread)
{
  return unique_ptr<WorkerPlatform>(new WindowsWorkerPlatform(thread));
}

void CALLBACK command_perform_helper(__in ULONG_PTR payload)
{
  WindowsWorkerPlatform *platform = reinterpret_cast<WindowsWorkerPlatform*>(payload);
  platform->handle_commands();
}

static void CALLBACK event_helper(DWORD error_code, DWORD num_bytes, LPOVERLAPPED overlapped)
{
  Subscription *sub = static_cast<Subscription*>(overlapped->hEvent);
  Result<> r = sub->get_platform()->handle_fs_event(error_code, num_bytes, sub);
  if (r.is_error()) {
    LOGGER << "Unable to handle filesystem events: " << r << "." << endl;
  }
}

Result<string> to_utf8(const wstring &in)
{
  size_t len = WideCharToMultiByte(
    CP_UTF8, // code page
    0, // flags
    in.data(), // source string
    in.size(), // source string length
    nullptr, // destination string, null to measure
    0, // destination string length
    nullptr, // default char
    nullptr  // used default char
  );
  if (!len) {
    return windows_error_result<string>("Unable to measure string as UTF-8");
  }

  unique_ptr<char[]> payload(new char[len]);
  size_t copied = WideCharToMultiByte(
    CP_UTF8, // code page
    0, // flags
    in.data(), // source string
    in.size(), // source string length
    payload.get(), // destination string
    len, // destination string length
    nullptr, // default char
    nullptr // used default char
  );
  if (!copied) {
    return windows_error_result<string>("Unable to convert string to UTF-8");
  }

  return ok_result(string(payload.get(), len));
}

Result<wstring> to_wchar(const string &in)
{
  size_t wlen = MultiByteToWideChar(
    CP_UTF8, // code page
    0, // flags
    in.c_str(), // source string data
    -1, // source string length (null-terminated)
    0, // output buffer
    0 // output buffer size
  );
  if (wlen == 0) {
    return windows_error_result<wstring>("Unable to measure string as wide string");
  }

  unique_ptr<WCHAR[]> payload(new WCHAR[wlen]);
  size_t conv_success = MultiByteToWideChar(
    CP_UTF8, // code page
    0, // flags,
    in.c_str(), // source string data
    -1, // source string length (null-terminated)
    payload.get(), // output buffer
    wlen // output buffer size (in bytes)
  );
  if (!conv_success) {
    return windows_error_result<wstring>("Unable to convert string to wide string");
  }

  return ok_result(wstring(payload.get(), wlen - 1));
}

template < class V >
Result<V> windows_error_result(const string &prefix)
{
  return windows_error_result<V>(prefix, GetLastError());
}

template < class V >
Result<V> windows_error_result(const string &prefix, DWORD error_code)
{
  LPVOID msg_buffer;

  FormatMessage(
    FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
    NULL, // source
    error_code, // message ID
    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // language ID
    (LPSTR) &msg_buffer, // output buffer
    0, // size
    NULL // arguments
  );

  ostringstream msg;
  msg << prefix << "\n (" << error_code << ") " << (char*) msg_buffer;
  LocalFree(msg_buffer);

  return Result<V>::make_error(msg.str());
}
