#pragma once

#include <string>
#include <vector>

#include "types.h"

namespace ai {

// Every backend (Gemini, OpenRouter, Ollama, ...) implements this. main.cpp
// never talks to a provider directly - it goes through this interface, and
// provider_factory.h picks the concrete implementation based on settings.json.
class AIProvider {
 public:
  virtual ~AIProvider() = default;

  // Sends the full conversation so far and returns the model's reply.
  // `tools` is optional and currently unused by callers (tool execution
  // isn't implemented yet), but every provider already speaks it so wiring
  // tools in later doesn't require touching the provider layer again.
  virtual AIResponse sendMessage(const std::vector<Message>& history,
                                  const std::vector<ToolDefinition>& tools = {}) = 0;

  virtual std::string name() const = 0;
};

}  // namespace ai
