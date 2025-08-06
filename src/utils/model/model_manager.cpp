#include "utils/model/model_manager.h"

#include "utils/logger/log.h"

namespace arise {
ecs::Model* ModelManager::getModel(const std::filesystem::path& filepath) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto                        it = modelCache_.find(filepath);
  if (it != modelCache_.end()) {
    return it->second.get();
  }

  auto modelLoaderManager = ServiceLocator::s_get<ModelLoaderManager>();
  if (!modelLoaderManager) {
    LOG_ERROR("ModelLoaderManager not available in ServiceLocator.");
    return nullptr;
  }

  auto model = modelLoaderManager->loadModel(filepath);
  if (model) {
    ecs::Model* modelPtr  = model.get();
    modelCache_[filepath] = std::move(model);
    return modelPtr;
  }

  LOG_WARN("Failed to load model: " + filepath.string());
  return nullptr;
}
}  // namespace arise