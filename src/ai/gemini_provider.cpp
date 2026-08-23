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

  // Consecutive "tool" messages (results of multiple calls the model made in
  // one turn) must be batched into a single "user" turn with multiple
  // functionResponse parts - Gemini rejects two consecutive same-role turns,
  // which is exactly what you'd get sending one turn per tool result.
  json pending_tool_parts = json::array();
  auto flushToolResponses = [&]() {
    if (!pending_tool_parts.empty()) {
      contents.push_back({{"role", "user"}, {"parts", pending_tool_parts}});
      pending_tool_parts = json::array();
    }
  };

  for (const auto& msg : history) {
    if (msg.role == "system") {
      // Gemini takes at most one system instruction; concatenate if there's more than one.
      if (!system_prompt.empty()) system_prompt += "\n";
      system_prompt += msg.content;
      continue;
    }

    if (msg.role == "tool") {
      json response_part = {
          {"functionResponse",
           {
               {"name", msg.tool_name.empty() ? "tool" : msg.tool_name},
               {"response", {{"content", msg.content}}},
           }},
      };
      if (!msg.tool_call_id.empty()) {
        response_part["functionResponse"]["id"] = msg.tool_call_id;
      }
      pending_tool_parts.push_back(response_part);
      continue;
    }

    flushToolResponses();

    if (msg.role == "assistant" && !msg.tool_calls.empty()) {
      json parts = json::array();
      if (!msg.content.empty()) {
        parts.push_back({{"text", msg.content}});
      }
      for (const auto& tc : msg.tool_calls) {
        json call_part = {{"functionCall", {{"name", tc.name}, {"args", tc.arguments}}}};
        if (!tc.id.empty()) {
          call_part["functionCall"]["id"] = tc.id;
        }
        // thoughtSignature is a SIBLING of functionCall on the Part object, not
        // nested inside it, and must be echoed back byte-for-byte or Gemini 3+
        // rejects the request with a 400. Only present on the first call when
        // the model made several in parallel. Wire key is camelCase
        // "thoughtSignature" - the current ai.google.dev docs use this; an
        // older "(Legacy)" page shows snake_case "thought_signature", which
        // does NOT work against the live generateContent endpoint.
        if (!tc.thought_signature.empty()) {
          call_part["thoughtSignature"] = tc.thought_signature;
        }
        parts.push_back(call_part);
      }
      contents.push_back({{"role", "model"}, {"parts", parts}});
      continue;
    }

    contents.push_back({
        {"role", mapRoleToGemini(msg.role)},
        {"parts", json::array({json{{"text", msg.content}}})},
    });
  }
  flushToolResponses();

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
    // The response body (when present) has the real diagnostic - a transport-level
    // failure (no connection, timeout) has an empty body and only http_resp.error
    // to go on, but an HTTP-level failure (4xx/5xx) has a JSON body explaining
    // exactly what was wrong with the request, which is far more useful than the
    // generic "HTTP 400" status string.
    std::string detail = http_resp.body;
    try {
      json err_json = json::parse(http_resp.body);
      if (err_json.contains("error")) {
        detail = err_json["error"].value("message", http_resp.body);
      }
    } catch (...) {
      // Body wasn't JSON (or was empty) - fall through with whatever we have.
    }
    if (detail.empty()) detail = http_resp.error;
    result.error = "Gemini request failed (HTTP " + std::to_string(http_resp.status_code) +
                    "): " + detail;
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
          call.id = part["functionCall"].value("id", "");
          call.name = part["functionCall"].value("name", "");
          call.arguments = part["functionCall"].value("args", json::object());
          // Sibling of functionCall on the Part object, not nested inside it.
          // Wire key is camelCase "thoughtSignature" (current API), not the
          // snake_case "thought_signature" shown on Google's legacy docs page.
          call.thought_signature = part.value("thoughtSignature", "");
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
