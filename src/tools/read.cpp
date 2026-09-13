#include "read.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace read {

namespace fs = std::filesystem;

namespace {

// Same convention scan_project/edit_file/write_file use - resolve against
// the current working directory into an absolute, normalized path, so error
// messages always show exactly which file was meant regardless of how the
// path was phrased.
fs::path resolvePath(const std::string& raw_path) {
  return fs::weakly_canonical(fs::absolute(raw_path));
}

// Cheap heuristic: a NUL byte in the first few KB is a strong binary signal.
// Same check scan.cpp uses for the same reason - duplicated rather than
// shared for now since it's four lines; worth factoring out into a common
// spot if a third consumer needs it.
bool looksBinary(const std::string& content) {
  size_t check_len = std::min<size_t>(content.size(), 8192);
  return content.substr(0, check_len).find('\0') != std::string::npos;
}

}  // namespace

ReadResult readFile(const ReadOptions& options) {
  ReadResult result;
  fs::path resolved = resolvePath(options.path);

  if (!fs::exists(resolved)) {
    result.error = "File does not exist: " + resolved.string();
    return result;
  }
  if (fs::is_directory(resolved)) {
    result.error = resolved.string() + " is a directory, not a file.";
    return result;
  }

  std::error_code ec;
  auto size = fs::file_size(resolved, ec);
  if (ec) {
    result.error = "Failed to stat " + resolved.string() + ": " + ec.message();
    return result;
  }
  result.file_size = size;

  std::ifstream in(resolved, std::ios::binary);
  if (!in.is_open()) {
    result.error = "Failed to open " + resolved.string() + " for reading.";
    return result;
  }

  std::ostringstream ss;
  ss << in.rdbuf();
  std::string full_content = ss.str();

  if (looksBinary(full_content)) {
    result.error = resolved.string() + " looks like a binary file - refusing to read it as text.";
    return result;
  }

  if (full_content.size() > options.max_bytes) {
    result.content = full_content.substr(0, options.max_bytes);
    result.truncated = true;
  } else {
    result.content = full_content;
  }

  result.ok = true;
  return result;
}

}  // namespace read
