#include "provider_factory.h"

#include <stdexcept>

#include "gemini_provider.h"
#include "ollama_provider.h"
#include "openrouter_provider.h"

namespace ai {

std::unique_ptr<AIProvider> createProvider(const AISettings& settings) {
  if (settings.provider == "gemini") {
    return std::make_unique<GeminiProvider>(settings.gemini);
  }
  if (settings.provider == "openrouter") {
    return std::make_unique<OpenRouterProvider>(settings.openrouter);
  }
  if (settings.provider == "ollama") {
    return std::make_unique<OllamaProvider>(settings.ollama);
  }
  throw std::runtime_error("Unknown AI provider in settings.json: \"" + settings.provider +
                            "\" (expected gemini, openrouter, or ollama)");
}

}  // namespace ai
