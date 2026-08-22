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
#include <future>
#include <memory>
#include <nlohmann/json.hpp>

#include "ai/settings.h"
#include "ai/types.h"
#include "ai/provider.h"
#include "ai/provider_factory.h"

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
  ai::AISettings ai;
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
    ai::AISettings default_ai;
    json default_config = {
      {"first_run", true},
      {"theme", "aphelion-dark"},
      {"thinker", "default"},
      {"ai", ai::aiSettingsToJson(default_ai)}
    };
    std::ofstream out_file(file_path);
    out_file << default_config.dump(4);
    return { true, "aphelion-dark", "default", default_ai };
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
  settings.ai = settings_data.contains("ai")
                    ? ai::aiSettingsFromJson(settings_data["ai"])
                    : ai::AISettings{};

  return settings;
}

void saveSettings(const SETTINGS& settings, const std::string& file_path = "settings.json") {
  json settings_data = {
    {"first_run", settings.first_run},
    {"theme", settings.theme},
    {"thinker", settings.thinker},
    {"ai", ai::aiSettingsToJson(settings.ai)}
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

// Same spinner, but spins until a pending AI response is ready instead of a
// fixed duration - real network calls don't take a predictable 2 seconds.
void playThinkerAnimationUntilReady(const THINKER& thinker, const THEME& theme,
                                     std::future<ai::AIResponse>& pending) {
  size_t frame_index = 0;
  std::cout << "\033[?25l";

  while (pending.wait_for(std::chrono::milliseconds(thinker.speed_ms)) != std::future_status::ready) {
    printHex(theme.accent, "\r│ " + thinker.frames[frame_index] + " Processing command...");
    std::cout << std::flush;
    frame_index = (frame_index + 1) % thinker.frames.size();
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
  std::string& session_filepath,
  std::unique_ptr<ai::AIProvider>& provider
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
    printHex(current_theme.foreground, "  /provider <name>- Switch AI provider (gemini, openrouter, ollama)\n");
    printHex(current_theme.foreground, "  /model <name>   - Set the model for the active provider\n");
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

  if (command == "/provider") {
    std::string new_provider;
    if (ss >> new_provider) {
      if (new_provider != "gemini" && new_provider != "openrouter" && new_provider != "ollama") {
        printHex(current_theme.red, " ✗ Unknown provider. Available: gemini, openrouter, ollama\n\n");
        return true;
      }
      std::string previous_provider = settings.ai.provider;
      settings.ai.provider = new_provider;
      try {
        provider = ai::createProvider(settings.ai);
        saveSettings(settings);
        printHex(current_theme.green, " ✓ ");
        printHex(current_theme.foreground, "AI provider switched to: ");
        printHex(current_theme.accent, new_provider + "\n\n");
      } catch (const std::exception& e) {
        settings.ai.provider = previous_provider;
        printHex(current_theme.red, " ✗ Error switching provider: " + std::string(e.what()) + "\n\n");
      }
    } else {
      printHex(current_theme.red, " Usage: /provider <gemini|openrouter|ollama>\n\n");
    }
    return true;
  }

  if (command == "/model") {
    std::string new_model;
    if (ss >> new_model) {
      if (settings.ai.provider == "gemini") {
        settings.ai.gemini.model = new_model;
      } else if (settings.ai.provider == "openrouter") {
        settings.ai.openrouter.model = new_model;
      } else {
        settings.ai.ollama.model = new_model;
      }

      try {
        provider = ai::createProvider(settings.ai);
        saveSettings(settings);
        printHex(current_theme.green, " ✓ ");
        printHex(current_theme.foreground, "Model for " + settings.ai.provider + " set to: ");
        printHex(current_theme.accent, new_model + "\n\n");
      } catch (const std::exception& e) {
        printHex(current_theme.red, " ✗ Error updating model: " + std::string(e.what()) + "\n\n");
      }
    } else {
      printHex(current_theme.red, " Usage: /model <model_name>\n\n");
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

    std::unique_ptr<ai::AIProvider> provider;
    try {
      provider = ai::createProvider(settings.ai);
    } catch (const std::exception& e) {
      // Don't hard-fail startup over a bad provider config; let the user fix it
      // with /provider once the REPL is up.
      std::cerr << "Warning: " << e.what() << std::endl;
    }

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
        handleCommand(user_input, settings, current_theme, current_thinker, chat_history, session_filepath, provider);
      } else {
        chat_history.push_back({"user", user_input});
        saveSessionHistory(session_filepath, chat_history);

        if (!provider) {
          printHex(current_theme.red, " ✗ No AI provider configured. Use /provider to set one.\n\n");
        } else {
          std::vector<ai::Message> ai_history;
          ai_history.reserve(chat_history.size());
          for (const auto& msg : chat_history) {
            ai_history.push_back({msg.role, msg.content, {}, ""});
          }

          std::future<ai::AIResponse> response_future = std::async(std::launch::async, [&]() {
            return provider->sendMessage(ai_history);
          });
          playThinkerAnimationUntilReady(current_thinker, current_theme, response_future);
          ai::AIResponse response = response_future.get();

          if (response.ok) {
            chat_history.push_back({"assistant", response.content});
            saveSessionHistory(session_filepath, chat_history);

            printHex(current_theme.foreground, " " + response.content + "\n");
            if (!response.tool_calls.empty()) {
              printHex(current_theme.yellow,
                       " (" + std::to_string(response.tool_calls.size()) +
                       " tool call(s) requested - tool execution isn't implemented yet)\n");
            }
            std::cout << "\n";
          } else {
            chat_history.pop_back();  // don't keep a user turn that got no reply
            saveSessionHistory(session_filepath, chat_history);
            printHex(current_theme.red, " ✗ AI error: " + response.error + "\n\n");
          }
        }
      }
    }

  } catch (const std::exception& e) {
    std::cout << "\033[?25h";
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }
  return 0;
}
