#include <map>
#include <memory>
#include <nan.h>
#include <set>
#include <string>
#include <utility>
#include <uv.h>
#include <v8.h>
#include <vector>

#include "hub.h"
#include "log.h"
#include "message.h"
#include "nan/all_callback.h"
#include "nan/async_callback.h"
#include "nan/functional_callback.h"
#include "polling/polling_thread.h"
#include "result.h"
#include "status.h"
#include "worker/worker_thread.h"

using std::endl;
using std::map;
using std::move;
using std::multimap;
using std::set;
using std::shared_ptr;
using std::string;
using std::unique_ptr;
using std::vector;
using v8::Array;
using v8::Local;
using v8::Number;
using v8::Object;
using v8::String;
using v8::Uint32;
using v8::Value;

void handle_events_helper(uv_async_t * /*handle*/)
{
  Hub::get()->handle_events();
}

Hub *Hub::the_hub = nullptr;

Hub::Hub() :
  worker_thread(&event_handler),
  polling_thread(&event_handler),
  next_command_id{NULL_COMMAND_ID + 1},
  next_channel_id{NULL_CHANNEL_ID + 1},
  next_request_id{NULL_REQUEST_ID + 1}
{
  int err;

  report_errable(worker_thread);
  report_errable(polling_thread);

  err = uv_async_init(uv_default_loop(), &event_handler, handle_events_helper);
  if (err != 0) {
    report_uv_error(err);
  }

  report_if_error(worker_thread.run());
  freeze();
}

Result<> Hub::watch(string &&root,
  bool poll,
  bool recursive,
  unique_ptr<AsyncCallback> ack_callback,
  unique_ptr<AsyncCallback> event_callback)
{
  if (!check_async(ack_callback)) return ok_result();

  ChannelID channel_id = next_channel_id;
  next_channel_id++;

  channel_callbacks.emplace(channel_id, move(event_callback));

  if (poll) {
    return send_command(
      polling_thread, CommandPayloadBuilder::add(channel_id, move(root), recursive, 1), move(ack_callback));
  }

  return send_command(
    worker_thread, CommandPayloadBuilder::add(channel_id, move(root), recursive, 1), move(ack_callback));
}

Result<> Hub::unwatch(ChannelID channel_id, unique_ptr<AsyncCallback> &&ack_callback)
{
  if (!check_async(ack_callback)) return ok_result();

  string root;
  shared_ptr<AllCallback> all = AllCallback::create(move(ack_callback));

  Result<> r = ok_result();
  r &= send_command(
    worker_thread, CommandPayloadBuilder::remove(channel_id), all->create_callback("@atom/watcher:hub.unwatch.worker"));
  r &= send_command(polling_thread,
    CommandPayloadBuilder::remove(channel_id),
    all->create_callback("@atom/worker:hub.unwatch.polling"));

  auto maybe_event_callback = channel_callbacks.find(channel_id);
  if (maybe_event_callback == channel_callbacks.end()) {
    LOGGER << "Channel " << channel_id << " already has no event callback." << endl;
    return r;
  }
  channel_callbacks.erase(maybe_event_callback);
  return r;
}

Result<> Hub::status(std::unique_ptr<AsyncCallback> &&status_callback)
{
  if (!check_async(status_callback)) return ok_result();

  RequestID request_id = next_request_id;
  next_request_id++;

  unique_ptr<StatusReq> req{new StatusReq(move(status_callback))};

  // Main thread statistics
  req->status.pending_callback_count = pending_callbacks.size();
  req->status.channel_callback_count = channel_callbacks.size();

  status_reqs.emplace(request_id, move(req));

  Result<> r = ok_result();
  r &= send_command(worker_thread, CommandPayloadBuilder::status(request_id), noop_callback());
  r &= send_command(polling_thread, CommandPayloadBuilder::status(request_id), noop_callback());
  return r;
}

void Hub::handle_events()
{
  handle_events_from(worker_thread);
  handle_events_from(polling_thread);
}

