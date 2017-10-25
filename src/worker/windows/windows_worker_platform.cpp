#define WIN32_LEAN_AND_MEAN

#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <uv.h>
#include <vector>
#include <windows.h>

#include "../../helper/windows/helper.h"
#include "../../lock.h"
#include "../../log.h"
#include "../../message.h"
#include "../../message_buffer.h"
#include "../worker_platform.h"
#include "../worker_thread.h"
#include "subscription.h"

using std::default_delete;
using std::endl;
using std::make_pair;
using std::map;
using std::move;
using std::ostream;
using std::ostringstream;
using std::pair;
using std::shared_ptr;
using std::string;
using std::unique_ptr;
using std::vector;
using std::wostringstream;
using std::wstring;

static void CALLBACK command_perform_helper(__in ULONG_PTR payload);

static void CALLBACK event_helper(DWORD error_code, DWORD num_bytes, LPOVERLAPPED overlapped);

class WindowsWorkerPlatform : public WorkerPlatform
{
public:
  WindowsWorkerPlatform(WorkerThread *thread) : WorkerPlatform(thread), thread_handle{0}
  {
    int err;

    err = uv_mutex_init(&thread_handle_mutex);
    if (err) {
      report_uv_error(err);
    }
  };

  ~WindowsWorkerPlatform() override { uv_mutex_destroy(&thread_handle_mutex); }

  Result<> wake() override
  {
    if (!is_healthy()) return health_err_result();

    Lock lock(thread_handle_mutex);

    if (!thread_handle) {
      return ok_result();
    }

    BOOL success = QueueUserAPC(command_perform_helper, thread_handle, reinterpret_cast<ULONG_PTR>(this));
    if (!success) {
      return windows_error_result<>("Unable to queue APC");
    }

    return ok_result();
  }

