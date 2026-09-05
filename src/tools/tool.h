#pragma once

#include <string>

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

  // True if this tool can modify files, run commands, or otherwise change
  // state outside the conversation - anything the user should explicitly
  // approve before it runs. Defaults to false so read-only tools (like
  // scan_project) don't need to override anything. The confirmation prompt
  // itself is a REPL/UI concern and lives in main.cpp, not here - this
  // interface only decides whether one is required.
  virtual bool isDestructive() const { return false; }

  // Human-readable description of exactly what this call is about to do -
  // e.g. a diff for an edit, the literal command for a shell tool. Only
  // ever called when isDestructive() is true, right before the user is
  // asked to confirm. Default is empty since non-destructive tools never
  // need one.
  virtual std::string confirmationPreview(const ai::ToolCall& call) const { return ""; }
};

}  // namespace tools
