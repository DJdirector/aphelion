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
#include "tools/scan.h"
#include "tools/registry.h"
#include "config_paths.h"
#include "logger.h"

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
  std::vector<ai::ToolCall> tool_calls;  // set on "assistant" messages that called tools
  std::string tool_call_id;              // set on "tool" messages: which call this answers
  std::string tool_name;                 // set on "tool" messages: which tool produced this
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

// Same hex -> truecolor conversion as printHex, but returns just the
// "set foreground" escape sequence with no trailing text or reset - used by
// the markdown renderer, which needs to toggle bold/italic/color mid-line
// without stomping the surrounding style each time.
std::string ansiFg(const std::string& hex) {
  std::string clean_hex = hex;
  if (!clean_hex.empty() && clean_hex[0] == '#') {
    clean_hex.erase(0, 1);
  }
  if (clean_hex.length() < 6) {
    return "";
  }
  int r = std::stoi(clean_hex.substr(0, 2), nullptr, 16);
  int g = std::stoi(clean_hex.substr(2, 2), nullptr, 16);
  int b = std::stoi(clean_hex.substr(4, 2), nullptr, 16);

  std::ostringstream oss;
  oss << "\033[38;2;" << r << ";" << g << ";" << b << "m";
  return oss.str();
}

// Same as ansiFg but sets the background - used to give fenced code blocks
// an actual highlighted box instead of just colored text, since
// alt_background is meant to be used as a background, not foreground text
// (its whole purpose is being close to the terminal background, which makes
// it nearly unreadable as foreground text).
std::string ansiBg(const std::string& hex) {
  std::string clean_hex = hex;
  if (!clean_hex.empty() && clean_hex[0] == '#') {
    clean_hex.erase(0, 1);
  }
  if (clean_hex.length() < 6) {
    return "";
  }
  int r = std::stoi(clean_hex.substr(0, 2), nullptr, 16);
  int g = std::stoi(clean_hex.substr(2, 2), nullptr, 16);
  int b = std::stoi(clean_hex.substr(4, 2), nullptr, 16);

  std::ostringstream oss;
  oss << "\033[48;2;" << r << ";" << g << ";" << b << "m";
  return oss.str();
}

// Renders a single line's inline markdown: **bold**, *italic*/_italic_, and
// `inline code`. Deliberately hand-rolled instead of std::regex - libstdc++'s
// regex has no lookbehind, and this is simple enough not to need it.
void printInlineFormatted(const std::string& line, const THEME& theme) {
  std::cout << ansiFg(theme.foreground);
  bool bold = false;
  bool italic = false;
  size_t i = 0;
  const size_t n = line.size();

  while (i < n) {
    if (i + 1 < n && line[i] == '*' && line[i + 1] == '*') {
      bold = !bold;
      std::cout << (bold ? "\033[1m" : "\033[22m");
      i += 2;
      continue;
    }
    if (line[i] == '*' || line[i] == '_') {
      italic = !italic;
      std::cout << (italic ? "\033[3m" : "\033[23m");
      i += 1;
      continue;
    }
    if (line[i] == '`') {
      size_t end = line.find('`', i + 1);
      if (end != std::string::npos) {
        std::cout << ansiBg(theme.alt_background) << ansiFg(theme.yellow) << " "
                   << line.substr(i + 1, end - i - 1) << " "
                   << "\033[49m" << ansiFg(theme.foreground);
        i = end + 1;
        continue;
      }
    }
    std::cout << line[i];
    ++i;
  }

  std::cout << "\033[0m";
}

