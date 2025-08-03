#include "utils/material/material_loader_manager.h"

#include "utils/logger/global_logger.h"

namespace arise {
MaterialType getMaterialTypeFromExtension(const std::string& extension) {
  static const std::unordered_map<std::string, MaterialType> extensionToType = {
    { ".mtl",  MaterialType::MTL},
    { ".fbx",  MaterialType::FBX},
    {".gltf", MaterialType::GLTF},
    { ".glb", MaterialType::GLTF},
  };

  std::string ext = extension;
  std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

  auto it = extensionToType.find(ext);
  if (it != extensionToType.end()) {
    return it->second;
  }

  return MaterialType::UNKNOWN;
}

void MaterialLoaderManager::registerLoader(MaterialType materialType, std::shared_ptr<IMaterialLoader> loader) {
  std::lock_guard<std::mutex> lock(mutex_);
  loaderMap_[materialType] = std::move(loader);
}

std::vector<std::unique_ptr<ecs::Material>> MaterialLoaderManager::loadMaterials(
    const std::filesystem::path& filePath) {
  std::string extension = filePath.extension().string();
  std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
  MaterialType materialType = getMaterialTypeFromExtension(extension);

  if (materialType == MaterialType::UNKNOWN) {
    GlobalLogger::Log(LogLevel::Error, "Unknown material type for extension: " + extension);
    return {};
  }

  IMaterialLoader* loader = nullptr;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    auto                        it = loaderMap_.find(materialType);
    if (it != loaderMap_.end()) {
      loader = it->second.get();
    }
  }

  if (loader) {
    return loader->loadMaterials(filePath);
  }

  GlobalLogger::Log(LogLevel::Error, "No suitable loader found for material type: " + extension);
  return {};
}
}  // namespace arise