  Result<> listen() override
  {
    if (!is_healthy()) return health_err_result();

    {
      Lock lock(thread_handle_mutex);

      HANDLE pseudo_handle = GetCurrentThread();
      BOOL success = DuplicateHandle(GetCurrentProcess(),  // Source process
        pseudo_handle,  // Source handle
        GetCurrentProcess(),  // Destination process
        &thread_handle,  // Destination handle
        0,  // Desired access
        FALSE,  // Inheritable by new processes
        DUPLICATE_SAME_ACCESS  // options
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

  Result<bool> handle_add_command(CommandID command,
    ChannelID channel,
    const string &root_path,
    bool recursive) override
  {
    if (!is_healthy()) return health_err_result().propagate<bool>();

    // Convert the path to a wide-character string
    Result<wstring> convr = to_wchar(root_path);
    if (convr.is_error()) return convr.propagate<bool>();
    wstring &root_path_w = convr.get_value();

    // Open a directory handle
    HANDLE root = CreateFileW(root_path_w.c_str(),  // null-terminated wchar file name
      FILE_LIST_DIRECTORY,  // desired access
      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,  // share mode
      NULL,  // security attributes
      OPEN_EXISTING,  // creation disposition
      FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,  // flags and attributes
      NULL  // template file
    );
    if (root == INVALID_HANDLE_VALUE) {
      return windows_error_result<bool>("Unable to open directory handle");
    }

    // Allocate and persist the subscription
    Subscription *sub = new Subscription(channel, root, root_path_w, recursive, this);
    auto insert_result = subscriptions.insert(make_pair(channel, sub));
    if (!insert_result.second) {
      delete sub;

      ostringstream msg("Channel collision: ");
      msg << channel;
      return Result<bool>::make_error(msg.str());
    }

    ostream &logline = LOGGER << "Added directory root " << root_path;
    if (!recursive) logline << " (non-recursive)";
    logline << " at channel " << channel << "." << endl;

    Result<bool> schedr = sub->schedule(&event_helper);
    if (schedr.is_error()) return schedr.propagate<bool>();
    if (!schedr.get_value()) {
      LOGGER << "Falling back to polling for watch root " << root_path << "." << endl;

      return emit(Message(CommandPayloadBuilder::add(channel, string(root_path), recursive, 1).build()))
        .propagate(false);
    }

    return ok_result(true);
  }

  Result<bool> handle_remove_command(CommandID command, ChannelID channel) override
  {
    if (!is_healthy()) return health_err_result().propagate<bool>();

    auto it = subscriptions.find(channel);
    if (it == subscriptions.end()) {
      LOGGER << "Channel " << channel << " was already removed." << endl;
      return ok_result(true);
    }

    Result<> r = it->second->stop(command);
    if (r.is_error()) return r.propagate<bool>();

    LOGGER << "Subscription for channel " << channel << " stopped." << endl;
    return ok_result(false);
  }

  Result<> handle_fs_event(DWORD error_code, DWORD num_bytes, Subscription *sub)
  {
    if (!is_healthy()) return health_err_result();

    // Ensure that the subscription is valid.
    ChannelID channel = sub->get_channel();
    auto it = subscriptions.find(channel);
    if (it == subscriptions.end() || it->second != sub) {
      return ok_result();
    }

    // Subscription termination.
    bool terminate = false;
    if (error_code == ERROR_OPERATION_ABORTED) {
      LOGGER << "Completing termination of channel " << channel << "." << endl;
      terminate = true;
    } else if (sub->is_terminating()) {
      LOGGER << "Filesystem event encountered on terminating channel " << channel << "." << endl;
      terminate = true;
    }
    if (terminate) {
      AckPayload ack(sub->get_command_id(), channel, true, "");
      Message response(move(ack));

      subscriptions.erase(it);
      delete sub;

      return emit(move(response));
    }

    // Handle errors.
    if (error_code == ERROR_INVALID_PARAMETER) {
      LOGGER << "Attempting to revert to a network-friendly buffer size." << endl;
      Result<> resize = sub->use_network_size();
      if (resize.is_error()) return resize;

      return sub->schedule(&event_helper).propagate_as_void();
    }

    if (error_code == ERROR_NOTIFY_ENUM_DIR) {
      LOGGER << "Change buffer overflow. Some events may have been lost." << endl;
      return sub->schedule(&event_helper).propagate_as_void();
    }

    if (error_code != ERROR_SUCCESS) {
      return windows_error_result<>("Completion callback error", error_code);
    }

    // Schedule the next completion callback.
    BYTE *base = sub->get_written(num_bytes);
    Result<bool> next = sub->schedule(&event_helper);

    // Process received events.
    MessageBuffer buffer;
    ChannelMessageBuffer messages(buffer, channel);
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
      if (er.is_error()) LOGGER << "Unable to emit messages: " << er << "." << endl;
    }

    return next.propagate_as_void();
  }

private:
  Result<> reschedule(Subscription *sub)
  {
    Result<bool> sch = sub->schedule(&event_helper);
    if (sch.is_error()) return emit_fatal_error(sub, sch.propagate_as_void());

    if (!sch.get_value()) {
      Result<string> root = sub->get_root_path();
      if (root.is_error()) return emit_fatal_error(sub, root.propagate_as_void());

      LOGGER << "Falling back to polling for path " << root.get_value() << " at channel " << sub->get_channel() << "." << endl;

      Result<> rem = remove(sub);
      rem &= emit(Message(CommandPayloadBuilder::add(sub->get_channel(), move(root.get_value()), sub->is_recursive(), 1).build()));
      return rem;
    }

    return ok_result();
  }

  Result<> remove(Subscription *sub)
  {
    Message response(AckPayload(sub->get_command_id(), sub->get_channel(), true, ""));

    // Ensure that the subscription is valid.
    ChannelID channel = sub->get_channel();
    auto it = subscriptions.find(channel);
    if (it == subscriptions.end() || it->second != sub) {
      return ok_result();
    }

    subscriptions.erase(it);
    delete sub;

    if (sub->get_command_id() != NULL_COMMAND_ID) {
      return emit(move(response));
    } else {
      return ok_result();
    }
  }

  Result<> emit_fatal_error(Subscription *sub, Result<> &&r)
  {
    assert(r.is_error());

    Result<> out = emit(Message(ErrorPayload(sub->get_channel(), string(r.get_error()), true)));
    out &= remove(sub);
    return out;
  }

  Result<> process_event_payload(PFILE_NOTIFY_INFORMATION info,
    Subscription *sub,
    ChannelMessageBuffer &messages,
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
      case FILE_ACTION_ADDED: messages.created(move(path), kind); break;
      case FILE_ACTION_MODIFIED: messages.modified(move(path), kind); break;
      case FILE_ACTION_REMOVED: messages.deleted(move(path), kind); break;
      case FILE_ACTION_RENAMED_OLD_NAME:
        old_path_seen = true;
        old_path = move(path);
        break;
      case FILE_ACTION_RENAMED_NEW_NAME:
        if (old_path_seen) {
          // Old name received first
          messages.renamed(move(old_path), move(path), kind);
          old_path_seen = false;
        } else {
          // No old name. Treat it as a creation
          messages.created(move(path), kind);
        }
        break;
      default: {
        ostringstream out;
        out << "Unexpected action " << info->Action << " reported by ReadDirectoryChangesW for " << path;
        return error_result(out.str());
      } break;
    }

    return ok_result();
  }

  uv_mutex_t thread_handle_mutex;
  HANDLE thread_handle;

  map<ChannelID, Subscription *> subscriptions;
};

unique_ptr<WorkerPlatform> WorkerPlatform::for_worker(WorkerThread *thread)
{
  return unique_ptr<WorkerPlatform>(new WindowsWorkerPlatform(thread));
}

void CALLBACK command_perform_helper(__in ULONG_PTR payload)
{
  WindowsWorkerPlatform *platform = reinterpret_cast<WindowsWorkerPlatform *>(payload);
  platform->handle_commands();
}

static void CALLBACK event_helper(DWORD error_code, DWORD num_bytes, LPOVERLAPPED overlapped)
{
  Subscription *sub = static_cast<Subscription *>(overlapped->hEvent);
  Result<> r = sub->get_platform()->handle_fs_event(error_code, num_bytes, sub);
  if (r.is_error()) {
    LOGGER << "Unable to handle filesystem events: " << r << "." << endl;
  }
}
