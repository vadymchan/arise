#ifndef ARISE_I_RENDER_MODEL_LOADER_H
#define ARISE_I_RENDER_MODEL_LOADER_H

#include "ecs/components/model.h"
#include "ecs/components/render_model.h"

#include <optional>

namespace arise {

// we made RenderModelLoader friend to ModelLoader and MaterialLoader so we can
// access. As an alternative we can expose private methods to public.

// TODO: consider merge RenderModelLoader and ModelLoader (since this may
// confuse users)
class IRenderModelLoader {
  public:
  virtual ~IRenderModelLoader() = default;

  // Optionally returns CPU-side geometry model (outModel)
  virtual std::unique_ptr<ecs::RenderModel> loadRenderModel(const std::filesystem::path& filepath,
                                                            ecs::Model**                 outModel = nullptr)
      = 0;
};

}  // namespace arise

#endif  // ARISE_I_RENDER_MODEL_LOADER_H
