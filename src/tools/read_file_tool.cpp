#include "read_file_tool.h"

#include <nlohmann/json.hpp>

#include "read.h"

namespace tools {

using json = nlohmann::json;

ai::ToolDefinition ReadFileTool::definition() const {
  ai::ToolDefinition def;
  def.name = "read_file";
  def.description =
      "Reads and returns the full text content of a single existing file. "
      "Cheaper than scan_project for a targeted follow-up once you already "
      "know which file you need - e.g. after seeing scan_project's file "
      "tree, or when the user names a specific file. Refuses binary files "
      "and caps very large files (truncated content is marked as such). "
      "Use scan_project instead for a project-wide overview.";
  def.parameters = json{
      {"type", "object"},
      {"properties",
       {
           {"path", json{{"type", "string"},
                         {"description", "Path of the file to read, relative to the project root."}}},
       }},
      {"required", json::array({"path"})},
  };
  return def;
}

ai::Message ReadFileTool::execute(const ai::ToolCall& call) const {
  ai::Message result;
  result.role = "tool";
  result.tool_call_id = call.id;
  result.tool_name = call.name;

  if (!call.arguments.is_object() || !call.arguments.contains("path") ||
      !call.arguments["path"].is_string()) {
    result.content = "Error: read_file requires a \"path\" string argument.";
    return result;
  }

  read::ReadOptions options;
  options.path = call.arguments["path"].get<std::string>();

  read::ReadResult read_result = read::readFile(options);
  if (!read_result.ok) {
    result.content = "Error: " + read_result.error;
    return result;
  }

  std::string content = read_result.content;
  if (read_result.truncated) {
    content += "\n... (truncated - file is " + std::to_string(read_result.file_size) +
               " bytes, showing the first " + std::to_string(content.size()) + ")";
  }
  result.content = content;
  return result;
}

}  // namespace tools
