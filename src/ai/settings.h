#pragma once

#include <string>
#include <nlohmann/json.hpp>

namespace ai {

// Per-provider configuration. Not every field is used by every provider
// (e.g. Ollama ignores api_key/api_key_env, Gemini/OpenRouter ignore base_url
// unless the user wants to point at a proxy or gateway).
struct ProviderConfig {
  std::string model;
  std::string api_key;      // literal key stored in settings.json (optional, discouraged)
  std::string api_key_env;  // name of an env var to read the key from instead
  std::string base_url;     // API base URL; providers fall back to their default if empty
};

// Root AI configuration, stored under the "ai" key in settings.json.
struct AISettings {
  std::string provider = "openrouter";  // "gemini" | "openrouter" | "ollama"

  ProviderConfig gemini{
      "gemini-2.0-flash", "", "GEMINI_API_KEY",
      "https://generativelanguage.googleapis.com"};

  ProviderConfig openrouter{
      "openrouter/auto", "", "OPENROUTER_API_KEY",
      "https://openrouter.ai/api/v1"};

  ProviderConfig ollama{
      "llama3.1", "", "",
      "http://localhost:11434"};
};

// Resolves the actual API key to use for a provider: prefers a literal key
// in settings.json, falls back to reading api_key_env from the environment.
// Returns an empty string if neither is set/available.
std::string resolveApiKey(const ProviderConfig& cfg);

// settings.json <-> AISettings conversion. Missing fields fall back to the
// AISettings/ProviderConfig defaults above, so partial "ai" objects in an
// existing settings.json won't break loading.
AISettings aiSettingsFromJson(const nlohmann::json& j);
nlohmann::json aiSettingsToJson(const AISettings& settings);

}  // namespace ai
