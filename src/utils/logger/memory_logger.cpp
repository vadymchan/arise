#include "utils/logger/memory_logger.h"

#include <chrono>
#include <filesystem>

namespace arise {

void MemoryLogger::log(LogLevel logLevel, const std::string& message, const std::source_location& loc) {
  auto file = std::filesystem::path(loc.file_name()).filename().string();

  std::string functionName{loc.function_name()};
  // strip off the parameter list (remove everything from '(' onward)
  if (auto p = functionName.find('('); p != std::string::npos) {
    functionName.erase(p);
  }
  // strip off any namespace qualifiers (remove up to the last "::")
  if (auto ns = functionName.rfind("::"); ns != std::string::npos) {
    functionName.erase(0, ns + 2);
  }

  LogEntry entry{std::chrono::system_clock::now(), logLevel, file, static_cast<int>(loc.line()), functionName, message};

  m_entries_.push_back(std::move(entry));
  if (m_maxEntries_ > 0 && m_entries_.size() > m_maxEntries_) {
    m_entries_.erase(m_entries_.begin());
  }
}

const std::vector<LogEntry>& MemoryLogger::getLogEntries() const {
  return m_entries_;
}

void MemoryLogger::setMaxEntries(size_t maxEntries) {
  m_maxEntries_ = maxEntries;
}

size_t MemoryLogger::getMaxEntries() const {
  return m_maxEntries_;
}

}  // namespace arise
