#include "wcgl-debug.h"

namespace {

wcgl::DebugCallback debugCallback_s;
std::string prevMessage_s;
std::vector<std::pair<wcgl::DebugSeverity, std::string>> messageQueue_s;

} // namespace <anon>

void wcgl::setDebugCallback(wcgl::DebugCallback callback) {
  debugCallback_s = callback;

  // If messages were printed before the callback was sent, print them now.
  if (messageQueue_s.size() > 0) {
    for (auto& msg : messageQueue_s) {
      debugCallback_s(msg.first, msg.second);
    }
    messageQueue_s.clear();
  }
}

void wcgl::debugPrint(wcgl::DebugSeverity severity, std::string_view message) {
  // Prevent repeated messages.
  if (prevMessage_s == message)
    return;

  // If the debug callback has been set, call it to print the message.
  if (debugCallback_s)
    debugCallback_s(severity, message);
  // If it hasn't been set yet, add the message to the queue.
  else 
    messageQueue_s.push_back({severity, std::string(message)});

  // Save the message so it doesn't get repeated.
  prevMessage_s = message;
}