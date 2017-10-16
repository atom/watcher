#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "message.h"
#include "thread_starter.h"

using std::string;
using std::unique_ptr;
using std::vector;

ThreadStarter::ThreadStarter() : logging(new CommandPayload(CommandPayloadBuilder::log_disable().build()))
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
  dest.reset(new CommandPayload(*src));
}

Message ThreadStarter::wrap_command(unique_ptr<CommandPayload> &src)
{
  return Message(CommandPayload(*src));
}
