#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <uv.h>
#include <string>
#include <sstream>
#include <memory>

#include "../worker_platform.h"
#include "../worker_thread.h"
#include "../../message.h"
#include "../../log.h"
#include "../../lock.h"

using std::string;
using std::ostringstream;
using std::unique_ptr;
using std::endl;

static void CALLBACK command_perform_helper(__in ULONG_PTR payload);


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
      return windows_error_result("Unable to queue APC");
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
        Result<> r = windows_error_result("Unable to duplicate thread handle");
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
    return ok_result();
  }

  Result<> handle_remove_command(const ChannelID channel)
  {
    return ok_result();
  }

private:
  uv_mutex_t thread_handle_mutex;
  HANDLE thread_handle;
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
