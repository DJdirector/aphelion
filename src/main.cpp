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
#include <iomanip>
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

struct ChatMessage {
  std::string role;    
  std::string content; 
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

// Helper function to auto-append .json extension if missing
std::string sanitizeSessionFilename(std::string filename) {
  if (filename.length() < 5 || filename.substr(filename.length() - 5) != ".json") {
    filename += ".json";
  }
  return filename;
}

// Helper function to generate a new session filepath
std::string createNewSessionFilepath() {
  auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
  std::stringstream ss_filename;
  ss_filename << "history/session_" << std::put_time(std::localtime(&now), "%Y%m%d_%H%M%S") << ".json";
  return ss_filename.str();
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

void saveSessionHistory(const std::string& session_filepath, const std::vector<ChatMessage>& chat_history) {
  try {
    if (!fs::exists("history")) {
      fs::create_directory("history");
    }

    json history_json = json::array();
    for (const auto& msg : chat_history) {
      history_json.push_back({
        {"role", msg.role},
        {"content", msg.content}
      });
    }

    std::ofstream out_file(session_filepath);
    if (out_file.is_open()) {
      out_file << history_json.dump(4);
    }
  } catch (...) {
  }
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

bool handleCommand(
  const std::string& input,
  SETTINGS& settings,
  THEME& current_theme,
  THINKER& current_thinker,
  std::vector<ChatMessage>& chat_history,
  std::string& session_filepath
) {
  std::stringstream ss(input);
  std::string command;
  ss >> command;

  if (command == "/help") {
    printHex(current_theme.yellow, " Available Commands:\n");
    printHex(current_theme.foreground, "  /help           - Display this help menu\n");
    printHex(current_theme.foreground, "  /clear          - Clear the screen\n");
    printHex(current_theme.foreground, "  /history        - View active conversation buffer\n");
    printHex(current_theme.foreground, "  /reset          - Clear current conversation history\n");
    printHex(current_theme.foreground, "  /new            - Start a brand new session context\n");
    printHex(current_theme.foreground, "  /sessions       - List all saved conversation sessions\n");
    printHex(current_theme.foreground, "  /load <file>    - Load a saved session\n");
    printHex(current_theme.foreground, "  /delete <file>  - Delete a saved session\n");
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

  if (command == "/history") {
    if (chat_history.empty()) {
      printHex(current_theme.yellow, " Conversation context buffer is currently empty.\n\n");
      return true;
    }

    printHex(current_theme.yellow, " Active Context Buffer (" + std::to_string(chat_history.size()) + " messages):\n");
    for (size_t i = 0; i < chat_history.size(); ++i) {
      const auto& msg = chat_history[i];
      if (msg.role == "user") {
        printHex(current_theme.blue, "  [" + std::to_string(i + 1) + "] User: ");
      } else {
        printHex(current_theme.accent, "  [" + std::to_string(i + 1) + "] Assistant: ");
      }
      printHex(current_theme.foreground, msg.content + "\n");
    }
    std::cout << "\n";
    return true;
  }

  if (command == "/reset") {
    chat_history.clear();
    saveSessionHistory(session_filepath, chat_history);
    printHex(current_theme.green, " ✓ ");
    printHex(current_theme.foreground, "Conversation history context cleared.\n\n");
    return true;
  }

  if (command == "/new") {
    chat_history.clear();
    session_filepath = createNewSessionFilepath();
    printHex(current_theme.green, " ✓ ");
    printHex(current_theme.foreground, "Started new session context.\n\n");
    return true;
  }

  if (command == "/sessions") {
    printHex(current_theme.yellow, " Saved Sessions (history/):\n");
    if (!fs::exists("history") || fs::is_empty("history")) {
      printHex(current_theme.foreground, "  No saved sessions found.\n\n");
      return true;
    }
    
    for (const auto& entry : fs::directory_iterator("history")) {
      if (entry.is_regular_file() && entry.path().extension() == ".json") {
        std::string filename = entry.path().filename().string();
        printHex(current_theme.foreground, "  • " + filename);
        if ("history/" + filename == session_filepath) {
          printHex(current_theme.accent, " (Active)");
        }
        std::cout << "\n";
      }
    }
    std::cout << "\n";
    return true;
  }

  if (command == "/load") {
    std::string raw_filename;
    if (ss >> raw_filename) {
      std::string filename = sanitizeSessionFilename(raw_filename);
      std::string target_path = "history/" + filename;
      
      if (!fs::exists(target_path)) {
        printHex(current_theme.red, " ✗ Session not found. It might have gotten deleted.\n\n");
        return true;
      }

      try {
        std::ifstream file(target_path);
        json session_data;
        file >> session_data;

        chat_history.clear();
        for (const auto& item : session_data) {
          chat_history.push_back({
            item.value("role", "unknown"),
            item.value("content", "")
          });
        }
        
        session_filepath = target_path;

        printHex(current_theme.green, " ✓ ");
        printHex(current_theme.foreground, "Loaded session: ");
        printHex(current_theme.accent, filename + " (" + std::to_string(chat_history.size()) + " messages)\n\n");
      } catch (const std::exception& e) {
        printHex(current_theme.red, " ✗ Error parsing session file: " + std::string(e.what()) + "\n\n");
      }
    } else {
      printHex(current_theme.red, " Usage: /load <filename>\n\n");
    }
    return true;
  }

  if (command == "/delete") {
    std::string raw_filename;
    if (ss >> raw_filename) {
      std::string filename = sanitizeSessionFilename(raw_filename);
      std::string target_path = "history/" + filename;
      
      if (!fs::exists(target_path)) {
        printHex(current_theme.red, " ✗ Session not found. It might have gotten deleted.\n\n");
        return true;
      }

      try {
        fs::remove(target_path);
        printHex(current_theme.green, " ✓ ");
        printHex(current_theme.foreground, "Deleted session: ");
        printHex(current_theme.accent, filename + "\n\n");

        if (target_path == session_filepath) {
          chat_history.clear();
          session_filepath = createNewSessionFilepath();
          printHex(current_theme.yellow, " Active session was deleted. Started a new fresh context.\n\n");
        }
      } catch (const std::exception& e) {
        printHex(current_theme.red, " ✗ Error deleting session: " + std::string(e.what()) + "\n\n");
      }
    } else {
      printHex(current_theme.red, " Usage: /delete <filename>\n\n");
    }
    return true;
  }

  if (command == "/theme") {
    std::string new_theme;
    if (ss >> new_theme) {
      try {
        current_theme = loadTheme(new_theme);
        settings.theme = new_theme;
        saveSettings(settings);

        printHex(current_theme.green, " ✓ ");
        printHex(current_theme.foreground, "Theme updated to: ");
        printHex(current_theme.accent, new_theme + "\n\n");
      } catch (const std::exception& e) {
        printHex(current_theme.red, " ✗ Error loading theme: " + std::string(e.what()) + "\n\n");
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

    std::string session_filepath = createNewSessionFilepath();
    std::vector<ChatMessage> chat_history;

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
      printHex(current_theme.accent, "❯ ");
      
      std::string user_input;
      if (!std::getline(std::cin, user_input) || user_input.empty()) {
        continue;
      }

      if (user_input[0] == '/') {
        handleCommand(user_input, settings, current_theme, current_thinker, chat_history, session_filepath);
      } else {
        chat_history.push_back({"user", user_input});
        saveSessionHistory(session_filepath, chat_history);

        playThinkerAnimation(current_thinker, current_theme, 2);

        std::string mock_response = "[AI Response pending integration]: " + user_input;
        chat_history.push_back({"assistant", mock_response});
        saveSessionHistory(session_filepath, chat_history);

        printHex(current_theme.foreground, " " + mock_response + "\n\n");
      }
    }

  } catch (const std::exception& e) {
    std::cout << "\033[?25h";
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }
  return 0;
}
