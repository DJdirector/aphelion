#include <iostream>
#include <fstream>
#include <string>
#include <iomanip>
#include <stdexcept>
#include <filesystem>
#include <nlohmann/json.hpp>

using json = nlohmann::json;
namespace fs = std::filesystem;

struct SETTINGS {
  bool first_run;
  std::string theme;
};

struct THEME {
  std::string background;
  std::string foreground;
  std::string cursor;
  std::string accent;
  std::string alt_background;
  std::string red;
  std::string green;
  std::string yellow;
  std::string orange;
  std::string blue;
  std::string magenta;
};

void clearScreen() {
  std::cout << "\033[2J\033[H" << std::flush;
}

// Convert hex into ANSI colored text
void printHex(const std::string& hex, const std::string& text) {
  std::string clean_hex = hex;
  if (!clean_hex.empty() && clean_hex[0] == '#') {
    clean_hex.erase(0, 1);
  }

  if (clean_hex.length() < 6) {
    std::cout << text;
    return;
  }

  int r = std::stoi(clean_hex.substr(0, 2), nullptr, 16);
  int g = std::stoi(clean_hex.substr(2, 2), nullptr, 16);
  int b = std::stoi(clean_hex.substr(4, 2), nullptr, 16);

  std::cout << "\033[38;2;" << r << ";" << g << ";" << b << "m"
            << text
            << "\033[0m";
}

SETTINGS loadSettings(const std::string& file_path = "settings.json") {
  if (!fs::exists(file_path)) {
    json default_config = {
      {"first_run", true},
      {"theme", "aphelion-dark"}
    };
    std::ofstream out_file(file_path);
    out_file << default_config.dump(4);
    return { true, "aphelion-dark" };
  }

  std::ifstream file(file_path);
  if (!file.is_open()) {
    throw std::runtime_error("Failed to open settings file at: " + file_path);
  }

  json settings_data;
  file >> settings_data;

  SETTINGS settings;
  settings.first_run = settings_data.value("first_run", true);
  settings.theme = settings_data.value("theme", "gruvbox-material-soft-dark");

  return settings;
}

THEME loadTheme(const std::string& theme_name) {
  // Automatically map theme_name to themes/<theme_name>.json
  std::string file_path = "themes/" + theme_name + ".json";

  std::ifstream file(file_path);
  if (!file.is_open()) {
    throw std::runtime_error("Failed to open theme file at: " + file_path);
  }

  json theme_data;
  file >> theme_data;

  THEME theme;
  theme.background     = theme_data.value("background", "#000000");
  theme.foreground     = theme_data.value("foreground", "#ffffff");
  theme.cursor         = theme_data.value("cursor", "#ffffff");
  theme.accent         = theme_data.value("accent", "#ffffff");
  theme.alt_background = theme_data.value("alt_background", "#000000");
  theme.red            = theme_data.value("red", "#ff0000");
  theme.green          = theme_data.value("green", "#00ff00");
  theme.yellow         = theme_data.value("yellow", "#ffff00");
  theme.orange         = theme_data.value("orange", "#ffa500");
  theme.blue           = theme_data.value("blue", "#0000ff");
  theme.magenta        = theme_data.value("magenta", "#ff00ff");

  return theme;
}

int main() {
  try {
    // 1. Read app configuration
    SETTINGS settings = loadSettings("settings.json");

    // 2. Read selected theme based on settings
    THEME current_theme = loadTheme(settings.theme);

    clearScreen();

    if (settings.first_run) {
      printHex(current_theme.accent, "First run detected.\n\n");
    }

    // Output current theme details
    printHex(current_theme.background, "Background\n");
    printHex(current_theme.foreground, "Foreground\n");
    printHex(current_theme.cursor, "Cursor\n");
    printHex(current_theme.accent, "Accent\n");
    printHex(current_theme.alt_background, "Alt Background\n");
    printHex(current_theme.red, "Red\n");
    printHex(current_theme.green, "Green\n");
    printHex(current_theme.yellow, "Yellow\n");
    printHex(current_theme.orange, "Orange\n");
    printHex(current_theme.blue, "Blue\n");
    printHex(current_theme.magenta, "Magenta\n");

  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }
  return 0;
}
