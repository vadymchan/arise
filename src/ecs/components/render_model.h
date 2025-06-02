#ifndef ARISE_RENDER_MODEL_H
#define ARISE_RENDER_MODEL_H

#include "render_mesh.h"

#include "gfx/rhi/interface/buffer.h"

#include <filesystem>
#include <memory>
#include <vector>

namespace arise {

struct RenderModel {
  std::filesystem::path    filePath;
  std::vector<RenderMesh*> renderMeshes;
};

}  // namespace arise

#endif  // ARISE_RENDER_MODEL_H