// Line-oriented markdown renderer for AI responses: headers (#/##/###),
// fenced code blocks, bullet lists, and inline formatting via
// printInlineFormatted above. Not a full CommonMark implementation - just
// enough to make typical LLM output (headers, bold, code) readable in a
// terminal instead of showing raw '#'/'**'/backtick characters.
void renderMarkdown(const std::string& text, const THEME& theme) {
  std::istringstream iss(text);
  std::string line;
  bool in_code_block = false;

  while (std::getline(iss, line)) {
    size_t first = line.find_first_not_of(" \t");
    std::string stripped = (first == std::string::npos) ? "" : line.substr(first);

    if (stripped.rfind("```", 0) == 0) {
      bool opening = !in_code_block;
      in_code_block = !in_code_block;
      int width = getTerminalWidth();
      std::string blank_row(width, ' ');

      if (opening) {
        // Blank padding row above the label, so the box doesn't look clipped
        // at the top - mirrors the blank row the closing fence already gives
        // us for free at the bottom (see below).
        std::cout << ansiBg(theme.alt_background) << blank_row << "\033[0m\n";
        std::string lang = stripped.substr(3);
        std::string label = "  " + (lang.empty() ? "code" : lang);
        if (static_cast<int>(label.size()) < width) {
          label += std::string(width - label.size(), ' ');
        }
        std::cout << ansiBg(theme.alt_background) << ansiFg(theme.accent) << label << "\033[0m\n";
      } else {
        // Closing fence: just the blank padding row at the bottom.
        std::cout << ansiBg(theme.alt_background) << blank_row << "\033[0m\n";
      }
      continue;
    }

    if (in_code_block) {
      std::string content = "  " + line;
      int width = getTerminalWidth();
      if (static_cast<int>(content.size()) < width) {
        content += std::string(width - content.size(), ' ');
      }
      std::cout << ansiBg(theme.alt_background) << ansiFg(theme.green) << content << "\033[0m\n";
      continue;
    }

    size_t header_level = 0;
    while (header_level < stripped.size() && stripped[header_level] == '#') {
      ++header_level;
    }
    if (header_level > 0 && header_level <= 6 && header_level < stripped.size() &&
        stripped[header_level] == ' ') {
      std::string header_text = stripped.substr(header_level + 1);
      std::cout << "\033[1m" << ansiFg(theme.accent) << " " << header_text << "\033[0m\n";
      continue;
    }

    if (stripped.rfind("- ", 0) == 0 || stripped.rfind("* ", 0) == 0) {
      printHex(theme.accent, "  • ");
      printInlineFormatted(stripped.substr(2), theme);
      std::cout << "\n";
      continue;
    }

    if (stripped.empty()) {
      std::cout << "\n";
      continue;
    }

    printInlineFormatted(line, theme);
    std::cout << "\n";
  }
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
  ss_filename << "session_" << std::put_time(std::localtime(&now), "%Y%m%d_%H%M%S") << ".json";
  return (config_paths::historyDir() / ss_filename.str()).string();
}

// Same convention, for saved project scans.
std::string createScanFilepath() {
  auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
  std::stringstream ss_filename;
  ss_filename << "scan_" << std::put_time(std::localtime(&now), "%Y%m%d_%H%M%S") << ".txt";
  return (config_paths::historyDir() / ss_filename.str()).string();
}

SETTINGS loadSettings(const std::string& file_path = config_paths::settingsFilePath().string()) {
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
  try {
    file >> settings_data;
  } catch (const std::exception& e) {
    logger::error("settings_load", "Malformed settings.json at " + file_path + ": " + e.what());
    throw;
  }

  SETTINGS settings;
  settings.first_run = settings_data.value("first_run", true);
  settings.theme = settings_data.value("theme", "aphelion-dark");
  settings.thinker = settings_data.value("thinker", "default");
  settings.ai = settings_data.contains("ai")
                    ? ai::aiSettingsFromJson(settings_data["ai"])
                    : ai::AISettings{};

  return settings;
}

