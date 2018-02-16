#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "message.h"
#include "thread_starter.h"

using std::string;
using std::unique_ptr;
using std::vector;

vector<Message> ThreadStarter::get_messages()
{
  vector<Message> results;
  if (logging) {
    results.emplace_back(wrap_command(logging));
  }
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
