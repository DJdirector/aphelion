#include "run_command_tool.h"

#include <nlohmann/json.hpp>
#include <sstream>

#include "command.h"

namespace tools {

using json = nlohmann::json;

namespace {
// Fixed, not model-controllable - letting the model set its own timeout/cap
// would let it defeat the exact safety net these exist for.
constexpr int kTimeoutSeconds = 30;
constexpr size_t kMaxOutputBytes = 50 * 1024;
}  // namespace

ai::ToolDefinition RunCommandTool::definition() const {
  ai::ToolDefinition def;
  def.name = "run_command";
  def.description =
      "Executes a shell command and returns its combined stdout/stderr "
      "output and exit code. Runs with a fixed timeout (" +
      std::to_string(kTimeoutSeconds) +
      "s) and output cap - a command that runs too long or produces too "
      "much output is terminated. Requires explicit user confirmation "
      "before it runs, since a shell command can do anything a command "
      "typed into a terminal can do - reading, writing, or deleting files, "
      "installing software, etc. Use read_file/write_file/edit_file instead "
      "for simple file operations; only reach for this when you genuinely "
      "need to run a program (tests, builds, git, package managers, etc.).";
  def.parameters = json{
      {"type", "object"},
      {"properties",
       {
           {"command", json{{"type", "string"}, {"description", "The shell command to execute."}}},
       }},
      {"required", json::array({"command"})},
  };
  return def;
}

std::string RunCommandTool::confirmationPreview(const ai::ToolCall& call) const {
  std::string cmd = call.arguments.value("command", "");
  if (cmd.empty()) {
    return "(missing \"command\" argument)";
  }

  std::ostringstream out;
  out << "⚠⚠ THIS WILL RUN A SHELL COMMAND on your machine:\n\n";
  out << "  " << cmd << "\n\n";
  out << "This can do anything a command typed into your terminal can do -\n";
  out << "read, write, or delete files, install software, or change system state.\n";
  out << "Only approve this if you understand exactly what it does.";
  return out.str();
}

ai::Message RunCommandTool::execute(const ai::ToolCall& call) const {
  ai::Message result;
  result.role = "tool";
  result.tool_call_id = call.id;
  result.tool_name = call.name;

  if (!call.arguments.is_object() || !call.arguments.contains("command") ||
      !call.arguments["command"].is_string()) {
    result.content = "Error: run_command requires a \"command\" string argument.";
    return result;
  }

  command::CommandOptions options;
  options.command = call.arguments["command"].get<std::string>();
  options.timeout_seconds = kTimeoutSeconds;
  options.max_output_bytes = kMaxOutputBytes;

  command::CommandResult cmd_result = command::run(options);

  if (!cmd_result.ok) {
    result.content = "Error: " + cmd_result.setup_error;
    return result;
  }

  std::ostringstream out;
  out << "Exit code: " << cmd_result.exit_code << "\n";
  if (cmd_result.timed_out) {
    out << "(command timed out after " << kTimeoutSeconds << "s and was terminated)\n";
  }
  if (cmd_result.truncated) {
    out << "(output exceeded the size cap and was truncated; command was terminated)\n";
  }
  out << "--- output ---\n" << cmd_result.output;

  result.content = out.str();
  return result;
}

}  // namespace tools
