#pragma once

#include "provider.h"
#include "settings.h"

namespace ai {

// OpenRouter (https://openrouter.ai) - OpenAI-compatible /chat/completions API,
// gives access to many hosted models behind a single key.
class OpenRouterProvider : public AIProvider {
 public:
  explicit OpenRouterProvider(ProviderConfig config);

  AIResponse sendMessage(const std::vector<Message>& history,
                          const std::vector<ToolDefinition>& tools = {}) override;
  std::string name() const override { return "openrouter"; }

 private:
  ProviderConfig config_;
  std::string api_key_;
};

}  // namespace ai
