#pragma once

#include <cstddef>
#include <string>

namespace read {

struct ReadOptions {
  std::string path;
  size_t max_bytes = 100 * 1024;  // same per-file cap scan_project uses
};

struct ReadResult {
  bool ok = false;
  std::string error;       // set when ok == false
  std::string content;     // file content, possibly truncated to max_bytes
  size_t file_size = 0;    // actual size on disk, even if content was truncated
  bool truncated = false;  // true if content is shorter than file_size
};

// Reads a single existing file as text. Refuses directories and files that
// look binary (NUL byte in the first few KB), and caps how much of a very
// large file is returned - callers can tell the model the file was
// truncated using file_size vs content.size().
ReadResult readFile(const ReadOptions& options);

}  // namespace read
