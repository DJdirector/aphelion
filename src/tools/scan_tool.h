#pragma once

#include "tool.h"

namespace tools {

// Thin Tool adapter around scan::scanProject (scan.h/scan.cpp) - the actual
// scanning logic knows nothing about the Tool interface, so it stays usable
// standalone (e.g. by the human-typed /scan command in main.cpp) without
// pulling in tool-calling machinery.
class ScanProjectTool : public Tool {
 public:
  ai::ToolDefinition definition() const override;
  ai::Message execute(const ai::ToolCall& call) const override;
};

}  // namespace tools