void saveSettings(const SETTINGS& settings, const std::string& file_path = config_paths::settingsFilePath().string()) {
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
  std::string file_path = (config_paths::themesDir() / (theme_name + ".json")).string();

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
  std::string file_path = (config_paths::thinkersDir() / (thinker_name + ".json")).string();

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
    if (!fs::exists(config_paths::historyDir())) {
      fs::create_directory(config_paths::historyDir());
    }

    json history_json = json::array();
    for (const auto& msg : chat_history) {
      json entry = {{"role", msg.role}, {"content", msg.content}};

      if (!msg.tool_calls.empty()) {
        json calls = json::array();
        for (const auto& tc : msg.tool_calls) {
          calls.push_back({{"id", tc.id}, {"name", tc.name}, {"arguments", tc.arguments},
                            {"thought_signature", tc.thought_signature}});
        }
        entry["tool_calls"] = calls;
      }
      if (!msg.tool_call_id.empty()) {
        entry["tool_call_id"] = msg.tool_call_id;
      }
      if (!msg.tool_name.empty()) {
        entry["tool_name"] = msg.tool_name;
      }

      history_json.push_back(entry);
    }

    std::ofstream out_file(session_filepath);
    if (out_file.is_open()) {
      out_file << history_json.dump(4);
    }
  } catch (const std::exception& e) {
    logger::error("session_save", "Failed to save session to " + session_filepath + ": " + e.what());
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
    printHex(current_theme.foreground, "  /paths          - Show resolved config directory and file paths\n");
    printHex(current_theme.foreground, "  /scan           - Locally scan the project and add it to context\n");
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
        printHex(current_theme.foreground, msg.content + "\n");
      } else if (msg.role == "tool") {
        std::string label = msg.tool_name.empty() ? "tool" : msg.tool_name;
        printHex(current_theme.green, "  [" + std::to_string(i + 1) + "] Tool result (" + label + "): ");
        printHex(current_theme.foreground, msg.content + "\n");
      } else {
        printHex(current_theme.accent, "  [" + std::to_string(i + 1) + "] Assistant: ");
        printHex(current_theme.foreground, msg.content + "\n");
        for (const auto& tc : msg.tool_calls) {
          printHex(current_theme.yellow, "      ⚙ called " + tc.name + "(" + tc.arguments.dump() + ")\n");
        }
      }
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
    printHex(current_theme.yellow, " Saved Sessions (" + config_paths::historyDir().string() + "):\n");
    if (!fs::exists(config_paths::historyDir()) || fs::is_empty(config_paths::historyDir())) {
      printHex(current_theme.foreground, "  No saved sessions found.\n\n");
      return true;
    }
    
    for (const auto& entry : fs::directory_iterator(config_paths::historyDir())) {
      if (entry.is_regular_file() && entry.path().extension() == ".json") {
        std::string filename = entry.path().filename().string();
        printHex(current_theme.foreground, "  • " + filename);
        if (entry.path().string() == session_filepath) {
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
      std::string target_path = (config_paths::historyDir() / filename).string();
      
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
          ChatMessage msg;
          msg.role = item.value("role", "unknown");
          msg.content = item.value("content", "");
          msg.tool_call_id = item.value("tool_call_id", "");
          msg.tool_name = item.value("tool_name", "");
          if (item.contains("tool_calls") && item["tool_calls"].is_array()) {
            for (const auto& tc_json : item["tool_calls"]) {
              ai::ToolCall tc;
              tc.id = tc_json.value("id", "");
              tc.name = tc_json.value("name", "");
              tc.arguments = tc_json.value("arguments", json::object());
              tc.thought_signature = tc_json.value("thought_signature", "");
              msg.tool_calls.push_back(tc);
            }
          }
          chat_history.push_back(msg);
        }
        
        session_filepath = target_path;

        printHex(current_theme.green, " ✓ ");
        printHex(current_theme.foreground, "Loaded session: ");
        printHex(current_theme.accent, filename + " (" + std::to_string(chat_history.size()) + " messages)\n\n");
      } catch (const std::exception& e) {
        logger::error("session_load", "Failed to load " + target_path + ": " + e.what());
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
      std::string target_path = (config_paths::historyDir() / filename).string();
      
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
        logger::error("session_delete", "Failed to delete " + target_path + ": " + e.what());
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
        logger::error("theme_load", "Failed to load theme \"" + new_theme + "\": " + e.what());
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
        logger::error("thinker_load", "Failed to load thinker \"" + new_thinker + "\": " + e.what());
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
        logger::error("provider_switch", "Failed to switch to provider \"" + new_provider + "\": " + e.what());
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
        logger::error("model_update", "Failed to set model \"" + new_model + "\" for " + settings.ai.provider + ": " + e.what());
        printHex(current_theme.red, " ✗ Error updating model: " + std::string(e.what()) + "\n\n");
      }
    } else {
      printHex(current_theme.red, " Usage: /model <model_name>\n\n");
    }
    return true;
  }

  if (command == "/paths") {
    printHex(current_theme.yellow, " Aphelion Config Paths:\n");

    printHex(current_theme.foreground, "  Config directory:  ");
    printHex(current_theme.accent, config_paths::configDir().string() + "\n");

    fs::path settings_path = config_paths::settingsFilePath();
    printHex(current_theme.foreground, "  Settings file:     ");
    printHex(current_theme.accent, settings_path.string());
    printHex(fs::exists(settings_path) ? current_theme.green : current_theme.red,
             fs::exists(settings_path) ? "  ✓\n" : "  ✗ missing\n");

    fs::path theme_path = config_paths::themesDir() / (settings.theme + ".json");
    printHex(current_theme.foreground, "  Active theme:      ");
    printHex(current_theme.accent, theme_path.string());
    printHex(fs::exists(theme_path) ? current_theme.green : current_theme.red,
             fs::exists(theme_path) ? "  ✓\n" : "  ✗ missing\n");

    fs::path thinker_path = config_paths::thinkersDir() / (settings.thinker + ".json");
    printHex(current_theme.foreground, "  Active thinker:    ");
    printHex(current_theme.accent, thinker_path.string());
    printHex(fs::exists(thinker_path) ? current_theme.green : current_theme.red,
             fs::exists(thinker_path) ? "  ✓\n" : "  ✗ missing\n");

    printHex(current_theme.foreground, "  History directory: ");
    printHex(current_theme.accent, config_paths::historyDir().string() + "\n");

    printHex(current_theme.foreground, "  Logs directory:    ");
    printHex(current_theme.accent, config_paths::logsDir().string() + "\n");

    std::cout << "\n";
    return true;
  }

  if (command == "/scan") {
    printHex(current_theme.accent, " Scanning project...\n");

    scan::ScanResult result = scan::scanProject();

    std::string scan_filepath = createScanFilepath();
    std::ofstream out(scan_filepath);
    if (out.is_open()) {
      out << result.digest;
    }

    double approx_tokens = static_cast<double>(result.total_bytes) / 4.0;

    printHex(current_theme.green, " ✓ ");
    printHex(current_theme.foreground,
             "Scanned " + std::to_string(result.files_included) + " files (" +
             std::to_string(result.total_bytes) + " bytes, ~" +
             std::to_string(static_cast<long>(approx_tokens)) + " tokens)\n");

    if (result.files_skipped_size > 0 || result.files_skipped_binary > 0) {
      printHex(current_theme.yellow,
               " (" + std::to_string(result.files_skipped_size) + " skipped: too large, " +
               std::to_string(result.files_skipped_binary) + " skipped: binary)\n");
    }
    if (result.truncated) {
      printHex(current_theme.yellow, " Digest truncated - project is larger than the scan limit.\n");
    }

    printHex(current_theme.foreground, " Saved to: ");
    printHex(current_theme.accent, scan_filepath + "\n");

    chat_history.push_back({"user", result.digest});
    saveSessionHistory(session_filepath, chat_history);

    printHex(current_theme.foreground,
             " Added to conversation context - your next message will include it.\n\n");
    return true;
  }

  if (command == "/exit" || command == "/quit") {
    printHex(current_theme.accent, " Goodbye!\n");
    exit(0);
  }

  printHex(current_theme.red, " Unknown command: " + command + ". Type /help for options.\n\n");
  return true;
}

