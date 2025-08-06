#ifdef ARISE_USE_CGLTF

#include "resources/cgltf/cgltf_material_loader.h"

#include "resources/cgltf/cgltf_common.h"
#include "utils/image/image_manager.h"
#include "utils/logger/log.h"
#include "utils/service/service_locator.h"
#include "utils/texture/texture_manager.h"

#define CGLTF_IMPLEMENTATION
#include <cgltf.h>

namespace arise {

std::vector<std::unique_ptr<ecs::Material>> CgltfMaterialLoader::loadMaterials(const std::filesystem::path& filePath) {
  LOG_INFO("Loading materials from " + filePath.string());

  auto scene = CgltfSceneCache::getOrLoad(filePath);
  if (!scene) {
    LOG_ERROR("Failed to load GLTF scene: " + filePath.string());
    return {};
  }
  const cgltf_data* data = scene.get();

  auto materials = processMaterials(data, filePath);

  LOG_INFO("Successfully loaded " + std::to_string(materials.size()) + " material(s) from " + filePath.string());

  return materials;
}

std::vector<std::unique_ptr<ecs::Material>> CgltfMaterialLoader::processMaterials(
    const cgltf_data* data, const std::filesystem::path& filePath) {
  std::vector<std::unique_ptr<ecs::Material>> materials;
  materials.reserve(data->materials_count);

  for (size_t i = 0; i < data->materials_count; ++i) {
    auto material = processMaterial(&data->materials[i], filePath, i);
    if (material) {
      materials.push_back(std::move(material));
    }
  }

  return materials;
}

std::unique_ptr<ecs::Material> CgltfMaterialLoader::processMaterial(const cgltf_material*        material,
                                                                    const std::filesystem::path& filePath,
                                                                    size_t                       materialIndex) {
  auto outMaterial = std::make_unique<ecs::Material>();

  outMaterial->materialName = material->name ? material->name : "Material_" + std::to_string(materialIndex);

  outMaterial->filePath = filePath;

  if (material->has_pbr_metallic_roughness) {
    const auto& pbr = material->pbr_metallic_roughness;

    outMaterial->vectorParameters["base_color"] = math::Vector4f(
        pbr.base_color_factor[0], pbr.base_color_factor[1], pbr.base_color_factor[2], pbr.base_color_factor[3]);

    outMaterial->scalarParameters["metallic"] = pbr.metallic_factor;

    outMaterial->scalarParameters["roughness"] = pbr.roughness_factor;
  }

  if (material->alpha_mode == cgltf_alpha_mode_opaque) {
    outMaterial->scalarParameters["opacity"] = 1.0f;
  } else if (material->alpha_mode == cgltf_alpha_mode_blend) {
    outMaterial->scalarParameters["opacity"] = material->pbr_metallic_roughness.base_color_factor[3];
  }

  loadTextures(material, outMaterial.get(), filePath.parent_path());

  return outMaterial;
}

void CgltfMaterialLoader::loadTextures(const cgltf_material*        material,
                                       ecs::Material*               outMaterial,
                                       const std::filesystem::path& basePath) {
  // base color
  if (material->pbr_metallic_roughness.base_color_texture.texture) {
    const auto* texture = material->pbr_metallic_roughness.base_color_texture.texture;
    const auto* image   = texture->image;

    if (image == nullptr && texture->has_basisu && texture->basisu_image) {
      image = texture->basisu_image;
    } else {
      LOG_WARN("Image is null for base color texture");
    }

    if (image) {
      auto textureResource = loadTexture(image, basePath, "albedo");
      if (textureResource) {
        outMaterial->textures["albedo"] = textureResource;
      }
    }
  }

  // metallic-roughness
  if (material->pbr_metallic_roughness.metallic_roughness_texture.texture) {
    const auto* texture = material->pbr_metallic_roughness.metallic_roughness_texture.texture;
    const auto* image   = texture->image;

    if (image == nullptr && texture->has_basisu && texture->basisu_image) {
      image = texture->basisu_image;
    } else {
      LOG_WARN("Image is null for metallic roughness texture");
    }

    if (image) {
      auto textureResource = loadTexture(image, basePath, "metallic_roughness");
      if (textureResource) {
        outMaterial->textures["metallic_roughness"] = textureResource;
      }
    }
  }

  // normal map
  if (material->normal_texture.texture) {
    const auto* texture = material->normal_texture.texture;
    const auto* image   = texture->image;

    if (image == nullptr && texture->has_basisu && texture->basisu_image) {
      image = texture->basisu_image;
    } else {
      LOG_WARN("Image is null for normal map texture");
    }

    if (image) {
      auto textureResource = loadTexture(image, basePath, "normal_map");
      if (textureResource) {
        outMaterial->textures["normal_map"] = textureResource;
      }
    }
  }
}

gfx::rhi::Texture* CgltfMaterialLoader::loadTexture(const cgltf_image*           image,
                                                    const std::filesystem::path& basePath,
                                                    const std::string&           textureName) {
  std::filesystem::path texturePath;

  if (image->uri) {
    texturePath = basePath / image->uri;
  } else if (image->buffer_view) {
    LOG_WARN("Embedded textures not fully implemented");
    return nullptr;
  } else {
    LOG_ERROR("Invalid image source in GLTF");
    return nullptr;
  }

  auto imageManager = ServiceLocator::s_get<ImageManager>();
  if (!imageManager) {
    LOG_ERROR("ImageManager not available");
    return nullptr;
  }

  auto imagePtr = imageManager->getImage(texturePath);
  if (!imagePtr) {
    LOG_ERROR("Failed to load image: " + texturePath.string());
    return nullptr;
  }

  auto textureManager = ServiceLocator::s_get<TextureManager>();
  if (!textureManager) {
    LOG_ERROR("TextureManager not found in ServiceLocator");
    return nullptr;
  }

  std::string uniqueTextureName = texturePath.filename().string();

  auto texturePtr = textureManager->getTexture(uniqueTextureName);
  if (!texturePtr) {
    texturePtr = textureManager->createTexture(imagePtr, uniqueTextureName);
  }

  return texturePtr;
}

}  // namespace arise

#endif  // ARISE_USE_CGLTF