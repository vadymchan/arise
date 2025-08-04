#ifndef ARISE_MEMORY_LOGGER_H
#define ARISE_MEMORY_LOGGER_H

#include "i_logger.h"

#include <chrono>
#include <string>
#include <vector>

namespace arise {

struct LogEntry {
  std::chrono::system_clock::time_point timestamp;
  LogLevel                              level;
  std::string                           file;
  int                                   line;
  std::string                           function;
  std::string                           message;
};

class MemoryLogger : public ILogger {
  public:
  // maxEntries = 0 means no limit
  MemoryLogger(std::string loggerName, size_t maxEntries = 0)
      : ILogger(std::move(loggerName))
      , m_maxEntries_(maxEntries) {}

  void log(LogLevel                    level,
           const std::string&          message,
           const std::source_location& location = std::source_location::current()) override;

  const std::vector<LogEntry>& getLogEntries() const;

  void                 setMaxEntries(size_t maxEntries);
  [[nodiscard]] size_t getMaxEntries() const;

  private:
  std::vector<LogEntry> m_entries_;
  size_t                m_maxEntries_{0};
};

}  // namespace arise

#endif  // ARISE_MEMORY_LOGGER_H
