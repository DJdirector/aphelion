#include "registry.h"

#include <memory>

#include "scan_tool.h"
#include "tool.h"

namespace tools {

namespace {

// The single list of tools available to the AI. Adding a new tool means:
// implement Tool in its own file (see scan_tool.h/.cpp for the pattern),
// then add one line here - nothing else in this file changes. Mirrors how
// provider_factory.cpp only needs one new branch per AI provider.
const std::vector<std::unique_ptr<Tool>>& toolInstances() {
  static const std::vector<std::unique_ptr<Tool>> instances = [] {
    std::vector<std::unique_ptr<Tool>> v;
    v.push_back(std::make_unique<ScanProjectTool>());
    return v;
  }();
  return instances;
}

}  // namespace

std::vector<ai::ToolDefinition> availableTools() {
  std::vector<ai::ToolDefinition> defs;
  for (const auto& tool : toolInstances()) {
    defs.push_back(tool->definition());
  }
  return defs;
}

ai::Message executeToolCall(const ai::ToolCall& call) {
  for (const auto& tool : toolInstances()) {
    if (tool->definition().name == call.name) {
      return tool->execute(call);
    }
  }

  ai::Message result;
  result.role = "tool";
  result.tool_call_id = call.id;
  result.tool_name = call.name;
  result.content = "Error: unknown tool \"" + call.name + "\".";
  return result;
}

}  // namespace tools
