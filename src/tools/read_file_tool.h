#pragma once

#include "tool.h"

namespace tools {

// Reads a single existing file's content. Read-only - isDestructive()
// correctly defaults to false, so no confirmation gate applies, unlike
// edit_file/write_file. Cheaper than scan_project for a targeted follow-up
// once the model already knows which file it needs.
class ReadFileTool : public Tool {
 public:
  ai::ToolDefinition definition() const override;
  ai::Message execute(const ai::ToolCall& call) const override;
};

}  // namespace tools
