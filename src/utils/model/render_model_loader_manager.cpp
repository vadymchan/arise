#include "utils/model/render_model_loader_manager.h"

namespace arise {
void RenderModelLoaderManager::registerLoader(ModelType modelType, std::shared_ptr<IRenderModelLoader> loader) {
  std::lock_guard<std::mutex> lock(mutex_);
  loaderMap_[modelType] = std::move(loader);
}

std::unique_ptr<RenderModel> RenderModelLoaderManager::loadRenderModel(const std::filesystem::path& filePath,
                                                                       std::optional<Model*>        outModel) {
  std::string extension = filePath.extension().string();
  std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
  ModelType modelType = getModelTypeFromExtension(extension);

  if (modelType == ModelType::UNKNOWN) {
    GlobalLogger::Log(LogLevel::Error, "Unknown model type for extension: " + extension);
    return nullptr;
  }

  IRenderModelLoader* loader = nullptr;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    auto                        it = loaderMap_.find(modelType);
    if (it != loaderMap_.end()) {
      loader = it->second.get();
    }
  }

  if (loader) {
    return loader->loadRenderModel(filePath, outModel);
  }

  GlobalLogger::Log(LogLevel::Error, "No loader found for model type with extension: " + extension);
  return nullptr;
}
}  // namespace arise