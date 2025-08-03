#ifndef ARISE_MATERIAL_H
#define ARISE_MATERIAL_H

#include "gfx/rhi/interface/texture.h"

#include <math_library/vector.h>

#include <filesystem>
#include <string>
#include <unordered_map>

namespace arise {
namespace ecs {

struct Material {
  std::string materialName;

  std::filesystem::path filePath;

  // TODO: consider adding tags to materials (so we can set specific
  // characteristics of material (e.g. "PBR", "transparent", etc.)
  // std::set<std::string> tags;

  // roughness, metallic
  std::unordered_map<std::string, float> scalarParameters;

  // base color
  std::unordered_map<std::string, math::Vector4f> vectorParameters;

  // Textures associated with the material
  // Keyed by texture name (e.g., "albedo", "normal")
  // TODO: consider create enum class and use it as key
  std::unordered_map<std::string, gfx::rhi::Texture*> textures;
};

}  // namespace ecs
}  // namespace arise

#endif  // ARISE_MATERIAL_H
