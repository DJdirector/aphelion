#include "scan_tool.h"

#include <nlohmann/json.hpp>

#include "scan.h"

namespace tools {

using json = nlohmann::json;

ai::ToolDefinition ScanProjectTool::definition() const {
  ai::ToolDefinition def;
  def.name = "scan_project";
  def.description =
      "Scans the local project directory the user is running Aphelion in and returns "
      "a text digest (file tree + file contents), similar to repomix. Use this when you "
      "need context about the codebase itself - e.g. the user asks about 'this project', "
      "'the codebase', or wants you to review or modify files you haven't seen yet. "
      "Automatically skips binary files, .git, build artifacts, and dependency directories. "
      "This is read-only - it never modifies anything.";
  def.parameters = json{
      {"type", "object"},
      {"properties",
       {
           {"path", json{{"type", "string"},
                         {"description",
                          "Relative path to scan. Defaults to the project root (\".\") if omitted."}}},
       }},
      {"required", json::array()},
  };
  return def;
}

ai::Message ScanProjectTool::execute(const ai::ToolCall& call) const {
  ai::Message result;
  result.role = "tool";
  result.tool_call_id = call.id;
  result.tool_name = call.name;

  scan::ScanOptions options;
  if (call.arguments.is_object() && call.arguments.contains("path") &&
      call.arguments["path"].is_string()) {
    options.root = call.arguments["path"].get<std::string>();
  }
  result.content = scan::scanProject(options).digest;
  return result;
}

}  // namespace tools
