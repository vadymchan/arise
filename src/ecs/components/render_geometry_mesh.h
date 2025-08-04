#ifndef ARISE_RENDER_GEOMETRY_MESH_H
#define ARISE_RENDER_GEOMETRY_MESH_H

#include "gfx/rhi/interface/buffer.h"

#include <cstdint>

namespace arise {
namespace ecs {

// GPU-Side Mesh Geometry data
// TODO: can we somehow manage vertex and index buffers? so we can have 1 big
// vertex buffer for model and each mesh will reffer to the part of it
struct RenderGeometryMesh {
  gfx::rhi::Buffer* vertexBuffer;
  gfx::rhi::Buffer* indexBuffer;

  // TODO
  // for convenience
  // uint32_t vertexCount = 0;
  // uint32_t indexCount  = 0;
  // for vertex binding
  // uint32_t vertexStride = 0;
};

}  // namespace ecs
}  // namespace arise

#endif  // ARISE_RENDER_GEOMETRY_MESH_H
