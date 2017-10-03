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
#include "nan/all_callback.h"
#include "hub.h"

using v8::Local;
using v8::Value;
using v8::Object;
using v8::String;
using v8::Number;
using v8::Array;
using Nan::Callback;
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

Hub Hub::the_hub;

Hub::Hub() :
  worker_thread(&event_handler),
  polling_thread(&event_handler)
{
  int err;

  next_command_id = NULL_COMMAND_ID + 1;
  next_channel_id = NULL_CHANNEL_ID + 1;

  err = uv_async_init(uv_default_loop(), &event_handler, handle_events_helper);
  if (err) return;

  worker_thread.run();
}

Result<> Hub::watch(
  string &&root,
  bool poll,
  unique_ptr<Callback> ack_callback,
  unique_ptr<Callback> event_callback
) {
  ChannelID channel_id = next_channel_id;
  next_channel_id++;

  channel_callbacks.emplace(channel_id, move(event_callback));

  if (poll) {
    return send_command(polling_thread, COMMAND_ADD, move(ack_callback), move(root), channel_id);
  } else {
    return send_command(worker_thread, COMMAND_ADD, move(ack_callback), move(root), channel_id);
  }
}

Result<> Hub::unwatch(ChannelID channel_id, unique_ptr<Callback> ack_callback)
{
  string root;
  AllCallback &all = AllCallback::create(move(ack_callback));

  Result<> r = ok_result();
  r &= send_command(worker_thread, COMMAND_REMOVE, all.create_callback(), "", channel_id);
  r &= send_command(polling_thread, COMMAND_REMOVE, all.create_callback(), "", channel_id);

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
  handle_events_from(worker_thread);
  handle_events_from(polling_thread);
}

void Hub::collect_status(Status &status)
{
  status.pending_callback_count = pending_callbacks.size();
  status.channel_callback_count = channel_callbacks.size();

  worker_thread.collect_status(status);
  polling_thread.collect_status(status);
}

Result<> Hub::send_command(
  Thread &thread,
  const CommandAction action,
  unique_ptr<Callback> callback,
  const string &&root,
  ChannelID channel_id
) {
  CommandID command_id = next_command_id;
  Message command(CommandPayload(action, command_id, move(root), channel_id));
  pending_callbacks.emplace(command_id, move(callback));
  next_command_id++;

  LOGGER << "Sending command " << command << " to the worker thread." << endl;
  Result<bool> sr = worker_thread.send(move(command));
  if (sr.is_error()) return sr.propagate();
  if (sr.get_value()) handle_events();
  return ok_result();
}

void Hub::handle_events_from(Thread &thread)
{
  Nan::HandleScope scope;
  bool repeat = true;

  Result< unique_ptr<vector<Message>> > rr = thread.receive_all();
  if (rr.is_error()) {
    LOGGER << "Unable to receive messages from thread: " << rr << "." << endl;
    return;
  }

  unique_ptr<vector<Message>> &accepted = rr.get_value();
  if (!accepted) {
    // No events to process.
    return;
  }

  unordered_map<ChannelID, vector<Local<Object>>> to_deliver;

  for (Message &message : *accepted) {
    const AckPayload *ack = message.as_ack();
    if (ack) {
      LOGGER << "Received ack message " << message << "." << endl;

      auto maybe_callback = pending_callbacks.find(ack->get_key());
      if (maybe_callback == pending_callbacks.end()) {
        LOGGER << "Ignoring unexpected ack " << message << "." << endl;
        continue;
      }

      unique_ptr<Callback> callback = move(maybe_callback->second);
      pending_callbacks.erase(maybe_callback);

      ChannelID channel_id = ack->get_channel_id();
      if (channel_id != NULL_CHANNEL_ID) {
        if (ack->was_successful()) {
          Local<Value> argv[] = {Nan::Null(), Nan::New<Number>(channel_id)};
          callback->Call(2, argv);
        } else {
          Local<Value> err = Nan::Error(ack->get_message().c_str());
          Local<Value> argv[] = {err, Nan::Null()};
          callback->Call(2, argv);
        }
      } else {
        callback->Call(0, nullptr);
      }

      continue;
    }

    const FileSystemPayload *fs = message.as_filesystem();
    if (fs) {
      LOGGER << "Received filesystem event message " << message << "." << endl;

      ChannelID channel_id = fs->get_channel_id();

      Local<Object> js_event = Nan::New<Object>();
      js_event->Set(
        Nan::New<String>("action").ToLocalChecked(),
        Nan::New<Number>(static_cast<int>(fs->get_filesystem_action()))
      );
      js_event->Set(
        Nan::New<String>("kind").ToLocalChecked(),
        Nan::New<Number>(static_cast<int>(fs->get_entry_kind()))
      );
      js_event->Set(
        Nan::New<String>("oldPath").ToLocalChecked(),
        Nan::New<String>(fs->get_old_path()).ToLocalChecked()
      );
      js_event->Set(
        Nan::New<String>("path").ToLocalChecked(),
        Nan::New<String>(fs->get_path()).ToLocalChecked()
      );

      to_deliver[channel_id].push_back(js_event);
      continue;
    }

    const CommandPayload *command = message.as_command();
    if (command) {
      LOGGER << "Received command message " << message << "." << endl;

      if (command->get_action() == COMMAND_DRAIN) {
        Result<bool> dr = thread.drain();
        if (dr.is_error()) {
          LOGGER << "Unable to drain dead letter office: " << dr << "." << endl;
        } else if (dr.get_value()) {
          repeat = true;
        }
      } else {
        LOGGER << "Ignoring unexpected command." << endl;
      }

      continue;
    }

    LOGGER << "Received unexpected message " << message << "." << endl;
  }

  for (auto &pair : to_deliver) {
    ChannelID channel_id = pair.first;
    vector<Local<Object>> js_events = pair.second;

    auto maybe_callback = channel_callbacks.find(channel_id);
    if (maybe_callback == channel_callbacks.end()) {
      LOGGER << "Ignoring unexpected filesystem event channel " << channel_id << "." << endl;
      continue;
    }
    shared_ptr<Callback> callback = maybe_callback->second;

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

  if (repeat) handle_events_from(thread);
}
