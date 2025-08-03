#include "ecs/systems/render_system.h"

#include "ecs/components/model.h"
#include "ecs/components/render_model.h"
#include "ecs/components/transform.h"
#include "utils/logger/global_logger.h"

namespace arise {
namespace ecs {

// TODO: currently not implemented and not used (it's responsible for organizing
// and preparing entity data for rendering (different render passes), applying
// culling and LOD selection
void RenderSystem::update(Scene* scene, float deltaTime) {
  Registry& registry = scene->getEntityRegistry();

  // Handle entities with RenderModel and Transform components
  auto modelView = registry.view<Transform, RenderModel>();

  for (auto entity : modelView) {
    auto& renderModel = modelView.get<RenderModel>(entity);

    GlobalLogger::Log(
        LogLevel::Info,
        "Entity " + std::to_string(static_cast<int>(entity)) + " has RenderModel: " + renderModel.filePath.string());
  }

  // Handle entities with RenderMesh components
  auto meshView = registry.view<RenderMesh>();

  for (auto entity : meshView) {
    auto& renderMesh = meshView.get<RenderMesh>(entity);

    if (renderMesh.material) {
      GlobalLogger::Log(
          LogLevel::Info,
          "Entity " + std::to_string(static_cast<int>(entity)) + " has Material: " + renderMesh.material->materialName);
    }
  }
}

}  // namespace ecs
}  // namespace arise