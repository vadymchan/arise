#ifndef ARISE_HOT_RELOAD_MANAGER_H
#define ARISE_HOT_RELOAD_MANAGER_H

#include "utils/file_watcher/file_watcher_manager.h"

namespace arise {

class HotReloadManager {
  public:
  // TODO: consider using FileWatcherManager Callback
  using Callback = std::function<void(const wtr::event&)>;

  void watchFileModifications(const std::filesystem::path& dirPath, const Callback& onChange);
};

}  // namespace arise

#endif  // ARISE_HOT_RELOAD_MANAGER_H
