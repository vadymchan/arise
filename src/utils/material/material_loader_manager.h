#ifndef ARISE_MATERIAL_LOADER_MANAGER_H
#define ARISE_MATERIAL_LOADER_MANAGER_H

#include "resources/i_material_loader.h"

#include <filesystem>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace arise {

enum class MaterialType {
  MTL,   // Wavefront (OBJ) .mtl files
  FBX,   // FBX material files
  GLTF,  // glTF material definitions
  UNKNOWN,
};

MaterialType getMaterialTypeFromExtension(const std::string& extension);

class MaterialLoaderManager {
  public:
  MaterialLoaderManager() = default;

  void registerLoader(MaterialType materialType, std::shared_ptr<IMaterialLoader> loader);

  std::vector<std::unique_ptr<ecs::Material>> loadMaterials(const std::filesystem::path& filePath);

  private:
  std::unordered_map<MaterialType, std::shared_ptr<IMaterialLoader>> loaderMap_;
  std::mutex                                                         mutex_;
};

}  // namespace arise

#endif  // ARISE_MATERIAL_LOADER_MANAGER_H
