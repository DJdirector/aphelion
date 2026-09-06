#include "registry.h"

#include <memory>

#include "scan_tool.h"
#include "edit_file_tool.h"
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
    v.push_back(std::make_unique<EditFileTool>());
    return v;
  }();
  return instances;
}

const Tool* findTool(const std::string& name) {
  for (const auto& tool : toolInstances()) {
    if (tool->definition().name == name) {
      return tool.get();
    }
  }
  return nullptr;
}

}  // namespace

std::vector<ai::ToolDefinition> availableTools() {
  std::vector<ai::ToolDefinition> defs;
  for (const auto& tool : toolInstances()) {
    defs.push_back(tool->definition());
  }
  return defs;
}

bool isDestructive(const std::string& tool_name) {
  const Tool* tool = findTool(tool_name);
  return tool ? tool->isDestructive() : false;
}

std::string getConfirmationPreview(const ai::ToolCall& call) {
  const Tool* tool = findTool(call.name);
  return tool ? tool->confirmationPreview(call) : "";
}

ai::Message executeToolCall(const ai::ToolCall& call) {
  if (const Tool* tool = findTool(call.name)) {
    return tool->execute(call);
  }

  ai::Message result;
  result.role = "tool";
  result.tool_call_id = call.id;
  result.tool_name = call.name;
  result.content = "Error: unknown tool \"" + call.name + "\".";
  return result;
}

}  // namespace tools
