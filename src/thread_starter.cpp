#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "message.h"
#include "thread_starter.h"

using std::move;
using std::string;
using std::unique_ptr;
using std::vector;

ThreadStarter::ThreadStarter() : logging(new CommandPayload(COMMAND_LOG_DISABLE))
{
  //
}

vector<Message> ThreadStarter::get_messages()
{
  vector<Message> results;
  results.emplace_back(wrap_command(logging));
  return results;
}

void ThreadStarter::set_command(unique_ptr<CommandPayload> &dest, const CommandPayload *src)
{
  dest.reset(new CommandPayload(src->get_action(), NULL_COMMAND_ID, string(src->get_root()), src->get_channel_id()));
}

Message ThreadStarter::wrap_command(unique_ptr<CommandPayload> &src)
{
  return Message(CommandPayload(src->get_action(), NULL_COMMAND_ID, string(src->get_root()), src->get_channel_id()));
}
