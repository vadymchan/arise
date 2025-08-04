#ifndef ARISE_FILE_LOGGER_H
#define ARISE_FILE_LOGGER_H

#include "utils/logger/i_logger.h"

#include <spdlog/spdlog.h>

#include <memory>
#include <string>

namespace arise {

class FileLogger : public ILogger {
  public:
  FileLogger(const std::string& loggerName     = std::string(s_kDefaultLoggerName),
             LogLevel           logLevel       = s_kDefaultLogLevel,
             const std::string& pattern        = std::string(s_kDefaultPattern),
             const std::string& filePath       = std::string(s_kDefaultLogFilePath),
             bool               multiThreaded  = true,
             bool               truncateOnOpen = false);

  ~FileLogger() override;

  void log(LogLevel                    level,
           const std::string&          message,
           const std::source_location& loc = std::source_location::current()) override;

  [[nodiscard]] const std::string& getPattern() const;
  [[nodiscard]] LogLevel           getLogLevel() const;
  [[nodiscard]] const std::string& getFilePath() const;

  void setLoggerName(const std::string& name);
  void setPattern(const std::string& pattern);
  void setLogLevel(LogLevel level);
  void setFilePath(const std::string& filePath);

  static constexpr std::string_view s_kDefaultLoggerName  = "file_logger";
  static constexpr LogLevel         s_kDefaultLogLevel    = LogLevel::Trace;
  static constexpr std::string_view s_kDefaultPattern     = "[%^%l%$] %v";
  static constexpr std::string_view s_kDefaultLogFilePath = "output.log";

  private:
  std::shared_ptr<spdlog::logger> m_logger_;
  std::string                     m_pattern_;
  std::string                     m_filePath_;
  LogLevel                        m_logLevel_;
  bool                            m_isMultithreaded_;
  bool                            m_truncateOnOpen_;
};

}  // namespace arise

#endif  // ARISE_FILE_LOGGER_H