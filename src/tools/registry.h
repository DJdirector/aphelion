#pragma once

#include <string>
#include <vector>

#include "../ai/types.h"

namespace tools {

// Every tool the AI is allowed to call, advertised to the provider on each
// request.
std::vector<ai::ToolDefinition> availableTools();

// True if the named tool requires user confirmation before running (see
// Tool::isDestructive). Callers (main.cpp's tool-call loop) must check this
// - and get explicit confirmation - before calling executeToolCall on a
// destructive tool. Returns false for an unrecognized tool name.
bool isDestructive(const std::string& tool_name);

// Human-readable preview of what this specific call is about to do, for
// showing the user before they confirm. Only meaningful when isDestructive()
// is true for call.name; returns an empty string otherwise or if the tool
// is unrecognized.
std::string getConfirmationPreview(const ai::ToolCall& call);

// Executes a single tool call requested by the model and returns the
// corresponding "tool" role message (content = result, tool_call_id/tool_name
// carried through so providers can address it correctly on the next request).
// Callers are responsible for the confirmation gate above - this function
// does not check isDestructive() itself and will run a destructive tool
// unconditionally if called directly.
ai::Message executeToolCall(const ai::ToolCall& call);

}  // namespace tools
