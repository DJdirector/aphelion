#pragma once

#include "provider.h"
#include "settings.h"

namespace ai {

// Google Gemini via the ai.dev / Generative Language API
// (https://generativelanguage.googleapis.com/v1beta/models/{model}:generateContent).
class GeminiProvider : public AIProvider {
 public:
  explicit GeminiProvider(ProviderConfig config);

  AIResponse sendMessage(const std::vector<Message>& history,
                          const std::vector<ToolDefinition>& tools = {}) override;
  std::string name() const override { return "gemini"; }

 private:
  ProviderConfig config_;
  std::string api_key_;
};

}  // namespace ai
