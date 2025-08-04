#ifndef ARISE_MESH_H
#define ARISE_MESH_H

#include "ecs/components/bounding_volume.h"
#include "ecs/components/vertex.h"

#include <string>
#include <vector>

namespace arise {
namespace ecs {

// This is the geometry data on CPU side (imported from cgltf)
struct Mesh {
  std::string           meshName;
  std::vector<Vertex>   vertices;
  std::vector<uint32_t> indices;
  math::Matrix4f<>      transformMatrix = math::Matrix4f<>::Identity();
  BoundingBox           boundingBox;  // in mesh local space
};

}  // namespace ecs
}  // namespace arise

#endif  // ARISE_MESH_H
