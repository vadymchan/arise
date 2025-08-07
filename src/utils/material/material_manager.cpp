#include "utils/material/material_manager.h"

#include "utils/logger/log.h"
#include "utils/material/material_loader_manager.h"
#include "utils/service/service_locator.h"
#include "utils/texture/texture_manager.h"

namespace arise {
MaterialManager::~MaterialManager() {
  std::lock_guard<std::mutex> lock(m_mutex_);
  if (!materialCache_.empty()) {
    size_t totalMaterials = 0;
    for (const auto& [path, materials] : materialCache_) {
      totalMaterials += materials.size();
    }

    LOG_INFO("MaterialManager destroyed, releasing {} materials from {} files", totalMaterials, materialCache_.size());

    for (const auto& [path, materials] : materialCache_) {
      for (const auto& material : materials) {
        LOG_INFO("Released material: {} from {}", material->materialName, path.filename().string());
      }
    }
  }
}

std::vector<ecs::Material*> MaterialManager::getMaterials(const std::filesystem::path& filepath) {
  std::lock_guard<std::mutex> lock(m_mutex_);

  auto it = materialCache_.find(filepath);
  if (it != materialCache_.end()) {
    std::vector<ecs::Material*> result;
    for (const auto& material : it->second) {
      result.push_back(material.get());
    }
    return result;
  }

  auto materialLoaderManager = ServiceLocator::s_get<MaterialLoaderManager>();
  if (!materialLoaderManager) {
    LOG_ERROR("MaterialLoaderManager not available in ServiceLocator.");
    return {};
  }

  auto materials = materialLoaderManager->loadMaterials(filepath);
  if (!materials.empty()) {
    std::vector<ecs::Material*> result;

    auto& cacheEntry = materialCache_[filepath];

    for (auto& material : materials) {
      result.push_back(material.get());

      cacheEntry.push_back(std::move(material));
    }

    return result;
  }

  LOG_WARN("Failed to load materials from: {}", filepath.string());
  return {};
}

bool MaterialManager::removeMaterial(ecs::Material* material) {
  if (!material) {
    LOG_ERROR("Cannot remove null material");
    return false;
  }

  std::lock_guard<std::mutex> lock(m_mutex_);

  for (auto it = materialCache_.begin(); it != materialCache_.end(); ++it) {
    auto& materialVec = it->second;
    auto  materialIt  = std::find_if(materialVec.begin(),
                                   materialVec.end(),
                                   [material](const std::unique_ptr<ecs::Material>& m) { return m.get() == material; });

    if (materialIt != materialVec.end()) {
      LOG_INFO("Removing material: {}", material->materialName);

      auto textureManager = ServiceLocator::s_get<TextureManager>();
      if (textureManager) {
        for (const auto& [textureName, texturePtr] : material->textures) {
          if (texturePtr) {
            LOG_DEBUG("Releasing texture '{}' from material '{}'", textureName, material->materialName);
            textureManager->removeTexture(texturePtr);
          }
        }
      } else {
        LOG_WARN("TextureManager not available, textures may not be properly released");
      }

      LOG_INFO("Material '{}' deleted", material->materialName);

      materialVec.erase(materialIt);
      if (materialVec.empty()) {
        materialCache_.erase(it);
      }
      return true;
    }
  }

  LOG_DEBUG("Material not found in manager (may have been removed already)");
  return false;
}
}  // namespace arise