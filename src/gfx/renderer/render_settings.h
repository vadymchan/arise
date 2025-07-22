#ifndef ARISE_RENDER_SETTINGS_H
#define ARISE_RENDER_SETTINGS_H

#include "core/application_mode.h"

#include <math_library/dimension.h>

namespace arise {
namespace gfx {
namespace renderer {

enum class RenderMode {
  Solid,
  Wireframe,
  NormalMapVisualization,
  VertexNormalVisualization,
  ShaderOverdraw,
  LightVisualization,
  WorldGrid,
  MeshHighlight,
  BoundingBoxVisualization,
};

enum class PostProcessMode {
  None,
  Grayscale,
  ColorInversion
};

struct RenderSettings {
  RenderMode             renderMode              = RenderMode::Solid;
  PostProcessMode        postProcessMode         = PostProcessMode::None;
  math::Dimension2i      renderViewportDimension = math::Dimension2i(1, 1);
  arise::ApplicationMode appMode                 = arise::ApplicationMode::Standalone;
};

}  // namespace renderer
}  // namespace gfx
}  // namespace arise

#endif  // ARISE_RENDER_SETTINGS_H