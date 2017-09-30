#include <memory>
#include <string>
#include <utility>
#include <unordered_map>
#include <vector>
#include <nan.h>
#include <uv.h>
#include <v8.h>

#include "result.h"
#include "message.h"
#include "log.h"
#include "worker/worker_thread.h"
#include "polling/polling_thread.h"
#include "hub.h"

using v8::Local;
using v8::Value;
using v8::Object;
using v8::String;
using v8::Number;
using v8::Array;
using std::string;
using std::unique_ptr;
using std::shared_ptr;
using std::unordered_map;
using std::vector;
using std::move;
using std::endl;

void handle_events_helper(uv_async_t *handle)
{
  Hub::get().handle_events();
}

Hub::Hub() :
  worker_thread(&event_handler),
  polling_thread(&event_handler)
{
  int err;

  next_command_id = 0;
  next_channel_id = NULL_CHANNEL_ID + 1;

  err = uv_async_init(uv_default_loop(), &event_handler, handle_events_helper);
  if (err) return;

  worker_thread.run();
}

Result<> Hub::send_worker_command(
  const CommandAction action,
  const string &&root,
  unique_ptr<Nan::Callback> callback,
  ChannelID channel_id
) {
  CommandID command_id = next_command_id;

  CommandPayload command_payload(next_command_id, action, move(root), channel_id);
  Message command_message(move(command_payload));

  pending_callbacks.emplace(command_id, move(callback));

  next_command_id++;

  LOGGER << "Sending command " << command_message << " to the worker thread." << endl;
  return worker_thread.send(move(command_message));
}

Result<> Hub::send_polling_command(
  const CommandAction action,
  const string &&root,
  unique_ptr<Nan::Callback> callback,
  ChannelID channel_id
) {
  CommandID command_id = next_command_id;

  CommandPayload command_payload(next_command_id, action, move(root), channel_id);
  Message command_message(move(command_payload));

  pending_callbacks.emplace(command_id, move(callback));

  next_command_id++;

  LOGGER << "Sending command " << command_message << " to the polling thread." << endl;
  return polling_thread.send(move(command_message));
}

Result<> Hub::watch(
  string &&root,
  bool poll,
  unique_ptr<Nan::Callback> ack_callback,
  unique_ptr<Nan::Callback> event_callback
) {
  ChannelID channel_id = next_channel_id;
  next_channel_id++;

  channel_callbacks.emplace(channel_id, move(event_callback));

  if (poll) {
    return send_polling_command(COMMAND_ADD, move(root), move(ack_callback), channel_id);
  } else {
    return send_worker_command(COMMAND_ADD, move(root), move(ack_callback), channel_id);
  }
}

Result<> Hub::unwatch(ChannelID channel_id, unique_ptr<Nan::Callback> ack_callback)
{
  string root;
  Result<> r = send_worker_command(COMMAND_REMOVE, move(root), move(ack_callback), channel_id);

  auto maybe_event_callback = channel_callbacks.find(channel_id);
  if (maybe_event_callback == channel_callbacks.end()) {
    LOGGER << "Channel " << channel_id << " already has no event callback." << endl;
    return ok_result();
  }
  channel_callbacks.erase(maybe_event_callback);
  return r;
}

void Hub::handle_events()
{
  Nan::HandleScope scope;

  Result< unique_ptr<vector<Message>> > rr = worker_thread.receive_all();
  if (rr.is_error()) {
    LOGGER << "Unable to receive messages from the worker thread: " << rr << "." << endl;
    return;
  }

  unique_ptr<vector<Message>> &accepted = rr.get_value();
  if (!accepted) {
    // No events to process.
    return;
  }

  unordered_map<ChannelID, vector<Local<Object>>> to_deliver;

  for (auto it = accepted->begin(); it != accepted->end(); ++it) {
    const AckPayload *ack_message = it->as_ack();
    if (ack_message) {
      LOGGER << "Received ack message " << *it << "." << endl;

      auto maybe_callback = pending_callbacks.find(ack_message->get_key());
      if (maybe_callback == pending_callbacks.end()) {
        LOGGER << "Ignoring unexpected ack " << *it << "." << endl;
        continue;
      }

      unique_ptr<Nan::Callback> callback = move(maybe_callback->second);
      pending_callbacks.erase(maybe_callback);

      ChannelID channel_id = ack_message->get_channel_id();
      if (channel_id != NULL_CHANNEL_ID) {
        if (ack_message->was_successful()) {
          Local<Value> argv[] = {Nan::Null(), Nan::New<Number>(channel_id)};
          callback->Call(2, argv);
        } else {
          Local<Value> err = Nan::Error(ack_message->get_message().c_str());
          Local<Value> argv[] = {err, Nan::Null()};
          callback->Call(2, argv);
        }
      } else {
        callback->Call(0, nullptr);
      }

      continue;
    }

    const FileSystemPayload *filesystem_message = it->as_filesystem();
    if (filesystem_message) {
      LOGGER << "Received filesystem event message " << *it << "." << endl;

      ChannelID channel_id = filesystem_message->get_channel_id();

      Local<Object> js_event = Nan::New<Object>();
      js_event->Set(
        Nan::New<String>("action").ToLocalChecked(),
        Nan::New<Number>(static_cast<int>(filesystem_message->get_filesystem_action()))
      );
      js_event->Set(
        Nan::New<String>("kind").ToLocalChecked(),
        Nan::New<Number>(static_cast<int>(filesystem_message->get_entry_kind()))
      );
      js_event->Set(
        Nan::New<String>("oldPath").ToLocalChecked(),
        Nan::New<String>(filesystem_message->get_old_path()).ToLocalChecked()
      );
      js_event->Set(
        Nan::New<String>("path").ToLocalChecked(),
        Nan::New<String>(filesystem_message->get_path()).ToLocalChecked()
      );

      to_deliver[channel_id].push_back(js_event);
      continue;
    }

    LOGGER << "Received unexpected message " << *it << "." << endl;
  }

  for (auto it = to_deliver.begin(); it != to_deliver.end(); ++it) {
    ChannelID channel_id = it->first;
    vector<Local<Object>> js_events = it->second;

    auto maybe_callback = channel_callbacks.find(channel_id);
    if (maybe_callback == channel_callbacks.end()) {
      LOGGER << "Ignoring unexpected filesystem event channel " << channel_id << "." << endl;
      continue;
    }
    shared_ptr<Nan::Callback> callback = maybe_callback->second;

    LOGGER << "Dispatching " << js_events.size()
      << " event(s) on channel " << channel_id << " to node callbacks." << endl;

    Local<Array> js_array = Nan::New<Array>(js_events.size());

    int index = 0;
    for (auto et = js_events.begin(); et != js_events.end(); ++et) {
      js_array->Set(index, *et);
      index++;
    }

    Local<Value> argv[] = {
      Nan::Null(),
      js_array
    };
    callback->Call(2, argv);
  }
}

void Hub::collect_status(Status &status)
{
  status.pending_callback_count = pending_callbacks.size();
  status.channel_callback_count = channel_callbacks.size();

  worker_thread.collect_status(status);
  polling_thread.collect_status(status);
}
