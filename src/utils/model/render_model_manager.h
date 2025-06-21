#ifndef ARISE_RENDER_MODEL_MANAGER_H
#define ARISE_RENDER_MODEL_MANAGER_H

#include "utils/model/render_model_loader_manager.h"
#include "utils/service/service_locator.h"

#include <filesystem>
#include <memory>
#include <shared_mutex>
#include <unordered_map>

namespace arise {

class RenderModelManager {
  public:
  RenderModelManager() = default;
  ~RenderModelManager();

  RenderModel* getRenderModel(const std::filesystem::path& filepath, Model** outModel = nullptr);

  bool hasRenderModel(const std::filesystem::path& filepath) const;

  bool removeRenderModel(RenderModel* renderModel);

  private:
  std::unordered_map<std::filesystem::path, std::unique_ptr<RenderModel>> renderModelCache_;
  mutable std::shared_mutex                                               mutex_;
};

}  // namespace arise

#endif  // ARISE_RENDER_MODEL_MANAGER_H
