#include "utils/model/render_model_manager.h"

#include "utils/logger/log.h"
#include "utils/model/model_manager.h"
#include "utils/model/render_mesh_manager.h"

#include <shared_mutex>

namespace arise {
RenderModelManager::~RenderModelManager() {
  if (!renderModelCache_.empty()) {
    LOG_INFO("RenderModelManager destroyed, releasing " + std::to_string(renderModelCache_.size()) + " render models");

    for (const auto& [path, renderModel] : renderModelCache_) {
      LOG_INFO("Released render model: " + path.string());
    }
  }
}

ecs::RenderModel* RenderModelManager::getRenderModel(const std::filesystem::path& filepath, ecs::Model** outModel) {
  {
    std::shared_lock<std::shared_mutex> readLock(mutex_);
    auto                                it = renderModelCache_.find(filepath);
    if (it != renderModelCache_.end()) {
      if (outModel) {
        auto modelManager = ServiceLocator::s_get<ModelManager>();
        if (modelManager) {
          *outModel = modelManager->getModel(filepath);
          LOG_DEBUG("CPU model retrieved from cache for: " + filepath.string());
        } else {
          LOG_WARN("ModelManager not available for cached model: " + filepath.string());
          *outModel = nullptr;
        }
      }
      return it->second.get();
    }
  }

  auto renderModelLoaderManager = ServiceLocator::s_get<RenderModelLoaderManager>();
  if (!renderModelLoaderManager) {
    LOG_ERROR("RenderModelLoaderManager not available in ServiceLocator.");
    return nullptr;
  }

  auto renderModel = renderModelLoaderManager->loadRenderModel(filepath, outModel);

  if (renderModel) {
    ecs::RenderModel* modelPtr = renderModel.get();
    {
      std::unique_lock<std::shared_mutex> writeLock(mutex_);

      auto it = renderModelCache_.find(filepath);
      if (it != renderModelCache_.end()) {
        return it->second.get();
      }
      renderModelCache_[filepath] = std::move(renderModel);
    }
    return modelPtr;
  }

  LOG_WARN("Failed to load render model: " + filepath.string());
  return nullptr;
}

bool RenderModelManager::hasRenderModel(const std::filesystem::path& filepath) const {
  std::shared_lock<std::shared_mutex> readLock(mutex_);
  return renderModelCache_.find(filepath) != renderModelCache_.end();
}

bool RenderModelManager::removeRenderModel(ecs::RenderModel* renderModel) {
  if (!renderModel) {
    LOG_ERROR("Cannot remove null render model");
    return false;
  }

  LOG_INFO("Removing render model: " + renderModel->filePath.string());

  auto renderMeshManager = ServiceLocator::s_get<RenderMeshManager>();

  if (renderMeshManager) {
    for (auto* renderMesh : renderModel->renderMeshes) {
      if (renderMesh) {
        renderMeshManager->removeRenderMesh(renderMesh);
      }
    }
  }

  std::unique_lock<std::shared_mutex> writeLock(mutex_);

  auto it = std::find_if(renderModelCache_.begin(), renderModelCache_.end(), [renderModel](const auto& pair) {
    return pair.second.get() == renderModel;
  });

  if (it != renderModelCache_.end()) {
    renderModelCache_.erase(it);
    return true;
  }

  LOG_WARN("Render model not found in manager");
  return false;
}

}  // namespace arise