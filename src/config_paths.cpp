#include "config_paths.h"

#include <cstdlib>
#include <fstream>
#include <iostream>

#include "default_assets.h"

namespace config_paths {

namespace fs = std::filesystem;

fs::path homeDir() {
#if defined(_WIN32)
  const char* home = std::getenv("USERPROFILE");
  const char* env_name = "%USERPROFILE%";
#else
  const char* home = std::getenv("HOME");
  const char* env_name = "$HOME";
#endif
  if (!home || std::string(home).empty()) {
    std::cerr << "Warning: could not resolve home directory (" << env_name
              << " is not set) - falling back to the current directory.\n";
    return fs::current_path();
  }
  return fs::path(home);
}

fs::path configDir() { return homeDir() / ".aphelion"; }

fs::path settingsFilePath() { return configDir() / "settings.json"; }
fs::path historyDir() { return configDir() / "history"; }
fs::path themesDir() { return configDir() / "themes"; }
fs::path thinkersDir() { return configDir() / "thinkers"; }

namespace {

void writeIfMissing(const fs::path& path, const std::string& content) {
  if (fs::exists(path)) return;
  std::ofstream out(path, std::ios::binary);
  if (out.is_open()) out << content;
}

}  // namespace

void ensureConfigLayout() {
  fs::create_directories(configDir());
  fs::create_directories(historyDir());

  // Only seed themes/thinkers the first time each directory itself is
  // created - if the directory already existed (even empty, e.g. the user
  // deleted every theme on purpose), we don't fight them by re-seeding.
  bool themes_dir_existed = fs::exists(themesDir());
  fs::create_directories(themesDir());
  if (!themes_dir_existed) {
    writeIfMissing(themesDir() / "aphelion-dark.json", default_assets::kThemeAphelionDark);
    writeIfMissing(themesDir() / "tokyo-night.json", default_assets::kThemeTokyoNight);
    writeIfMissing(themesDir() / "gruvbox-material-soft-dark.json",
                    default_assets::kThemeGruvboxMaterialSoftDark);
  }

  bool thinkers_dir_existed = fs::exists(thinkersDir());
  fs::create_directories(thinkersDir());
  if (!thinkers_dir_existed) {
    writeIfMissing(thinkersDir() / "default.json", default_assets::kThinkerDefault);
    writeIfMissing(thinkersDir() / "bar.json", default_assets::kThinkerBar);
    writeIfMissing(thinkersDir() / "spinner.json", default_assets::kThinkerSpinner);
    writeIfMissing(thinkersDir() / "swipe-runner.json", default_assets::kThinkerSwipeRunner);
  }
}

}  // namespace config_paths
