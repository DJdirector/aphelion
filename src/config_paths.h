#pragma once

#include <filesystem>
#include <string>

namespace config_paths {

// Cross-platform home directory: $HOME on Linux/macOS, %USERPROFILE% on
// Windows. Falls back to the current directory (with a warning on stderr)
// if neither is set, so a missing env var degrades instead of crashing.
std::filesystem::path homeDir();

// ~/.aphelion - the root of all user-level Aphelion state.
std::filesystem::path configDir();

std::filesystem::path settingsFilePath();
std::filesystem::path historyDir();
std::filesystem::path themesDir();
std::filesystem::path thinkersDir();

// Creates ~/.aphelion and its subdirectories if missing, and seeds
// themesDir()/thinkersDir() with the built-in defaults (see
// default_assets.h) the first time each directory is created, so a fresh
// install works without depending on being run from the source tree.
void ensureConfigLayout();

}  // namespace config_paths
