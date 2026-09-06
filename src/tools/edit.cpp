#include "edit.h"

#include <filesystem>
#include <fstream>
#include <sstream>

namespace edit {

namespace fs = std::filesystem;

namespace {

constexpr size_t kPreviewMaxBytes = 1500;

std::string truncateForPreview(const std::string& text) {
  if (text.size() <= kPreviewMaxBytes) return text;
  return text.substr(0, kPreviewMaxBytes) + "\n... (truncated, " +
         std::to_string(text.size() - kPreviewMaxBytes) + " more bytes)";
}

// Prefixes every line of `text` with `prefix`, git-diff style. Handles the
// empty-string case (new_str can legitimately be empty when deleting text)
// by still emitting one prefixed line so the removal is visible.
std::string prefixLines(const std::string& text, const std::string& prefix) {
  std::istringstream iss(text);
  std::ostringstream out;
  std::string line;
  bool any = false;
  while (std::getline(iss, line)) {
    out << prefix << line << "\n";
    any = true;
  }
  if (!any) {
    out << prefix << "\n";
  }
  return out.str();
}

// Resolves a model/user-provided path against the current working directory
// (same convention scan_project uses) into an absolute, normalized path, so
// previews and error messages always show exactly where the file actually
// is, regardless of how the path was phrased.
fs::path resolvePath(const std::string& raw_path) {
  return fs::weakly_canonical(fs::absolute(raw_path));
}

}  // namespace

EditResult computeEdit(const std::string& path, const std::string& old_str, const std::string& new_str) {
  EditResult result;

  if (old_str.empty()) {
    result.error = "old_str cannot be empty - there's nothing to match against.";
    return result;
  }

  fs::path resolved = resolvePath(path);

  if (!fs::exists(resolved)) {
    result.error =
        "File does not exist: " + resolved.string() + ". Use write_file to create a new file instead.";
    return result;
  }
  if (fs::is_directory(resolved)) {
    result.error = resolved.string() + " is a directory, not a file.";
    return result;
  }

  std::ifstream in(resolved, std::ios::binary);
  if (!in.is_open()) {
    result.error = "Failed to open " + resolved.string() + " for reading.";
    return result;
  }
  std::ostringstream ss;
  ss << in.rdbuf();
  std::string content = ss.str();
  result.old_content = content;

  size_t first = content.find(old_str);
  if (first == std::string::npos) {
    result.error = "old_str was not found in " + resolved.string() +
                    ". It must match the file's current content exactly, including whitespace.";
    return result;
  }
  size_t second = content.find(old_str, first + 1);
  if (second != std::string::npos) {
    result.error = "old_str appears more than once in " + resolved.string() +
                    " - it must be unique. Widen old_str with more surrounding context.";
    return result;
  }

  std::string new_content = content;
  new_content.replace(first, old_str.size(), new_str);
  result.new_content = new_content;

  std::ostringstream preview;
  preview << "File: " << resolved.string() << "\n\n";
  preview << prefixLines(truncateForPreview(old_str), "- ");
  preview << prefixLines(truncateForPreview(new_str), "+ ");
  result.diff_preview = preview.str();

  result.ok = true;
  return result;
}

bool writeEditResult(const std::string& path, const EditResult& result) {
  if (!result.ok) return false;
  fs::path resolved = resolvePath(path);
  std::ofstream out(resolved, std::ios::binary);
  if (!out.is_open()) return false;
  out << result.new_content;
  return true;
}

}  // namespace edit
