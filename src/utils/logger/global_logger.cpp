#include "utils/logger/log.h"

namespace arise {

void GlobalLogger::AddLogger(std::unique_ptr<ILogger> logger) {
  s_loggers.push_back(std::move(logger));
}

void GlobalLogger::Log(LogLevel logLevel, const std::string& message, const std::source_location& loc) {
  for (auto& logger : s_loggers) {
    logger->log(logLevel, message, loc);
  }
}

ILogger* GlobalLogger::GetLogger(const std::string& name) {
  for (const auto& logger : s_loggers) {
    if (logger->getLoggerName() == name) {
      return logger.get();
    }
  }
  return nullptr;
}

void GlobalLogger::Shutdown() {
  s_loggers.clear();
}

}  // namespace arise