#ifndef ARISE_LOG_H
#define ARISE_LOG_H

#include "utils/logger/global_logger.h"

#include <source_location>

namespace arise {

namespace detail {

inline void Log(LogLevel level, const std::source_location& loc, const std::string& msg) {
  GlobalLogger::Log(level, msg, loc);
}

template <typename... Args>
inline void Log(LogLevel level, const std::source_location& loc, fmt::format_string<Args...> fmtStr, Args&&... args) {
  GlobalLogger::Log(level, fmt::format(fmtStr, std::forward<Args>(args)...), loc);
}

}  // namespace detail

inline void LogTrace(const std::string& msg, const std::source_location& loc = std::source_location::current()) {
  detail::Log(LogLevel::Trace, loc, msg);
}

inline void LogDebug(const std::string& msg, const std::source_location& loc = std::source_location::current()) {
  detail::Log(LogLevel::Debug, loc, msg);
}

inline void LogInfo(const std::string& msg, const std::source_location& loc = std::source_location::current()) {
  detail::Log(LogLevel::Info, loc, msg);
}

inline void LogWarn(const std::string& msg, const std::source_location& loc = std::source_location::current()) {
  detail::Log(LogLevel::Warning, loc, msg);
}

inline void LogError(const std::string& msg, const std::source_location& loc = std::source_location::current()) {
  detail::Log(LogLevel::Error, loc, msg);
}

inline void LogFatal(const std::string& msg, const std::source_location& loc = std::source_location::current()) {
  detail::Log(LogLevel::Fatal, loc, msg);
}

}  // namespace arise

#ifndef ARISE_DISABLE_LOGGING
#define LOG_TRACE(...) arise::detail::Log(LogLevel::Trace, std::source_location::current(), __VA_ARGS__)
#define LOG_DEBUG(...) arise::detail::Log(LogLevel::Debug, std::source_location::current(), __VA_ARGS__)
#define LOG_INFO(...)  arise::detail::Log(LogLevel::Info, std::source_location::current(), __VA_ARGS__)
#define LOG_WARN(...)  arise::detail::Log(LogLevel::Warning, std::source_location::current(), __VA_ARGS__)
#define LOG_ERROR(...) arise::detail::Log(LogLevel::Error, std::source_location::current(), __VA_ARGS__)
#define LOG_FATAL(...) arise::detail::Log(LogLevel::Fatal, std::source_location::current(), __VA_ARGS__)
#else
#define LOG_TRACE(...)
#define LOG_DEBUG(...)
#define LOG_INFO(...)
#define LOG_WARN(...)
#define LOG_ERROR(...)
#define LOG_FATAL(...)
#endif

#endif  // ARISE_LOG_H