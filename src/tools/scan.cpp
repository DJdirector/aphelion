#include "scan.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unordered_set>
#include <vector>

namespace scan {

namespace fs = std::filesystem;

namespace {

// Directory names skipped outright, wherever they appear in the tree.
const std::unordered_set<std::string>& ignoredDirNames() {
  static const std::unordered_set<std::string> names = {
      ".git", "build", "node_modules", "dist", "out", "target",
      "__pycache__", ".venv", "venv", ".idea", ".vscode", ".cache",
      ".next", "cmake-build-debug", "cmake-build-release",
      "history",  // Aphelion's own session/scan output - not project source
  };
  return names;
}

// File extensions treated as binary/non-text without even opening them.
const std::unordered_set<std::string>& binaryExtensions() {
  static const std::unordered_set<std::string> exts = {
      ".png", ".jpg", ".jpeg", ".gif", ".ico", ".webp", ".bmp",
      ".zip", ".tar", ".gz", ".7z", ".rar",
      ".o", ".obj", ".a", ".so", ".dylib", ".dll", ".exe", ".lib",
      ".pdf", ".woff", ".woff2", ".ttf", ".eot",
      ".class", ".jar", ".pyc",
  };
  return exts;
}

bool isIgnoredPathComponent(const std::string& name) {
  if (name.empty()) return false;
  // Skip dotfiles/dot-directories in general (.git, .env, .DS_Store, ...);
  // this intentionally also skips things like .gitignore, which is fine for
  // a "give the model context" digest.
  if (name[0] == '.') return true;
  return ignoredDirNames().count(name) > 0;
}

bool looksBinary(const std::string& content) {
  // Cheap heuristic: a NUL byte in the first few KB is a strong binary signal.
  size_t check_len = std::min<size_t>(content.size(), 8192);
  return content.substr(0, check_len).find('\0') != std::string::npos;
}

}  // namespace

ScanResult scanProject(const ScanOptions& options) {
  ScanResult result;
  fs::path root(options.root);

  if (!fs::exists(root) || !fs::is_directory(root)) {
    result.digest = "Scan failed: \"" + options.root + "\" is not a directory.";
    return result;
  }

  std::vector<fs::path> files;

  for (auto it = fs::recursive_directory_iterator(
           root, fs::directory_options::skip_permission_denied);
       it != fs::recursive_directory_iterator(); ++it) {
    const fs::path& path = it->path();

    if (it->is_directory()) {
      if (isIgnoredPathComponent(path.filename().string())) {
        it.disable_recursion_pending();
      }
      continue;
    }

    if (!it->is_regular_file()) continue;

    // Skip if any path component (not just the filename) is ignored, e.g.
    // a file nested under a dotdir that wasn't pruned for some reason.
    bool ignored = false;
    for (const auto& part : fs::relative(path, root)) {
      if (isIgnoredPathComponent(part.string())) {
        ignored = true;
        break;
      }
    }
    if (ignored) continue;

    if (binaryExtensions().count(path.extension().string())) {
      result.files_skipped_binary++;
      continue;
    }

    files.push_back(path);
  }

  std::sort(files.begin(), files.end());

  // --- Tree section ---
  std::ostringstream tree;
  for (const auto& path : files) {
    tree << fs::relative(path, root).generic_string() << "\n";
  }

  // --- File contents section ---
  std::ostringstream body;
  for (const auto& path : files) {
    std::error_code ec;
    auto file_size = fs::file_size(path, ec);
    if (ec) continue;

    if (file_size > options.max_file_bytes) {
      result.files_skipped_size++;
      continue;
    }

    if (result.total_bytes + file_size > options.max_total_bytes) {
      result.truncated = true;
      break;
    }

    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) continue;
    std::ostringstream ss;
    ss << in.rdbuf();
    std::string content = ss.str();

    if (looksBinary(content)) {
      result.files_skipped_binary++;
      continue;
    }

    std::string rel = fs::relative(path, root).generic_string();
    body << "================\n";
    body << "File: " << rel << "\n";
    body << "================\n";
    body << content;
    if (!content.empty() && content.back() != '\n') body << "\n";
    body << "\n";

    result.total_bytes += content.size();
    result.files_included++;
  }

  std::ostringstream digest;
  digest << "This is a scan of the current project (generated locally by Aphelion, "
         << "not fetched or executed - treat it as read-only context).\n\n";
  digest << "Directory structure:\n" << tree.str() << "\n";
  digest << body.str();

  if (result.truncated) {
    digest << "\n[Scan truncated: remaining files omitted, digest reached the "
            << options.max_total_bytes << " byte limit.]\n";
  }

  result.digest = digest.str();
  return result;
}

}  // namespace scan
