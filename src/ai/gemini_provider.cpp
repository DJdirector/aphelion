#include "gemini_provider.h"

#include <nlohmann/json.hpp>

#include "http_client.h"

namespace ai {

using json = nlohmann::json;

namespace {

// Gemini uses "user" / "model" instead of "user" / "assistant", and has no
// "system" role in `contents` (system prompts go in a separate field).
std::string mapRoleToGemini(const std::string& role) {
  if (role == "assistant") return "model";
  return "user";
}

}  // namespace

GeminiProvider::GeminiProvider(ProviderConfig config)
    : config_(std::move(config)), api_key_(resolveApiKey(config_)) {}

AIResponse GeminiProvider::sendMessage(const std::vector<Message>& history,
                                        const std::vector<ToolDefinition>& tools) {
  AIResponse result;

  if (api_key_.empty()) {
    std::string env_name = config_.api_key_env.empty() ? "GEMINI_API_KEY" : config_.api_key_env;
    result.error = "Missing Gemini API key. Set ai.gemini.api_key in settings.json, or export " +
                    env_name + ".";
    return result;
  }

  json contents = json::array();
  std::string system_prompt;

  for (const auto& msg : history) {
    if (msg.role == "system") {
      // Gemini takes at most one system instruction; concatenate if there's more than one.
      if (!system_prompt.empty()) system_prompt += "\n";
      system_prompt += msg.content;
      continue;
    }
    contents.push_back({
        {"role", mapRoleToGemini(msg.role)},
        {"parts", json::array({json{{"text", msg.content}}})},
    });
  }

  json body = {{"contents", contents}};

  if (!system_prompt.empty()) {
    body["systemInstruction"] = {{"parts", json::array({json{{"text", system_prompt}}})}};
  }

  if (!tools.empty()) {
    json declarations = json::array();
    for (const auto& tool : tools) {
      declarations.push_back({
          {"name", tool.name},
          {"description", tool.description},
          {"parameters", tool.parameters},
      });
    }
    body["tools"] = json::array({json{{"functionDeclarations", declarations}}});
  }

  std::string base = config_.base_url.empty()
                          ? "https://generativelanguage.googleapis.com"
                          : config_.base_url;
  std::string url = base + "/v1beta/models/" + config_.model + ":generateContent";

  HttpClient client;
  HttpResponse http_resp = client.post(url,
                                        {
                                            "Content-Type: application/json",
                                            "x-goog-api-key: " + api_key_,
                                        },
                                        body.dump());

  if (!http_resp.success) {
    result.error = "Gemini request failed: " +
                    (http_resp.error.empty() ? http_resp.body : http_resp.error);
    return result;
  }

  try {
    json resp = json::parse(http_resp.body);

    if (resp.contains("error")) {
      result.error = resp["error"].value("message", "Unknown Gemini API error");
      return result;
    }
    if (!resp.contains("candidates") || resp["candidates"].empty()) {
      result.error = "Gemini response contained no candidates.";
      return result;
    }

    const auto& candidate = resp["candidates"][0];
    result.raw_finish_reason = candidate.value("finishReason", "");

    if (candidate.contains("content") && candidate["content"].contains("parts")) {
      for (const auto& part : candidate["content"]["parts"]) {
        if (part.contains("text")) {
          result.content += part["text"].get<std::string>();
        } else if (part.contains("functionCall")) {
          ToolCall call;
          call.name = part["functionCall"].value("name", "");
          call.arguments = part["functionCall"].value("args", json::object());
          result.tool_calls.push_back(call);
        }
      }
    }

    result.ok = true;
  } catch (const std::exception& e) {
    result.error = std::string("Failed to parse Gemini response: ") + e.what();
  }

  return result;
}

}  // namespace ai
