#pragma once

#include <string>

namespace edit {

struct EditResult {
  bool ok = false;
  std::string error;         // set when ok == false
  std::string old_content;   // full original file content
  std::string new_content;   // full content after the edit (only valid if ok)
  std::string diff_preview;  // human-readable before/after snippet for confirmation
};

// Reads `path`, finds exactly one occurrence of old_str in its content, and
// computes what the file would look like with it replaced by new_str.
// Does NOT write anything to disk - purely computes the result so it can be
// shown to the user before they approve. Fails if the file doesn't exist,
// old_str appears zero times (nothing to change), or more than once
// (ambiguous - which occurrence should it replace?).
EditResult computeEdit(const std::string& path, const std::string& old_str, const std::string& new_str);

// Writes result.new_content to `path`. Only call this after computeEdit()
// returned ok == true and the user has confirmed - this function itself
// performs no validation or gating, it just writes.
bool writeEditResult(const std::string& path, const EditResult& result);

}  // namespace edit
