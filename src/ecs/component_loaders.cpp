#include "ecs/component_loaders.h"

#include "components/light.h"
#include "components/movement.h"
#include "components/render_model.h"
#include "config/config_manager.h"
#include "ecs/components/camera.h"
#include "ecs/components/transform.h"
#include "utils/logger/global_logger.h"
#include "utils/model/render_model_manager.h"
#include "utils/path_manager/path_manager.h"
#include "utils/service/service_locator.h"

namespace game_engine {

Transform g_loadTransform(const ConfigValue& value) {
  Transform transform;

  transform.translation = math::Vector3Df(0.0f, 0.0f, 0.0f);
  transform.rotation    = math::Vector3Df(0.0f, 0.0f, 0.0f);
  transform.scale       = math::Vector3Df(1.0f, 1.0f, 1.0f);

  if (value.HasMember("position") && value["position"].IsArray()) {
    auto position = value["position"].GetArray();
    for (size_t i = 0; i < position.Size() && i < 3; i++) {
      if (position[i].IsNumber()) {
        transform.translation.coeffRef(i) = position[i].GetFloat();
      }
    }
  }

  if (value.HasMember("rotation") && value["rotation"].IsArray()) {
    auto rotation = value["rotation"].GetArray();
    for (size_t i = 0; i < rotation.Size() && i < 3; i++) {
      if (rotation[i].IsNumber()) {
        transform.rotation.coeffRef(i) = rotation[i].GetFloat();
      }
    }
  }

  if (value.HasMember("scale") && value["scale"].IsArray()) {
    auto scale = value["scale"].GetArray();
    for (size_t i = 0; i < scale.Size() && i < 3; i++) {
      if (scale[i].IsNumber()) {
        transform.scale.coeffRef(i) = scale[i].GetFloat();
      }
    }
  }

  return transform;
}

Camera g_loadCamera(const ConfigValue& value) {
  Camera camera;

  if (value.HasMember("cameraType") && value["cameraType"].IsString()) {
    std::string typeStr = value["cameraType"].GetString();
    if (typeStr == "perspective") {
      camera.type = CameraType::Perspective;
    } else if (typeStr == "orthographic") {
      camera.type = CameraType::Orthographic;
    } else {
      GlobalLogger::Log(LogLevel::Warning, "Unknown camera type: " + typeStr + ", using perspective as default");
      camera.type = CameraType::Perspective;
    }
  } else if (value.HasMember("cameraType") && value["cameraType"].IsNumber()) {
    camera.type = static_cast<CameraType>(value["cameraType"].GetInt());
  } else {
    GlobalLogger::Log(LogLevel::Warning, "Camera type not specified, using perspective as default");
    camera.type = CameraType::Perspective;
  }

  if (value.HasMember("fov") && value["fov"].IsNumber()) {
    camera.fov = value["fov"].GetFloat();
  }

  if (value.HasMember("near") && value["near"].IsNumber()) {
    camera.nearClip = value["near"].GetFloat();
  }

  if (value.HasMember("far") && value["far"].IsNumber()) {
    camera.farClip = value["far"].GetFloat();
  }

  if (value.HasMember("width") && value["width"].IsNumber()) {
    camera.width = value["width"].GetFloat();
  }

  if (value.HasMember("height") && value["height"].IsNumber()) {
    camera.height = value["height"].GetFloat();
  }

  return camera;
}

Light g_loadLight(const ConfigValue& value) {
  Light light;

  if (value.HasMember("color") && value["color"].IsArray()) {
    auto color = value["color"].GetArray();
    if (color.Size() >= 3) {
      light.color.x() = color[0].GetFloat();
      light.color.y() = color[1].GetFloat();
      light.color.z() = color[2].GetFloat();
    }
  }

  if (value.HasMember("intensity") && value["intensity"].IsNumber()) {
    light.intensity = value["intensity"].GetFloat();
  }

  return light;
}

DirectionalLight g_loadDirectionalLight(const ConfigValue& value) {
  DirectionalLight dirLight;

  if (value.HasMember("direction") && value["direction"].IsArray()) {
    auto direction = value["direction"].GetArray();
    if (direction.Size() >= 3) {
      dirLight.direction.x() = direction[0].GetFloat();
      dirLight.direction.y() = direction[1].GetFloat();
      dirLight.direction.z() = direction[2].GetFloat();
    }
  }

  return dirLight;
}

PointLight g_loadPointLight(const ConfigValue& value) {
  PointLight pointLight;

  if (value.HasMember("range") && value["range"].IsNumber()) {
    pointLight.range = value["range"].GetFloat();
  }

  return pointLight;
}

SpotLight g_loadSpotLight(const ConfigValue& value) {
  SpotLight spotLight;

  if (value.HasMember("range") && value["range"].IsNumber()) {
    spotLight.range = value["range"].GetFloat();
  }

  if (value.HasMember("innerConeAngle") && value["innerConeAngle"].IsNumber()) {
    spotLight.innerConeAngle = value["innerConeAngle"].GetFloat();
  }

  if (value.HasMember("outerConeAngle") && value["outerConeAngle"].IsNumber()) {
    spotLight.outerConeAngle = value["outerConeAngle"].GetFloat();
  }

  return spotLight;
}

std::string LoadModelPath(const ConfigValue& value) {
  if (value.HasMember("path") && value["path"].IsString()) {
    return value["path"].GetString();
  }
  return "";
}

Entity g_createEntityFromConfig(Registry& registry, const ConfigValue& entityConfig) {
  Entity entity = registry.create();

  if (entityConfig.HasMember("name") && entityConfig["name"].IsString()) {
    std::string entityName = entityConfig["name"].GetString();
    GlobalLogger::Log(LogLevel::Info, "Created entity: " + entityName);
  }

  if (entityConfig.HasMember("components") && entityConfig["components"].IsArray()) {
    const auto& components = entityConfig["components"];
    g_processEntityComponents(registry, entity, components);
  }

  return entity;
}

void g_processEntityComponents(Registry& registry, Entity entity, const ConfigValue& components) {
  if (!components.IsArray()) {
    GlobalLogger::Log(LogLevel::Warning, "Components value is not an array");
    return;
  }

  for (auto it = components.Begin(); it != components.End(); ++it) {
    const auto& component = *it;

    if (!component.HasMember("type") || !component["type"].IsString()) {
      GlobalLogger::Log(LogLevel::Warning, "Component missing type field");
      continue;
    }

    std::string componentType = component["type"].GetString();

    if (componentType == "transform") {
      registry.emplace<Transform>(entity, g_loadTransform(component));
    } else if (componentType == "camera") {
      registry.emplace<Camera>(entity, g_loadCamera(component));
    } else if (componentType == "movement") {
      registry.emplace<Movement>(entity);
    } else if (componentType == "model") {
      std::string modelPath = LoadModelPath(component);
      if (!modelPath.empty()) {
        auto modelManager = ServiceLocator::s_get<RenderModelManager>();
        auto renderModel  = modelManager->getRenderModel(modelPath);

        if (renderModel) {
          registry.emplace<RenderModel*>(entity, renderModel);
        } else {
          GlobalLogger::Log(LogLevel::Error, "Failed to load model: " + modelPath);
        }
      }
    } else if (componentType == "light") {
      registry.emplace<Light>(entity, g_loadLight(component));
    } else if (componentType == "directionalLight") {
      registry.emplace<DirectionalLight>(entity, g_loadDirectionalLight(component));
    } else if (componentType == "pointLight") {
      registry.emplace<PointLight>(entity, g_loadPointLight(component));
    } else if (componentType == "spotLight") {
      registry.emplace<SpotLight>(entity, g_loadSpotLight(component));
    } else {
      GlobalLogger::Log(LogLevel::Warning, "Unknown component type: " + componentType);
    }
  }
}
}  // namespace game_engine
