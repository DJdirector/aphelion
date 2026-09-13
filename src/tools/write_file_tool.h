#pragma once

#include "tool.h"

namespace tools {

  // Creates a brand-new file with model-provided content. Deliberately
  // refuses to overwrite a file that already exists, since "create"
  // and "modify" have very different blast radii and shouldnt share
  // one code path.
  class WriteFileTool : public Tool {
    public:
      ai::ToolDefinition definition() const override;
      bool isDestructive() const override { return true; }
      std::string confirmationPreview(const ai::ToolCall& call) const override;
      ai::Message execute(const ai::ToolCall& call) const override;
  };

} // namespace tools
