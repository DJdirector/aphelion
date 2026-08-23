#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace ai {

// A single tool/function call requested by the model.
struct ToolCall {
  std::string id;                // provider-assigned call id (may be empty, e.g. Ollama/Gemini)
  std::string name;              // tool/function name
  nlohmann::json arguments;      // parsed JSON object of arguments
  std::string thought_signature; // Gemini 3+ only: opaque token that must be echoed back
                                 // verbatim on the next request, or Gemini rejects it with a 400.
                                 // Empty for other providers, and for non-first parallel calls.
};

// Describes a tool available to the model, in a provider-agnostic shape.
// Each provider adapts this into its own wire format.
struct ToolDefinition {
  std::string name;
  std::string description;
  nlohmann::json parameters;    // JSON schema object
};

// A single turn in the conversation. Mirrors main.cpp's ChatMessage but adds
// the fields needed to round-trip tool calls once they're implemented.
struct Message {
  std::string role;             // "system" | "user" | "assistant" | "tool"
  std::string content;
  std::vector<ToolCall> tool_calls;  // set on assistant messages that call tools
  std::string tool_call_id;          // set on "tool" role messages (result of a call)
  std::string tool_name;             // set on "tool" role messages: which tool produced this
};

// Normalized result of asking a provider for a completion.
struct AIResponse {
  bool ok = false;
  std::string content;
  std::vector<ToolCall> tool_calls;
  std::string error;                 // human-readable error, set when ok == false
  std::string raw_finish_reason;     // provider's finish/stop reason, for debugging
};

}  // namespace ai
