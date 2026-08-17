#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>
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

void playThinkerAnimation(const THINKER& thinker, const THEME& theme, int duration_seconds) {
  auto start_time = std::chrono::steady_clock::now();
  size_t frame_index = 0;

  std::cout << "\033[?25l";

  while (true) {
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
      std::chrono::steady_clock::now() - start_time
    ).count();

    if (elapsed >= duration_seconds) {
      break;
    }

    printHex(theme.accent, "\r│ " + thinker.frames[frame_index] + " Processing command...");
    std::cout << std::flush;

    frame_index = (frame_index + 1) % thinker.frames.size();
    std::this_thread::sleep_for(std::chrono::milliseconds(thinker.speed_ms));
  }

  std::cout << "\r\033[K\033[?25h" << std::flush;
}

void renderHeader(const THEME& theme) {
  printHex(theme.accent, " ☄ ");
  printHex(theme.foreground, "APHELION ");
  printHex(theme.alt_background, "│ ");
  printHex(theme.yellow, "Pre-alpha\n");

  int width = getTerminalWidth();
  std::string rule = "";
  for (int i = 0; i < width; ++i) {
    rule += "─";
  }
  printHex(theme.alt_background, rule + "\n\n");
}

bool handleCommand(const std::string& input, SETTINGS& settings, THEME& current_theme, THINKER& current_thinker) {
  std::stringstream ss(input);
  std::string command;
  ss >> command;

  if (command == "/help") {
    printHex(current_theme.yellow, " Available Commands:\n");
    printHex(current_theme.foreground, "  /help           - Display this help menu\n");
    printHex(current_theme.foreground, "  /clear          - Clear the screen\n");
    printHex(current_theme.foreground, "  /theme <name>   - Change active theme\n");
    printHex(current_theme.foreground, "  /thinker <name> - Change active thinker spinner\n");
    printHex(current_theme.foreground, "  /exit, /quit    - Exit the application\n\n");
    return true;
  }

  if (command == "/clear") {
    clearScreen();
    renderHeader(current_theme);
    return true;
  }

  if (command == "/theme") {
    std::string new_theme;
    if (ss >> new_theme) {
      try {
        current_theme = loadTheme(new_theme);
        settings.theme = new_theme;
        saveSettings(settings);
        printHex(current_theme.green, " Theme updated to: " + new_theme + "\n\n");
      } catch (const std::exception& e) {
        printHex(current_theme.red, " Error loading theme: " + std::string(e.what()) + "\n\n");
      }
    } else {
      printHex(current_theme.red, " Usage: /theme <theme_name>\n\n");
    }
    return true;
  }

  if (command == "/thinker") {
    std::string new_thinker;
    if (ss >> new_thinker) {
      try {
        current_thinker = loadThinker(new_thinker);
        settings.thinker = new_thinker;
        saveSettings(settings);
        printHex(current_theme.green, " Thinker updated to: " + new_thinker + "\n\n");
      } catch (const std::exception& e) {
        printHex(current_theme.red, " Error loading thinker: " + std::string(e.what()) + "\n\n");
      }
    } else {
      printHex(current_theme.red, " Usage: /thinker <thinker_name>\n\n");
    }
    return true;
  }

  if (command == "/exit" || command == "/quit") {
    printHex(current_theme.accent, " Goodbye!\n");
    exit(0);
  }

  printHex(current_theme.red, " Unknown command: " + command + ". Type /help for options.\n\n");
  return true;
}

int main() {
  try {
    SETTINGS settings = loadSettings("settings.json");
    THEME current_theme = loadTheme(settings.theme);
    THINKER current_thinker = loadThinker(settings.thinker);

    clearScreen();
    renderHeader(current_theme);

    if (settings.first_run) {
      printHex(current_theme.accent, " Hello there, I'm Aphelion. Type /help to see local commands.\n\n");
      settings.first_run = false;
      saveSettings(settings);
    }

    // Interactive REPL Loop
    while (true) {
      printHex(current_theme.accent, "│ ");
      printHex(current_theme.blue, "❯ ");
      
      std::string user_input;
      if (!std::getline(std::cin, user_input) || user_input.empty()) {
        continue;
      }

      // Check if input is a local command (starts with /)
      if (user_input[0] == '/') {
        handleCommand(user_input, settings, current_theme, current_thinker);
      } else {
        // AI Prompt Handling (Simulated for now)
        playThinkerAnimation(current_thinker, current_theme, 2);
        printHex(current_theme.foreground, " [AI Response pending integration]: " + user_input + "\n\n");
      }
    }

  } catch (const std::exception& e) {
    std::cout << "\033[?25h";
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }
  return 0;
}
