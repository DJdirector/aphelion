#pragma once

#include <string>
#include <cstddef>

namespace scan {

// Tunables for scanProject(). Defaults are conservative enough to stay
// well under a free-tier context/rate limit for a mid-sized project, while
// still giving the model something genuinely useful.
struct ScanOptions {
  std::string root = ".";
  size_t max_file_bytes = 100 * 1024;    // skip individual files larger than this
  size_t max_total_bytes = 400 * 1024;   // stop adding files once the digest hits this
};

struct ScanResult {
  std::string digest;          // repomix-style text: tree + per-file contents
  size_t files_included = 0;
  size_t files_skipped_size = 0;   // skipped for being too large
  size_t files_skipped_binary = 0; // skipped for looking non-text
  size_t total_bytes = 0;          // bytes of file content actually included
  bool truncated = false;          // true if max_total_bytes was hit before scanning finished
};

// Walks `options.root`, skipping common junk directories (.git, build,
// node_modules, etc.) and files that look binary, and produces a single
// text digest: a flat file tree followed by each included file's contents
// in a fenced section. Purely local - no network, no AI calls.
ScanResult scanProject(const ScanOptions& options = {});

}  // namespace scan
