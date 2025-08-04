#ifndef ARISE_GLOBAL_LOGGER_H
#define ARISE_GLOBAL_LOGGER_H

#include "utils/logger/i_logger.h"

#include <spdlog/fmt/fmt.h>

#include <memory>
#include <vector>

namespace arise {

class GlobalLogger {
  public:
  static void AddLogger(std::unique_ptr<ILogger> logger);

  static void Log(LogLevel                    level,
                  const std::string&          message,
                  const std::source_location& loc = std::source_location::current());

  template <typename... Args>
  static void Log(LogLevel                    level,
                  fmt::format_string<Args...> fmtStr,
                  Args&&... args,
                  const std::source_location& loc = std::source_location::current()) {
    GlobalLogger::Log(level, fmt::format(fmtStr, std::forward<Args>(args)...), loc);
  }

  static ILogger* GetLogger(const std::string& name);

  static void Shutdown();

  private:
  static inline std::vector<std::unique_ptr<ILogger>> s_loggers;
};

}  // namespace arise

#endif  // ARISE_GLOBAL_LOGGER_H