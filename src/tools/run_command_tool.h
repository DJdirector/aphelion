#pragma once

#include "tool.h"

namespace tools {

// Executes a shell command via command::run (see command.h). By far the
// widest blast radius of any tool here - a shell command isn't limited to
// files, unlike edit_file/write_file. isDestructive() is true and the
// confirmation preview is deliberately more alarming than the file tools',
// per the issue's explicit ask for "an even louder" confirmation.
class RunCommandTool : public Tool {
 public:
  ai::ToolDefinition definition() const override;
  bool isDestructive() const override { return true; }
  std::string confirmationPreview(const ai::ToolCall& call) const override;
  ai::Message execute(const ai::ToolCall& call) const override;
};

}  // namespace tools
