#pragma once

#include <cstddef>
#include <string>

namespace command {

struct CommandOptions {
  std::string command;
  int timeout_seconds = 30;
  size_t max_output_bytes = 50 * 1024;
};

struct CommandResult {
  bool ok = false;              // false ONLY for infrastructure failures (bad
                                 // input, fork/pipe failure) - a command that
                                 // ran and exited nonzero, timed out, or hit
                                 // the output cap still has ok == true, since
                                 // those are outcomes the model should see,
                                 // not tool-level errors.
  std::string setup_error;      // set when ok == false
  int exit_code = -1;           // -N if killed by signal N; meaningless if timed_out
  std::string output;           // combined stdout+stderr, possibly truncated
  bool timed_out = false;
  bool truncated = false;
};

// Runs `options.command` via "/bin/sh -c", capturing combined stdout+stderr.
// The command (and anything it spawns) is killed if it runs longer than
// timeout_seconds or produces more than max_output_bytes of output - either
// condition sets the corresponding flag rather than being treated as a
// tool-level error.
CommandResult run(const CommandOptions& options);

}  // namespace command
