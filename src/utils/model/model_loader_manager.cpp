#include "utils/model/model_loader_manager.h"

namespace arise {

void ModelLoaderManager::registerLoader(ModelType modelType, std::shared_ptr<IModelLoader> loader) {
  std::lock_guard<std::mutex> lock(mutex_);
  loaderMap_[modelType] = std::move(loader);
}

std::unique_ptr<ecs::Model> ModelLoaderManager::loadModel(const std::filesystem::path& filePath) {
  std::string extension = filePath.extension().string();
  std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
  ModelType modelType = getModelTypeFromExtension(extension);

  if (modelType == ModelType::UNKNOWN) {
    GlobalLogger::Log(LogLevel::Error, "Unknown model type for extension: " + extension);
    return nullptr;
  }

  IModelLoader* loader = nullptr;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    auto                        it = loaderMap_.find(modelType);
    if (it != loaderMap_.end()) {
      loader = it->second.get();
    }
  }

  if (loader) {
    return loader->loadModel(filePath);
  }

  GlobalLogger::Log(LogLevel::Error, "No loader found for model type with extension: " + extension);
  return nullptr;
}

}  // namespace arise
