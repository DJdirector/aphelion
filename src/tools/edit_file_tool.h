#pragma once

#include "tool.h"

namespace tools {

// Makes a targeted edit to an EXISTING file by replacing one unique
// occurrence of old_str with new_str (see edit.h). Deliberately separate
// from write_file - editing requires the file to already exist and old_str
// to match unambiguously, neither of which apply to creating a new file.
class EditFileTool : public Tool {
 public:
  ai::ToolDefinition definition() const override;
  bool isDestructive() const override { return true; }
  std::string confirmationPreview(const ai::ToolCall& call) const override;
  ai::Message execute(const ai::ToolCall& call) const override;
};

}  // namespace tools
