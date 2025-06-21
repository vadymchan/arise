#include "utils/file_watcher/file_watcher_manager.h"

#include "utils/logger/global_logger.h"

namespace arise {

void FileWatcherManager::addWatcher(const std::filesystem::path& dirPath,
                                    const Callback&              callback) {
  std::filesystem::path modifiedDirPath = "./" / dirPath;

  if (m_watchers_.contains(modifiedDirPath)) {
    GlobalLogger::Log(
        LogLevel::Warning,
                      "Watcher for path " + modifiedDirPath.string() + " already exists! Overriding previous watcher.");
  }

  m_watchers_[modifiedDirPath]
      = std::make_unique<wtr::watcher::watch>(modifiedDirPath, callback);
}

void FileWatcherManager::removeWatcher(const std::filesystem::path& dirPath) {
  if (m_watchers_.contains(dirPath)) {
    m_watchers_.erase(dirPath);
  } else {
    GlobalLogger::Log(LogLevel::Error,
                      "No watcher for this path: " + dirPath.string());
  }
}

void FileWatcherManager::removeAllWatchers() {
  m_watchers_.clear();
}

}  // namespace arise
