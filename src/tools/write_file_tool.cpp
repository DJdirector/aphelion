#include "write_file_tool.h"

#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <sstream>

namespace tools {

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace {

// Resolves a model-provided path against the current working directory
// (same convention scan_project/edit_file use - "the project" is wherever
// Aphelion was launched from) into an absolute, normalized path, without
// requiring the path to exist yet (fs::canonical would throw for that).
// This is what gets shown in the confirmation preview, so the user always
// sees exactly where the file will land regardless of how the model
// phrased the path.
fs::path resolvePath(const std::string& raw_path) {
  return fs::weakly_canonical(fs::absolute(raw_path));
}

// Preview content is capped so a huge generated file doesn't flood the
// terminal before the user even gets to decide - the full content is still
// written verbatim on confirm, this only limits what's *shown*.
constexpr size_t kPreviewMaxBytes = 2000;

std::string previewContent(const std::string& content) {
  if (content.size() <= kPreviewMaxBytes) {
    return content;
  }
  return content.substr(0, kPreviewMaxBytes) + "\n... (truncated, " +
         std::to_string(content.size() - kPreviewMaxBytes) +
         " more bytes not shown - full content will be written)";
}

}  // namespace

ai::ToolDefinition WriteFileTool::definition() const {
  ai::ToolDefinition def;
  def.name = "write_file";
  def.description =
      "Creates a brand-new file with the given content. Only for files that "
      "don't exist yet - it will refuse if the target path already exists, "
      "rather than overwriting it (use edit_file to modify an existing "
      "file). Parent directories are created automatically if needed. "
      "Requires user confirmation before it runs.";
  def.parameters = json{
      {"type", "object"},
      {"properties",
       {
           {"path", json{{"type", "string"},
                         {"description", "Path of the new file, relative to the project root."}}},
           {"content", json{{"type", "string"}, {"description", "Full content to write to the new file."}}},
       }},
      {"required", json::array({"path", "content"})},
  };
  return def;
}

std::string WriteFileTool::confirmationPreview(const ai::ToolCall& call) const {
  std::string raw_path = call.arguments.value("path", "");
  std::string content = call.arguments.value("content", "");

  if (raw_path.empty()) {
    return "(missing \"path\" argument)";
  }

  fs::path resolved = resolvePath(raw_path);

  std::ostringstream out;
  if (fs::exists(resolved)) {
    out << "⚠ A file already exists at this path and will NOT be overwritten:\n  "
        << resolved.string() << "\n";
    return out.str();
  }

  out << "Create new file: " << resolved.string() << "\n";
  if (!resolved.parent_path().empty() && !fs::exists(resolved.parent_path())) {
    out << "(parent directory " << resolved.parent_path().string() << " will be created)\n";
  }
  out << "---\n" << previewContent(content) << "\n---";
  return out.str();
}

ai::Message WriteFileTool::execute(const ai::ToolCall& call) const {
  ai::Message result;
  result.role = "tool";
  result.tool_call_id = call.id;
  result.tool_name = call.name;

  if (!call.arguments.is_object() || !call.arguments.contains("path") ||
      !call.arguments["path"].is_string()) {
    result.content = "Error: write_file requires a \"path\" string argument.";
    return result;
  }
  if (!call.arguments.contains("content") || !call.arguments["content"].is_string()) {
    result.content = "Error: write_file requires a \"content\" string argument.";
    return result;
  }

  std::string raw_path = call.arguments["path"].get<std::string>();
  std::string content = call.arguments["content"].get<std::string>();
  fs::path resolved = resolvePath(raw_path);

  // Authoritative check - independent of whatever the preview said, since
  // the preview is purely informational and this is the actual gate.
  if (fs::exists(resolved)) {
    result.content = "Error: file already exists at " + resolved.string() +
                      ". write_file only creates new files - it won't overwrite an "
                      "existing one. Use edit_file to modify it, or ask the user how "
                      "they'd like to proceed.";
    return result;
  }

  std::error_code ec;
  if (!resolved.parent_path().empty()) {
    fs::create_directories(resolved.parent_path(), ec);
    if (ec) {
      result.content =
          "Error: failed to create parent directory " + resolved.parent_path().string() + ": " + ec.message();
      return result;
    }
  }

  std::ofstream out(resolved, std::ios::binary);
  if (!out.is_open()) {
    result.content = "Error: failed to open " + resolved.string() + " for writing.";
    return result;
  }
  out << content;
  out.close();

  result.content = "Created " + resolved.string() + " (" + std::to_string(content.size()) + " bytes).";
  return result;
}

}  // namespace tools
