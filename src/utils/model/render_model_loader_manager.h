#ifndef ARISE_RENDER_MODEL_LOADER_MANAGER_H
#define ARISE_RENDER_MODEL_LOADER_MANAGER_H

#include "resources/i_render_model_loader.h"
#include "utils/logger/global_logger.h"
#include "utils/model/model_type.h"

#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <unordered_map>

namespace arise {

class RenderModelLoaderManager {
  public:
  RenderModelLoaderManager() = default;

  void registerLoader(ModelType modelType, std::shared_ptr<IRenderModelLoader> loader);

  std::unique_ptr<RenderModel> loadRenderModel(const std::filesystem::path& filePath, Model** outModel = nullptr);

  private:
  std::unordered_map<ModelType, std::shared_ptr<IRenderModelLoader>> loaderMap_;
  std::mutex                                                         mutex_;
};

}  // namespace arise

#endif  // ARISE_RENDER_MODEL_LOADER_MANAGER_H