// A short identity/system prompt sent to the model on every request so it
// answers as Aphelion rather than surfacing the underlying provider/model.
// Kept out of chat_history/session files on purpose - it's config, not a turn.
std::string buildSystemPrompt() {
  return "You are Aphelion, a free and open-source AI coding agent that runs in the "
         "user's terminal. If asked who or what you are, answer as Aphelion - don't "
         "reveal or speculate about the specific underlying model or provider powering "
         "you unless the user explicitly asks about the implementation. Be concise and "
         "direct, in keeping with a terminal tool. You may use standard Markdown - "
         "headers (#, ##, ###), **bold**, *italic*, `inline code`, fenced code blocks, "
         "and simple '- ' bullet lists - it is rendered directly in the terminal. Avoid "
         "tables, images, nested/numbered lists, and links, which do not render well here. "
         "You have a scan_project tool available: it returns a read-only text digest "
         "(file tree + contents) of the user's project. Call it when you genuinely need to "
         "see the codebase to answer - e.g. the user asks about 'this project', 'the "
         "codebase', or wants you to review/explain/modify files you haven't seen yet. Don't "
         "call it speculatively or more than once per turn unless the result truly didn't "
         "answer what you needed - the user may be on a metered API plan.";
}

int main() {
  try {
    config_paths::ensureConfigLayout();

    SETTINGS settings = loadSettings();
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
      logger::error("provider_setup", e.what());
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
        chat_history.push_back({"user", user_input, {}, "", ""});
        saveSessionHistory(session_filepath, chat_history);

        if (!provider) {
          printHex(current_theme.red, " ✗ No AI provider configured. Use /provider to set one.\n\n");
        } else {
          std::vector<ai::ToolDefinition> tool_defs = tools::availableTools();
          const int max_tool_iterations = 5;  // guards against a runaway tool-call loop
          bool turn_done = false;

          for (int iteration = 0; !turn_done && iteration < max_tool_iterations; ++iteration) {
            std::vector<ai::Message> ai_history;
            ai_history.reserve(chat_history.size() + 1);

            ai::Message sys_msg;
            sys_msg.role = "system";
            sys_msg.content = buildSystemPrompt();
            ai_history.push_back(sys_msg);

            for (const auto& msg : chat_history) {
              ai::Message m;
              m.role = msg.role;
              m.content = msg.content;
              m.tool_calls = msg.tool_calls;
              m.tool_call_id = msg.tool_call_id;
              m.tool_name = msg.tool_name;
              ai_history.push_back(m);
            }

            std::future<ai::AIResponse> response_future = std::async(std::launch::async, [&]() {
              return provider->sendMessage(ai_history, tool_defs);
            });
            playThinkerAnimationUntilReady(current_thinker, current_theme, response_future);
            ai::AIResponse response = response_future.get();

            if (!response.ok) {
              if (iteration == 0) {
                chat_history.pop_back();  // don't keep a user turn that got no reply at all
              }
              saveSessionHistory(session_filepath, chat_history);
              logger::error("ai_request", response.error);
              printHex(current_theme.red, " ✗ AI error: " + response.error + "\n\n");
              turn_done = true;
              break;
            }

            ChatMessage assistant_msg;
            assistant_msg.role = "assistant";
            assistant_msg.content = response.content;
            assistant_msg.tool_calls = response.tool_calls;
            chat_history.push_back(assistant_msg);
            saveSessionHistory(session_filepath, chat_history);

            if (!response.content.empty()) {
              renderMarkdown(response.content, current_theme);
            }

            if (response.tool_calls.empty()) {
              std::cout << "\n";
              turn_done = true;
              break;
            }

            for (const auto& call : response.tool_calls) {
              ai::Message tool_result;
              bool proceed = true;

              if (tools::isDestructive(call.name)) {
                printHex(current_theme.yellow, " ⚠ ");
                printHex(current_theme.foreground, "\"" + call.name + "\" wants to make a change:\n");
                std::string preview = tools::getConfirmationPreview(call);
                if (!preview.empty()) {
                  printHex(current_theme.foreground, "  " + preview + "\n");
                }
                printHex(current_theme.accent, " Proceed? [y/N] ");

                std::string confirm_input;
                std::getline(std::cin, confirm_input);
                proceed = (confirm_input == "y" || confirm_input == "Y");

                if (!proceed) {
                  tool_result.role = "tool";
                  tool_result.tool_call_id = call.id;
                  tool_result.tool_name = call.name;
                  tool_result.content =
                      "User declined to run this tool. Do not attempt it again unless the user explicitly asks.";
                  printHex(current_theme.red, " ✗ Declined.\n");
                }
              }

              if (proceed) {
                printHex(current_theme.yellow, " ⚙ ");
                printHex(current_theme.foreground, "Running tool: ");
                printHex(current_theme.accent, call.name + "\n");

                try {
                  tool_result = tools::executeToolCall(call);
                } catch (const std::exception& e) {
                  logger::error("tool_execution", "Tool \"" + call.name + "\" threw: " + e.what());
                  tool_result.role = "tool";
                  tool_result.tool_call_id = call.id;
                  tool_result.tool_name = call.name;
                  tool_result.content = "Error: tool execution failed - " + std::string(e.what());
                  printHex(current_theme.red,
                           " ✗ Tool \"" + call.name + "\" failed: " + std::string(e.what()) + "\n");
                }
              }

              ChatMessage tool_msg;
              tool_msg.role = "tool";
              tool_msg.content = tool_result.content;
              tool_msg.tool_call_id = tool_result.tool_call_id;
              tool_msg.tool_name = tool_result.tool_name;
              chat_history.push_back(tool_msg);
            }
            saveSessionHistory(session_filepath, chat_history);

            if (iteration == max_tool_iterations - 1) {
              printHex(current_theme.yellow,
                       " (Reached the max tool-call iterations for this turn - stopping here.)\n\n");
            }
          }
        }
      }
    }

  } catch (const std::exception& e) {
    std::cout << "\033[?25h";
    logger::error("fatal", e.what());
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }
  return 0;
}
