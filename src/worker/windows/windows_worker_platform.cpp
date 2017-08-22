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

template < class V = void* >
static Result<V> windows_error_result(const string &prefix);

template < class V = void* >
static Result<V> windows_error_result(const string &prefix, DWORD error_code);

class WindowsWorkerPlatform;

class Subscription {
public:
  Subscription(ChannelID channel, HANDLE root, WindowsWorkerPlatform *platform) :
    channel{channel},
    platform{platform},
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

private:
  ChannelID channel;
  WindowsWorkerPlatform *platform;

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
        report_error("Unable to acquire thread handle");
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
    LOGGER << "Watching: " << root_path << endl;

    // Convert the path to a wide-character array
    size_t wlen = MultiByteToWideChar(
      CP_UTF8, // code page
      0, // flags
      root_path.data(), // source string data
      -1, // source string length (null-terminated)
      0, // output buffer
      0 // output buffer size
    );
    if (wlen == 0) {
      return windows_error_result<>("Unable to measure UTF-16 buffer");
    }
    unique_ptr<WCHAR[]> root_path_w{new WCHAR[wlen]};
    size_t conv_success = MultiByteToWideChar(
      CP_UTF8, // code page
      0, // flags,
      root_path.data(), // source string data
      -1, // source string length (null-terminated)
      root_path_w.get(), // output buffer
      wlen // output buffer size
    );
    if (!conv_success) {
      return windows_error_result<>("Unable to convert root path to UTF-16");
    }

    // Open a directory handle
    HANDLE root = CreateFileW(
      root_path_w.get(), // file name
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
    Subscription *sub = new Subscription(channel, root, this);
    auto insert_result = subscriptions.insert(make_pair(channel, sub));
    if (!insert_result.second) {
      delete sub;

      ostringstream msg("Channel collision: ");
      msg << channel;
      return error_result(msg.str());
    }

    LOGGER << "Now watching directory " << root_path << "." << endl;

    return sub->schedule();
  }

  Result<> handle_remove_command(const ChannelID channel)
  {
    return ok_result();
  }

private:
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
    return windows_error_result<string>("Unable to measure path as UTF-8");
  }

  char *out = new char[len];
  size_t copied = WideCharToMultiByte(
    CP_UTF8, // code page
    0, // flags
    in.data(), // source string
    in.size(), // source string length
    out, // destination string
    len, // destination string length
    nullptr, // default char
    nullptr // used default char
  );
  if (!copied) {
    delete [] out;
    return windows_error_result<string>("Unable to convert path to UTF-8");
  }

  return ok_result(string{out, len});
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
