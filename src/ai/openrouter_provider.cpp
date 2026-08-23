#include "openrouter_provider.h"

#include <nlohmann/json.hpp>

#include "http_client.h"

namespace ai {

using json = nlohmann::json;

OpenRouterProvider::OpenRouterProvider(ProviderConfig config)
    : config_(std::move(config)), api_key_(resolveApiKey(config_)) {}

AIResponse OpenRouterProvider::sendMessage(const std::vector<Message>& history,
                                            const std::vector<ToolDefinition>& tools) {
  AIResponse result;

  if (api_key_.empty()) {
    std::string env_name = config_.api_key_env.empty() ? "OPENROUTER_API_KEY" : config_.api_key_env;
    result.error = "Missing OpenRouter API key. Set ai.openrouter.api_key in settings.json, or export " +
                    env_name + ".";
    return result;
  }

  json messages = json::array();
  for (const auto& msg : history) {
    json m = {{"role", msg.role}};

    // Assistant turns that requested tool calls must echo those calls back in
    // OpenAI's tool_calls schema, or the API rejects the follow-up "tool" role
    // message for referencing a call id that was never declared.
    if (!msg.tool_calls.empty()) {
      m["content"] = msg.content.empty() ? nullptr : json(msg.content);
      json calls = json::array();
      for (const auto& tc : msg.tool_calls) {
        calls.push_back({
            {"id", tc.id},
            {"type", "function"},
            {"function", {{"name", tc.name}, {"arguments", tc.arguments.dump()}}},
        });
      }
      m["tool_calls"] = calls;
    } else {
      m["content"] = msg.content;
    }

    if (msg.role == "tool" && !msg.tool_call_id.empty()) {
      m["tool_call_id"] = msg.tool_call_id;
    }
    messages.push_back(m);
  }

  json body = {{"model", config_.model}, {"messages", messages}};

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

  std::string base = config_.base_url.empty() ? "https://openrouter.ai/api/v1" : config_.base_url;
  std::string url = base + "/chat/completions";

  HttpClient client;
  HttpResponse http_resp = client.post(url,
                                        {
                                            "Content-Type: application/json",
                                            "Authorization: Bearer " + api_key_,
                                            "HTTP-Referer: https://github.com/aphelion-cli",
                                            "X-Title: Aphelion",
                                        },
                                        body.dump());

  if (!http_resp.success) {
    std::string detail = http_resp.body;
    try {
      json err_json = json::parse(http_resp.body);
      if (err_json.contains("error")) {
        detail = err_json["error"].value("message", http_resp.body);
      }
    } catch (...) {
    }
    if (detail.empty()) detail = http_resp.error;
    result.error = "OpenRouter request failed (HTTP " + std::to_string(http_resp.status_code) +
                    "): " + detail;
    return result;
  }

  try {
    json resp = json::parse(http_resp.body);

    if (resp.contains("error")) {
      result.error = resp["error"].value("message", "Unknown OpenRouter API error");
      return result;
    }
    if (!resp.contains("choices") || resp["choices"].empty()) {
      result.error = "OpenRouter response contained no choices.";
      return result;
    }

    const auto& choice = resp["choices"][0];
    result.raw_finish_reason = choice.value("finish_reason", "");

    const auto& message = choice["message"];
    result.content = message.value("content", "");

    if (message.contains("tool_calls") && message["tool_calls"].is_array()) {
      for (const auto& tc : message["tool_calls"]) {
        ToolCall call;
        call.id = tc.value("id", "");
        if (tc.contains("function")) {
          call.name = tc["function"].value("name", "");
          std::string args_str = tc["function"].value("arguments", "{}");
          try {
            call.arguments = json::parse(args_str);
          } catch (...) {
            call.arguments = json::object();
          }
        }
        result.tool_calls.push_back(call);
      }
    }

    result.ok = true;
  } catch (const std::exception& e) {
    result.error = std::string("Failed to parse OpenRouter response: ") + e.what();
  }

  return result;
}

}  // namespace ai
