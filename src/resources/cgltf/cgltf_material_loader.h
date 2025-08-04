#ifndef ARISE_GLTF_MATERIAL_LOADER_H
#define ARISE_GLTF_MATERIAL_LOADER_H

#ifdef ARISE_USE_CGLTF

#include "gfx/rhi/interface/device.h"
#include "resources/i_material_loader.h"
#include "resources/image.h"

// Forward declarations
struct cgltf_data;
struct cgltf_material;
struct cgltf_image;

namespace arise {

class CgltfMaterialLoader : public IMaterialLoader {
  public:
  CgltfMaterialLoader()  = default;
  ~CgltfMaterialLoader() = default;

  std::vector<std::unique_ptr<ecs::Material>> loadMaterials(const std::filesystem::path& filePath) override;

  private:
  std::vector<std::unique_ptr<ecs::Material>> processMaterials(const cgltf_data*            data,
                                                               const std::filesystem::path& filePath);

  std::unique_ptr<ecs::Material> processMaterial(const cgltf_material*        material,
                                                 const std::filesystem::path& filePath,
                                                 size_t                       materialIndex);

  void loadTextures(const cgltf_material* material, ecs::Material* outMaterial, const std::filesystem::path& basePath);

  gfx::rhi::Texture* loadTexture(const cgltf_image*           image,
                                 const std::filesystem::path& basePath,
                                 const std::string&           textureName);
};

}  // namespace arise

#endif  // ARISE_USE_CGLTF

#endif  // ARISE_GLTF_MATERIAL_LOADER_H