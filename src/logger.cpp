#include "logger.h"

#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>

#include "config_paths.h"

namespace logger {

namespace {

const char* levelName(Level level) {
  switch (level) {
    case Level::Info: return "INFO";
    case Level::Warn: return "WARN";
    case Level::Error: return "ERROR";
  }
  return "UNKNOWN";
}

}  // namespace

void log(Level level, const std::string& category, const std::string& message) {
  try {
    auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm tm_buf{};
#if defined(_WIN32)
    localtime_s(&tm_buf, &now);
#else
    localtime_r(&now, &tm_buf);
#endif

    std::ostringstream date_stream;
    date_stream << std::put_time(&tm_buf, "%Y-%m-%d");
    std::string log_path =
        (config_paths::logsDir() / ("aphelion_" + date_stream.str() + ".log")).string();

    std::ostringstream line;
    line << "[" << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S") << "] "
         << "[" << levelName(level) << "] "
         << "[" << category << "] "
         << message << "\n";

    std::ofstream out(log_path, std::ios::app);
    if (out.is_open()) {
      out << line.str();
    }
  } catch (...) {
    // Logging must never be the reason Aphelion crashes.
  }
}

}  // namespace logger
