#pragma once

#include "provider.h"
#include "settings.h"

namespace ai {

// Local Ollama instance (https://github.com/ollama/ollama), talks to the
// /api/chat endpoint. No API key involved; base_url defaults to localhost.
class OllamaProvider : public AIProvider {
 public:
  explicit OllamaProvider(ProviderConfig config);

  AIResponse sendMessage(const std::vector<Message>& history,
                          const std::vector<ToolDefinition>& tools = {}) override;
  std::string name() const override { return "ollama"; }

 private:
  ProviderConfig config_;
};

}  // namespace ai
