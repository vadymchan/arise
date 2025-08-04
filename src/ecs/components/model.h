#ifndef ARISE_MODEL_H
#define ARISE_MODEL_H

#include "ecs/components/bounding_volume.h"
#include "ecs/components/mesh.h"

#include <filesystem>
#include <memory>

namespace arise {
namespace ecs {

// This is the geometry data on CPU side (imported from cgltf)
struct Model {
  // TODO: no modelNames (consider adding it, maybe filename is already okay)
  std::filesystem::path filePath;
  std::vector<Mesh*>    meshes;
  BoundingBox           boundingBox;  // in model space
};

}  // namespace ecs
}  // namespace arise

#endif  // ARISE_MODEL_H
