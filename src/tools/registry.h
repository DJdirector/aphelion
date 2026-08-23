#pragma once

#include <vector>

#include "../ai/types.h"

namespace tools {

// Every tool the AI is allowed to call, advertised to the provider on each
// request. Keep this list read-only/side-effect-free for now - executeToolCall
// runs whatever the model asks for with no confirmation gate, which is only
// safe as long as every tool here is non-destructive (like scan_project).
// A confirmation step belongs here before any tool that writes files, runs
// shell commands, etc. gets added.
std::vector<ai::ToolDefinition> availableTools();

// Executes a single tool call requested by the model and returns the
// corresponding "tool" role message (content = result, tool_call_id/tool_name
// carried through so providers can address it correctly on the next request).
ai::Message executeToolCall(const ai::ToolCall& call);

}  // namespace tools
