#ifndef ARISE_MODEL_LOADER_MANAGER_H
#define ARISE_MODEL_LOADER_MANAGER_H

#include "resources/i_model_loader.h"
#include "utils/logger/global_logger.h"
#include "utils/model/model_type.h"

#include <filesystem>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace arise {

class ModelLoaderManager {
  public:
  ModelLoaderManager() = default;

  void registerLoader(ModelType modelType, std::shared_ptr<IModelLoader> loader);

  std::unique_ptr<ecs::Model> loadModel(const std::filesystem::path& filePath);

  private:
  std::unordered_map<ModelType, std::shared_ptr<IModelLoader>> loaderMap_;
  std::mutex                                                   mutex_;
};

}  // namespace arise

#endif  // ARISE_MODEL_LOADER_MANAGER_H
