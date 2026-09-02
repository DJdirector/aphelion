#pragma once

#include <string>

// Minimal, best-effort logging to ~/.aphelion/logs/. Never throws - a
// logging failure should never be the reason Aphelion crashes. One log file
// per calendar day, which "rotates" naturally with zero cleanup logic
// needed (old files just sit there; pruning old logs is a separate concern
// if it ever becomes one).
namespace logger {

enum class Level { Info, Warn, Error };

// category is a short machine-friendly tag (e.g. "provider_setup",
// "tool_execution", "settings_load") so a log file can be grepped by
// failure type when triaging a report from someone else's machine.
void log(Level level, const std::string& category, const std::string& message);

inline void info(const std::string& category, const std::string& message) {
  log(Level::Info, category, message);
}
inline void warn(const std::string& category, const std::string& message) {
  log(Level::Warn, category, message);
}
inline void error(const std::string& category, const std::string& message) {
  log(Level::Error, category, message);
}

}  // namespace logger