Result<> Hub::send_command(Thread &thread, CommandPayloadBuilder &&builder, std::unique_ptr<AsyncCallback> callback)
{
  CommandID command_id = next_command_id;
  builder.set_id(command_id);
  Message command(builder.build());
  pending_callbacks.emplace(command_id, move(callback));
  next_command_id++;

  LOGGER << "Sending command " << command << " to " << thread << "." << endl;
  Result<bool> sr = thread.send(move(command));
  if (sr.is_error()) return sr.propagate();
  if (sr.get_value()) handle_events();
  return ok_result();
}

bool Hub::check_async(const std::unique_ptr<AsyncCallback> &callback)
{
  if (is_healthy()) return true;

  Nan::HandleScope scope;
  Local<Value> err = Nan::Error(get_message().c_str());
  Local<Value> argv[] = {err};
  callback->SyncCall(1, argv);
  return false;
}

void Hub::handle_events_from(Thread &thread)
{
  Nan::HandleScope scope;
  bool repeat = true;

  unique_ptr<vector<Message>> accepted = thread.receive_all();
  if (!accepted) {
    // No events to process.
    return;
  }

  map<ChannelID, vector<Local<Object>>> to_deliver;
  multimap<ChannelID, Local<Value>> errors;
  set<ChannelID> to_unwatch;

  for (Message &message : *accepted) {
    const AckPayload *ack = message.as_ack();
    if (ack != nullptr) {
      LOGGER << "Received ack message " << message << "." << endl;

      auto maybe_callback = pending_callbacks.find(ack->get_key());
      if (maybe_callback == pending_callbacks.end()) {
        LOGGER << "Ignoring unexpected ack " << message << "." << endl;
        continue;
      }

      unique_ptr<AsyncCallback> callback = move(maybe_callback->second);
      pending_callbacks.erase(maybe_callback);

      ChannelID channel_id = ack->get_channel_id();
      if (ack->was_successful()) {
        Local<Value> argv[] = {Nan::Null(), Nan::New<Number>(channel_id)};
        callback->Call(2, argv);
      } else {
        Local<Value> err = Nan::Error(ack->get_message().c_str());
        Local<Value> argv[] = {err, Nan::Null()};
        callback->Call(2, argv);
      }

      continue;
    }

    const FileSystemPayload *fs = message.as_filesystem();
    if (fs != nullptr) {
      LOGGER << "Received filesystem event message " << message << "." << endl;

      ChannelID channel_id = fs->get_channel_id();

      Local<Object> js_event = Nan::New<Object>();
      js_event->Set(
        Nan::New<String>("action").ToLocalChecked(), Nan::New<Number>(static_cast<int>(fs->get_filesystem_action())));
      js_event->Set(
        Nan::New<String>("kind").ToLocalChecked(), Nan::New<Number>(static_cast<int>(fs->get_entry_kind())));
      js_event->Set(
        Nan::New<String>("oldPath").ToLocalChecked(), Nan::New<String>(fs->get_old_path()).ToLocalChecked());
      js_event->Set(Nan::New<String>("path").ToLocalChecked(), Nan::New<String>(fs->get_path()).ToLocalChecked());

      to_deliver[channel_id].push_back(js_event);
      continue;
    }

    const CommandPayload *command = message.as_command();
    if (command != nullptr) {
      LOGGER << "Received command message " << message << "." << endl;

      if (command->get_action() == COMMAND_DRAIN) {
        Result<bool> dr = thread.drain();
        if (dr.is_error()) {
          LOGGER << "Unable to drain dead letter office: " << dr << "." << endl;
        } else if (dr.get_value()) {
          repeat = true;
        }
      } else if (command->get_action() == COMMAND_ADD && &thread == &worker_thread) {
        polling_thread.send(move(message));
      } else {
        LOGGER << "Ignoring unexpected command." << endl;
      }

      continue;
    }

    const ErrorPayload *error = message.as_error();
    if (error != nullptr) {
      LOGGER << "Received error message " << message << "." << endl;

      const ChannelID &channel_id = error->get_channel_id();

      Local<Value> js_err = Nan::Error(error->get_message().c_str());
      errors.emplace(channel_id, js_err);

      if (error->was_fatal()) {
        to_unwatch.insert(channel_id);
      }

      continue;
    }

    const StatusPayload *status = message.as_status();
    if (status != nullptr) {
      LOGGER << "Received status message " << message << "." << endl;

      const RequestID &request_id = status->get_request_id();

      auto req = status_reqs.find(request_id);
      if (req == status_reqs.end()) {
        LOGGER << "Unrecognized request ID " << request_id << "." << endl;
        continue;
      }

      Status &s = req->second->status;
      if (&thread == &worker_thread) {
        s.assimilate_worker_status(status->get_status());
      } else if (&thread == &polling_thread) {
        s.assimilate_polling_status(status->get_status());
      } else {
        LOGGER << "Unknown thread." << endl;
        continue;
      }

      if (s.complete()) {
        handle_completed_status(*(req->second));
        status_reqs.erase(req);
        LOGGER << "Status request " << request_id << " has been completed." << endl;
      }

      continue;
    }

    LOGGER << "Received unexpected message " << message << "." << endl;
  }

  for (auto &pair : to_deliver) {
    const ChannelID &channel_id = pair.first;
    vector<Local<Object>> &js_events = pair.second;

    auto maybe_callback = channel_callbacks.find(channel_id);
    if (maybe_callback == channel_callbacks.end()) {
      LOGGER << "Ignoring unexpected filesystem event channel " << channel_id << "." << endl;
      continue;
    }
    shared_ptr<AsyncCallback> callback = maybe_callback->second;

    LOGGER << "Dispatching " << js_events.size() << " event(s) on channel " << channel_id << " to the node callback."
           << endl;

    Local<Array> js_array = Nan::New<Array>(js_events.size());

    int index = 0;
    for (auto &js_event : js_events) {
      js_array->Set(index, js_event);
      index++;
    }

    Local<Value> argv[] = {Nan::Null(), js_array};
    callback->Call(2, argv);
  }

  for (auto &pair : errors) {
    const ChannelID &channel_id = pair.first;
    Local<Value> &err = pair.second;

    auto maybe_callback = channel_callbacks.find(channel_id);
    if (maybe_callback == channel_callbacks.end()) {
      LOGGER << "Error reported for unexpected channel " << channel_id << "." << endl;
      continue;
    }
    shared_ptr<AsyncCallback> callback = maybe_callback->second;

    LOGGER << "Report an error on channel " << channel_id << " to the node callback." << endl;

    Local<Value> argv[] = {err};
    callback->Call(1, argv);
  }

  for (const ChannelID &channel_id : to_unwatch) {
    Result<> er = unwatch(channel_id, noop_callback());
    if (er.is_error()) LOGGER << "Unable to unwatch fatally errored channel " << channel_id << "." << endl;
  }

  if (repeat) handle_events_from(thread);
}

