#ifndef ARISE_COMPONENT_LOADERS_H
#define ARISE_COMPONENT_LOADERS_H

#include "config/config.h"
#include "ecs/entity.h"
#include "scene/scene.h"

namespace arise {
namespace ecs {

struct Transform;
struct Camera;
struct Light;
struct DirectionalLight;
struct PointLight;
struct SpotLight;

Transform        g_loadTransform(const ConfigValue& value);
Camera           g_loadCamera(const ConfigValue& value);
Light            g_loadLight(const ConfigValue& value);
DirectionalLight g_loadDirectionalLight(const ConfigValue& value);
PointLight       g_loadPointLight(const ConfigValue& value);
SpotLight        g_loadSpotLight(const ConfigValue& value);

Entity g_createEntityFromConfig(Registry& registry, const ConfigValue& entityConfig);
void   g_processEntityComponents(Registry& registry, Entity entity, const ConfigValue& components);

}  // namespace ecs
}  // namespace arise

#endif  // ARISE_COMPONENT_LOADERS_H
