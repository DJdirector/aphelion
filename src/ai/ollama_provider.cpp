#include "ollama_provider.h"

#include <nlohmann/json.hpp>

#include "http_client.h"

namespace ai {

using json = nlohmann::json;

OllamaProvider::OllamaProvider(ProviderConfig config) : config_(std::move(config)) {}

AIResponse OllamaProvider::sendMessage(const std::vector<Message>& history,
                                        const std::vector<ToolDefinition>& tools) {
  AIResponse result;

  json messages = json::array();
  for (const auto& msg : history) {
    json m = {{"role", msg.role}, {"content", msg.content}};

    if (!msg.tool_calls.empty()) {
      json calls = json::array();
      for (const auto& tc : msg.tool_calls) {
        calls.push_back({{"function", {{"name", tc.name}, {"arguments", tc.arguments}}}});
      }
      m["tool_calls"] = calls;
    }

    messages.push_back(m);
  }

  json body = {{"model", config_.model}, {"messages", messages}, {"stream", false}};

  if (!tools.empty()) {
    json tool_defs = json::array();
    for (const auto& tool : tools) {
      tool_defs.push_back({
          {"type", "function"},
          {"function",
           {
               {"name", tool.name},
               {"description", tool.description},
               {"parameters", tool.parameters},
           }},
      });
    }
    body["tools"] = tool_defs;
  }

  std::string base = config_.base_url.empty() ? "http://localhost:11434" : config_.base_url;
  std::string url = base + "/api/chat";

  HttpClient client;
  // Local models can be slow to load/run, especially on the first call, so give
  // Ollama more rope than the hosted providers.
  HttpResponse http_resp = client.post(url, {"Content-Type: application/json"}, body.dump(), 120);

  if (!http_resp.success) {
    std::string detail = http_resp.body;
    try {
      json err_json = json::parse(http_resp.body);
      if (err_json.contains("error")) {
        detail = err_json.value("error", http_resp.body);  // Ollama uses a flat string, not error.message
      }
    } catch (...) {
    }
    if (detail.empty()) detail = http_resp.error;
    result.error = "Ollama request failed (HTTP " + std::to_string(http_resp.status_code) +
                    "): " + detail + ". Is Ollama running at " + base + "?";
    return result;
  }

  try {
    json resp = json::parse(http_resp.body);

    if (resp.contains("error")) {
      result.error = resp.value("error", "Unknown Ollama error");
      return result;
    }
    if (!resp.contains("message")) {
      result.error = "Ollama response did not contain a message.";
      return result;
    }

    const auto& message = resp["message"];
    result.content = message.value("content", "");

    if (message.contains("tool_calls") && message["tool_calls"].is_array()) {
      for (const auto& tc : message["tool_calls"]) {
        ToolCall call;
        if (tc.contains("function")) {
          call.name = tc["function"].value("name", "");
          call.arguments = tc["function"].value("arguments", json::object());
        }
        result.tool_calls.push_back(call);
      }
    }

    result.ok = true;
  } catch (const std::exception& e) {
    result.error = std::string("Failed to parse Ollama response: ") + e.what();
  }

  return result;
}

}  // namespace ai
