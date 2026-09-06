#include "edit_file_tool.h"

#include <nlohmann/json.hpp>

#include "edit.h"

namespace tools {

using json = nlohmann::json;

ai::ToolDefinition EditFileTool::definition() const {
  ai::ToolDefinition def;
  def.name = "edit_file";
  def.description =
      "Makes a targeted edit to an EXISTING file by replacing one exact "
      "occurrence of old_str with new_str. old_str must match the file's "
      "current content exactly, including whitespace, and must appear in "
      "exactly one place - if it matches zero or multiple times, widen "
      "old_str with more surrounding context until it's unique. Use "
      "write_file instead to create a new file. Requires user confirmation "
      "before it runs.";
  def.parameters = json{
      {"type", "object"},
      {"properties",
       {
           {"path", json{{"type", "string"},
                         {"description", "Path of the existing file to edit, relative to the project root."}}},
           {"old_str", json{{"type", "string"},
                            {"description",
                             "Exact text to replace. Must match the file's current content in exactly one place."}}},
           {"new_str", json{{"type", "string"}, {"description", "Replacement text."}}},
       }},
      {"required", json::array({"path", "old_str", "new_str"})},
  };
  return def;
}

std::string EditFileTool::confirmationPreview(const ai::ToolCall& call) const {
  std::string path = call.arguments.value("path", "");
  std::string old_str = call.arguments.value("old_str", "");
  std::string new_str = call.arguments.value("new_str", "");

  if (path.empty()) {
    return "(missing \"path\" argument)";
  }

  edit::EditResult result = edit::computeEdit(path, old_str, new_str);
  if (!result.ok) {
    return "⚠ " + result.error;
  }
  return result.diff_preview;
}

ai::Message EditFileTool::execute(const ai::ToolCall& call) const {
  ai::Message result;
  result.role = "tool";
  result.tool_call_id = call.id;
  result.tool_name = call.name;

  if (!call.arguments.is_object() || !call.arguments.contains("path") || !call.arguments["path"].is_string()) {
    result.content = "Error: edit_file requires a \"path\" string argument.";
    return result;
  }
  if (!call.arguments.contains("old_str") || !call.arguments["old_str"].is_string()) {
    result.content = "Error: edit_file requires an \"old_str\" string argument.";
    return result;
  }
  if (!call.arguments.contains("new_str") || !call.arguments["new_str"].is_string()) {
    result.content = "Error: edit_file requires a \"new_str\" string argument.";
    return result;
  }

  std::string path = call.arguments["path"].get<std::string>();
  std::string old_str = call.arguments["old_str"].get<std::string>();
  std::string new_str = call.arguments["new_str"].get<std::string>();

  // Recompute fresh rather than trusting the preview shown earlier - the
  // file could have changed between when the preview was generated and now
  // (the user had a chance to look at it, think, maybe edit the file
  // themselves in another window, before answering the confirmation prompt).
  edit::EditResult edit_result = edit::computeEdit(path, old_str, new_str);
  if (!edit_result.ok) {
    result.content = "Error: " + edit_result.error;
    return result;
  }

  if (!edit::writeEditResult(path, edit_result)) {
    result.content = "Error: failed to write changes to " + path;
    return result;
  }

  result.content = "Edited " + path + " successfully.";
  return result;
}

}  // namespace tools
