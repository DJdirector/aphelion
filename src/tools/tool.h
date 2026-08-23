#pragma once

#include "../ai/types.h"

namespace tools {

// Every tool the AI can call implements this, the same way each backend
// implements ai::AIProvider. registry.cpp only ever talks to this interface -
// it doesn't know or care how scan_project (or any future tool) is built.
class Tool {
 public:
  virtual ~Tool() = default;

  // Advertised to the model on every request so it knows this tool exists.
  virtual ai::ToolDefinition definition() const = 0;

  // Runs the tool for a call the model made and returns the "tool" role
  // message to feed back (content/tool_call_id/tool_name are the caller's
  // responsibility to set based on `call`, not embedded here).
  virtual ai::Message execute(const ai::ToolCall& call) const = 0;
};

}  // namespace tools
