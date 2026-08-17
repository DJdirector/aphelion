#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <stdexcept>
#include <filesystem>
#include <random>
#include <nlohmann/json.hpp>

#if defined(_WIN32)
#include <windows.h>
#else
#include <sys/ioctl.h>
#include <unistd.h>
#endif

using json = nlohmann::json;
namespace fs = std::filesystem;

struct SETTINGS {
  bool first_run;
  std::string theme;
  std::string thinker;
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

struct THINKER {
  int speed_ms;
  std::vector<std::string> frames;
};

void clearScreen() {
  std::cout << "\033[2J\033[H" << std::flush;
}

int getTerminalWidth() {
#if defined(_WIN32)
  CONSOLE_SCREEN_BUFFER_INFO csbi;
  if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi)) {
    return csbi.srWindow.Right - csbi.srWindow.Left + 1;
  }
#else
  struct winsize w;
  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0) {
    return w.ws_col;
  }
#endif
  return 80;
}

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
      {"theme", "aphelion-dark"},
      {"thinker", "default"}
    };
    std::ofstream out_file(file_path);
    out_file << default_config.dump(4);
    return { true, "aphelion-dark", "default" };
  }

  std::ifstream file(file_path);
  if (!file.is_open()) {
    throw std::runtime_error("Failed to open settings file at: " + file_path);
  }

  json settings_data;
  file >> settings_data;

  SETTINGS settings;
  settings.first_run = settings_data.value("first_run", true);
  settings.theme = settings_data.value("theme", "aphelion-dark");
  settings.thinker = settings_data.value("thinker", "default");

  return settings;
}

void saveSettings(const SETTINGS& settings, const std::string& file_path = "settings.json") {
  json settings_data = {
    {"first_run", settings.first_run},
    {"theme", settings.theme},
    {"thinker", settings.thinker}
  };

  std::ofstream out_file(file_path);
  if (!out_file.is_open()) {
    throw std::runtime_error("Failed to save settings file at: " + file_path);
  }

  out_file << settings_data.dump(4);
}

THEME loadTheme(const std::string& theme_name) {
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

THINKER loadThinker(const std::string& thinker_name) {
  std::string file_path = "thinkers/" + thinker_name + ".json";

  std::ifstream file(file_path);
  if (!file.is_open()) {
    throw std::runtime_error("Failed to open thinker file at: " + file_path);
  }

  json thinker_data;
  file >> thinker_data;

  THINKER thinker;
  thinker.speed_ms = thinker_data.value("speed_ms", 100);

  if (thinker_data.contains("frames") && thinker_data["frames"].is_array()) {
    thinker.frames = thinker_data["frames"].get<std::vector<std::string>>();
  }

  if (thinker.frames.size() < 2 || thinker.frames.size() > 10) {
    throw std::runtime_error("Thinker frames count must be between 2 and 10! Got: " 
                             + std::to_string(thinker.frames.size()));
  }

  return thinker;
}

std::string getRandomWelcomeMessage() {
  std::vector<std::string> messages = {
    "Any new ideas to explore?",
    "Ready when you are.",
    "What are we building today?",
    "Let's get into it.",
    "Standing by for instructions."
  };

  static std::random_device rd;
  static std::mt19937 gen(rd());
  std::uniform_int_distribution<size_t> dist(0, messages.size() - 1);

  return messages[dist(gen)];
}

void renderHeader(const THEME& theme) {
  printHex(theme.accent, " ☄ ");
  printHex(theme.foreground, "APHELION ");
  printHex(theme.alt_background, "│ ");
  printHex(theme.yellow, "Alpha\n");
}

int main() {
  try {
    SETTINGS settings = loadSettings("settings.json");
    THEME current_theme = loadTheme(settings.theme);
    THINKER current_thinker = loadThinker(settings.thinker);

    clearScreen();

    // 1. Render App Header
    renderHeader(current_theme);

    // 2. Render Divider Rule
    int width = getTerminalWidth();
    std::string rule = "";
    for (int i = 0; i < width; ++i) {
      rule += "─";
    }
    printHex(current_theme.alt_background, rule + "\n\n");

    // 3. Render Welcome Banner
    if (settings.first_run) {
      printHex(current_theme.accent, " Hello there, I'm Aphelion. Let's get started!\n\n");
      settings.first_run = false;
      saveSettings(settings, "settings.json");
    } else {
      printHex(current_theme.foreground, " " + getRandomWelcomeMessage() + "\n\n");
    }

    // 4. Clean Interactive Prompt
    printHex(current_theme.accent, "│ ");
    printHex(current_theme.blue, "❯ ");
    
    std::string user_input;
    std::getline(std::cin, user_input);

  } catch (const std::exception& e) {
    std::cout << "\033[?25h";
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }
  return 0;
}
