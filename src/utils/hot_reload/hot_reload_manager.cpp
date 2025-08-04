#include "utils/hot_reload/hot_reload_manager.h"

#include "utils/service/service_locator.h"

namespace arise {

void HotReloadManager::watchFileModifications(const std::filesystem::path& dirPath, const Callback& onChange) {
  auto fileWatcher = ServiceLocator::s_get<FileWatcherManager>();
  fileWatcher->addWatcher(dirPath, [onChange](const wtr::event& e) {
    if (e.effect_type == wtr::event::effect_type::modify) {
      onChange(e);
    }
  });
}

}  // namespace arise