void Hub::handle_completed_status(StatusReq &req)
{
  Status &status = req.status;

  Local<Object> status_object = Nan::New<Object>();

  // Main thread
  Nan::Set(status_object,
    Nan::New<String>("pendingCallbackCount").ToLocalChecked(),
    Nan::New<Uint32>(static_cast<uint32_t>(status.pending_callback_count)));
  Nan::Set(status_object,
    Nan::New<String>("channelCallbackCount").ToLocalChecked(),
    Nan::New<Uint32>(static_cast<uint32_t>(status.channel_callback_count)));

  // Worker thread
  Nan::Set(status_object,
    Nan::New<String>("workerThreadState").ToLocalChecked(),
    Nan::New<String>(status.worker_thread_state).ToLocalChecked());
  Nan::Set(status_object,
    Nan::New<String>("workerThreadOk").ToLocalChecked(),
    Nan::New<String>(status.worker_thread_ok).ToLocalChecked());
  Nan::Set(status_object,
    Nan::New<String>("workerInSize").ToLocalChecked(),
    Nan::New<Uint32>(static_cast<uint32_t>(status.worker_in_size)));
  Nan::Set(status_object,
    Nan::New<String>("workerInOk").ToLocalChecked(),
    Nan::New<String>(status.worker_in_ok).ToLocalChecked());
  Nan::Set(status_object,
    Nan::New<String>("workerOutSize").ToLocalChecked(),
    Nan::New<Uint32>(static_cast<uint32_t>(status.worker_out_size)));
  Nan::Set(status_object,
    Nan::New<String>("workerOutOk").ToLocalChecked(),
    Nan::New<String>(status.worker_out_ok).ToLocalChecked());

  Nan::Set(status_object,
    Nan::New<String>("workerSubscriptionCount").ToLocalChecked(),
    Nan::New<Uint32>(static_cast<uint32_t>(status.worker_subscription_count)));
#ifdef PLATFORM_MACOS
  Nan::Set(status_object,
    Nan::New<String>("workerRenameBufferSize").ToLocalChecked(),
    Nan::New<Uint32>(static_cast<uint32_t>(status.worker_rename_buffer_size)));
  Nan::Set(status_object,
    Nan::New<String>("workerRecentFileCacheSize").ToLocalChecked(),
    Nan::New<Uint32>(static_cast<uint32_t>(status.worker_recent_file_cache_size)));
#endif
#ifdef PLATFORM_LINUX
  Nan::Set(status_object,
    Nan::New<String>("workerWatchDescriptorCount").ToLocalChecked(),
    Nan::New<Uint32>(static_cast<uint32_t>(status.worker_watch_descriptor_count)));
  Nan::Set(status_object,
    Nan::New<String>("workerChannelCount").ToLocalChecked(),
    Nan::New<Uint32>(static_cast<uint32_t>(status.worker_channel_count)));
  Nan::Set(status_object,
    Nan::New<String>("workerCookieJarSize").ToLocalChecked(),
    Nan::New<Uint32>(static_cast<uint32_t>(status.worker_cookie_jar_size)));
#endif

  // Polling thread
  Nan::Set(status_object,
    Nan::New<String>("pollingThreadState").ToLocalChecked(),
    Nan::New<String>(status.polling_thread_state).ToLocalChecked());
  Nan::Set(status_object,
    Nan::New<String>("pollingThreadOk").ToLocalChecked(),
    Nan::New<String>(status.polling_thread_ok).ToLocalChecked());
  Nan::Set(status_object,
    Nan::New<String>("pollingInSize").ToLocalChecked(),
    Nan::New<Uint32>(static_cast<uint32_t>(status.polling_in_size)));
  Nan::Set(status_object,
    Nan::New<String>("pollingInOk").ToLocalChecked(),
    Nan::New<String>(status.polling_in_ok).ToLocalChecked());
  Nan::Set(status_object,
    Nan::New<String>("pollingOutSize").ToLocalChecked(),
    Nan::New<Uint32>(static_cast<uint32_t>(status.polling_out_size)));
  Nan::Set(status_object,
    Nan::New<String>("pollingOutOk").ToLocalChecked(),
    Nan::New<String>(status.polling_out_ok).ToLocalChecked());
  Nan::Set(status_object,
    Nan::New<String>("pollingRootCount").ToLocalChecked(),
    Nan::New<Uint32>(static_cast<uint32_t>(status.polling_root_count)));
  Nan::Set(status_object,
    Nan::New<String>("pollingEntryCount").ToLocalChecked(),
    Nan::New<Uint32>(static_cast<uint32_t>(status.polling_entry_count)));

  Local<Value> argv[] = {Nan::Null(), status_object};
  req.callback->Call(2, argv);
}
