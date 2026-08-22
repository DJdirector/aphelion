#include "settings.h"

#include <cstdlib>

namespace ai {

using json = nlohmann::json;

std::string resolveApiKey(const ProviderConfig& cfg) {
  if (!cfg.api_key.empty()) {
    return cfg.api_key;
  }
  if (!cfg.api_key_env.empty()) {
    if (const char* val = std::getenv(cfg.api_key_env.c_str())) {
      return std::string(val);
    }
  }
  return "";
}

namespace {

ProviderConfig providerConfigFromJson(const json& j, const ProviderConfig& fallback) {
  ProviderConfig cfg = fallback;
  if (!j.is_object()) return cfg;
  cfg.model = j.value("model", cfg.model);
  cfg.api_key = j.value("api_key", cfg.api_key);
  cfg.api_key_env = j.value("api_key_env", cfg.api_key_env);
  cfg.base_url = j.value("base_url", cfg.base_url);
  return cfg;
}

json providerConfigToJson(const ProviderConfig& cfg) {
  json j = {
      {"model", cfg.model},
      {"api_key_env", cfg.api_key_env},
      {"base_url", cfg.base_url},
  };
  // Only persist a literal api_key if one was actually set, so a freshly
  // generated settings.json doesn't invite people to paste secrets into it.
  if (!cfg.api_key.empty()) {
    j["api_key"] = cfg.api_key;
  }
  return j;
}

}  // namespace

AISettings aiSettingsFromJson(const json& j) {
  AISettings settings;  // defaults
  if (!j.is_object()) return settings;

  settings.provider = j.value("provider", settings.provider);

  if (j.contains("gemini")) {
    settings.gemini = providerConfigFromJson(j["gemini"], settings.gemini);
  }
  if (j.contains("openrouter")) {
    settings.openrouter = providerConfigFromJson(j["openrouter"], settings.openrouter);
  }
  if (j.contains("ollama")) {
    settings.ollama = providerConfigFromJson(j["ollama"], settings.ollama);
  }

  return settings;
}

json aiSettingsToJson(const AISettings& settings) {
  return json{
      {"provider", settings.provider},
      {"gemini", providerConfigToJson(settings.gemini)},
      {"openrouter", providerConfigToJson(settings.openrouter)},
      {"ollama", providerConfigToJson(settings.ollama)},
  };
}

}  // namespace ai
