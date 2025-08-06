#include "scene/scene_saver.h"

#include "ecs/components/camera.h"
#include "ecs/components/light.h"
#include "ecs/components/movement.h"
#include "ecs/components/render_model.h"
#include "ecs/components/transform.h"
#include "file_loader/file_system_manager.h"
#include "utils/logger/log.h"
#include "utils/math/math_util.h"

#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>

namespace arise {

bool SceneSaver::saveScene(Scene* scene, const std::string& sceneName, const std::filesystem::path& filePath) {
  if (!scene) {
    LOG_ERROR("Cannot save null scene");
    return false;
  }

  rapidjson::Document document;
  document.SetObject();
  auto& allocator = document.GetAllocator();

  document.AddMember("schemaVersion", rapidjson::Value("1.0", allocator), allocator);

  document.AddMember("name", rapidjson::Value(sceneName.c_str(), allocator), allocator);

  rapidjson::Value entitiesArray(rapidjson::kArrayType);

  serializeRegistry(scene->getEntityRegistry(), document, allocator);

  rapidjson::StringBuffer                          buffer;
  rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
  document.Accept(writer);

  bool success = FileSystemManager::writeFile(filePath, buffer.GetString());
  if (success) {
    LOG_INFO("Scene saved to: " + filePath.string());
  } else {
    LOG_ERROR("Failed to save scene to: " + filePath.string());
  }

  return success;
}

void SceneSaver::serializeRegistry(const Registry&                   registry,
                                   rapidjson::Document&              document,
                                   rapidjson::MemoryPoolAllocator<>& allocator) {
  rapidjson::Value entitiesArray(rapidjson::kArrayType);

  auto view = registry.view<entt::entity>();

  for (auto entity : view) {
    rapidjson::Value entityObject(rapidjson::kObjectType);

    std::string entityName = "Entity_" + std::to_string(static_cast<uint32_t>(entity));

    if (registry.all_of<ecs::Camera>(entity)) {
      entityName = "MainCamera";
    } else if (registry.all_of<ecs::RenderModel*>(entity)) {
      auto* model = registry.get<ecs::RenderModel*>(entity);
      if (model && !model->filePath.empty()) {
        entityName = "Model_" + model->filePath.filename().string();
      }
    } else if (registry.all_of<ecs::Light, ecs::DirectionalLight>(entity)) {
      entityName = "DirectionalLight";
    } else if (registry.all_of<ecs::Light, ecs::PointLight>(entity)) {
      entityName = "PointLight";
    } else if (registry.all_of<ecs::Light, ecs::SpotLight>(entity)) {
      entityName = "SpotLight";
    }

    entityObject.AddMember("name", rapidjson::Value(entityName.c_str(), allocator), allocator);

    rapidjson::Value componentsArray(rapidjson::kArrayType);

    if (registry.all_of<ecs::Transform>(entity)) {
      rapidjson::Value componentObject(rapidjson::kObjectType);
      componentObject.AddMember("type", rapidjson::Value("transform", allocator), allocator);

      const auto& transform = registry.get<ecs::Transform>(entity);
      serializeTransform(transform, componentObject, allocator);

      componentsArray.PushBack(componentObject, allocator);
    }

    if (registry.all_of<ecs::Camera>(entity)) {
      rapidjson::Value componentObject(rapidjson::kObjectType);
      componentObject.AddMember("type", rapidjson::Value("camera", allocator), allocator);

      const auto& camera = registry.get<ecs::Camera>(entity);
      serializeCamera(camera, componentObject, allocator);

      componentsArray.PushBack(componentObject, allocator);
    }

    if (registry.all_of<ecs::Movement>(entity)) {
      rapidjson::Value componentObject(rapidjson::kObjectType);
      componentObject.AddMember("type", rapidjson::Value("movement", allocator), allocator);

      componentsArray.PushBack(componentObject, allocator);
    }

    if (registry.all_of<ecs::RenderModel*>(entity)) {
      rapidjson::Value componentObject(rapidjson::kObjectType);
      componentObject.AddMember("type", rapidjson::Value("model", allocator), allocator);

      auto* model = registry.get<ecs::RenderModel*>(entity);
      serializeModel(model, componentObject, allocator);

      componentsArray.PushBack(componentObject, allocator);
    }

    if (registry.all_of<ecs::Light>(entity)) {
      rapidjson::Value componentObject(rapidjson::kObjectType);
      componentObject.AddMember("type", rapidjson::Value("light", allocator), allocator);

      const auto& light = registry.get<ecs::Light>(entity);
      serializeLight(light, componentObject, allocator);

      componentsArray.PushBack(componentObject, allocator);

      if (registry.all_of<ecs::DirectionalLight>(entity)) {
        rapidjson::Value dirLightObject(rapidjson::kObjectType);
        dirLightObject.AddMember("type", rapidjson::Value("directionalLight", allocator), allocator);

        const auto& dirLight = registry.get<ecs::DirectionalLight>(entity);
        serializeDirectionalLight(dirLight, dirLightObject, allocator);

        componentsArray.PushBack(dirLightObject, allocator);
      }

      else if (registry.all_of<ecs::PointLight>(entity)) {
        rapidjson::Value pointLightObject(rapidjson::kObjectType);
        pointLightObject.AddMember("type", rapidjson::Value("pointLight", allocator), allocator);

        const auto& pointLight = registry.get<ecs::PointLight>(entity);
        serializePointLight(pointLight, pointLightObject, allocator);

        componentsArray.PushBack(pointLightObject, allocator);
      }

      else if (registry.all_of<ecs::SpotLight>(entity)) {
        rapidjson::Value spotLightObject(rapidjson::kObjectType);
        spotLightObject.AddMember("type", rapidjson::Value("spotLight", allocator), allocator);

        const auto& spotLight = registry.get<ecs::SpotLight>(entity);
        serializeSpotLight(spotLight, spotLightObject, allocator);

        componentsArray.PushBack(spotLightObject, allocator);
      }
    }

    entityObject.AddMember("components", componentsArray, allocator);

    entitiesArray.PushBack(entityObject, allocator);
  }

  document.AddMember("entities", entitiesArray, allocator);
}

void SceneSaver::serializeTransform(const ecs::Transform&             transform,
                                    rapidjson::Value&                 componentValue,
                                    rapidjson::MemoryPoolAllocator<>& allocator) {
  rapidjson::Value positionArray(rapidjson::kArrayType);
  positionArray.PushBack(transform.translation.x(), allocator);
  positionArray.PushBack(transform.translation.y(), allocator);
  positionArray.PushBack(transform.translation.z(), allocator);
  componentValue.AddMember("position", positionArray, allocator);

  auto normalizedRotation = math::normalizeRotation(transform.rotation);

  rapidjson::Value rotationArray(rapidjson::kArrayType);
  rotationArray.PushBack(normalizedRotation.x(), allocator);
  rotationArray.PushBack(normalizedRotation.y(), allocator);
  rotationArray.PushBack(normalizedRotation.z(), allocator);
  componentValue.AddMember("rotation", rotationArray, allocator);

  rapidjson::Value scaleArray(rapidjson::kArrayType);
  scaleArray.PushBack(transform.scale.x(), allocator);
  scaleArray.PushBack(transform.scale.y(), allocator);
  scaleArray.PushBack(transform.scale.z(), allocator);
  componentValue.AddMember("scale", scaleArray, allocator);
}

void SceneSaver::serializeCamera(const ecs::Camera&                camera,
                                 rapidjson::Value&                 componentValue,
                                 rapidjson::MemoryPoolAllocator<>& allocator) {
  if (camera.type == ecs::CameraType::Perspective) {
    componentValue.AddMember("cameraType", rapidjson::Value("perspective", allocator), allocator);
  } else {
    componentValue.AddMember("cameraType", rapidjson::Value("orthographic", allocator), allocator);
  }

  componentValue.AddMember("fov", camera.fov, allocator);
  componentValue.AddMember("near", camera.nearClip, allocator);
  componentValue.AddMember("far", camera.farClip, allocator);
  componentValue.AddMember("width", camera.width, allocator);
  componentValue.AddMember("height", camera.height, allocator);
}

void SceneSaver::serializeLight(const ecs::Light&                 light,
                                rapidjson::Value&                 componentValue,
                                rapidjson::MemoryPoolAllocator<>& allocator) {
  rapidjson::Value colorArray(rapidjson::kArrayType);
  colorArray.PushBack(light.color.x(), allocator);
  colorArray.PushBack(light.color.y(), allocator);
  colorArray.PushBack(light.color.z(), allocator);
  componentValue.AddMember("color", colorArray, allocator);

  componentValue.AddMember("intensity", light.intensity, allocator);
}

void SceneSaver::serializeDirectionalLight(const ecs::DirectionalLight&      dirLight,
                                           rapidjson::Value&                 componentValue,
                                           rapidjson::MemoryPoolAllocator<>& allocator) {
  rapidjson::Value directionArray(rapidjson::kArrayType);
  directionArray.PushBack(dirLight.direction.x(), allocator);
  directionArray.PushBack(dirLight.direction.y(), allocator);
  directionArray.PushBack(dirLight.direction.z(), allocator);
  componentValue.AddMember("direction", directionArray, allocator);
}

void SceneSaver::serializePointLight(const ecs::PointLight&            pointLight,
                                     rapidjson::Value&                 componentValue,
                                     rapidjson::MemoryPoolAllocator<>& allocator) {
  componentValue.AddMember("range", pointLight.range, allocator);
}

void SceneSaver::serializeSpotLight(const ecs::SpotLight&             spotLight,
                                    rapidjson::Value&                 componentValue,
                                    rapidjson::MemoryPoolAllocator<>& allocator) {
  componentValue.AddMember("range", spotLight.range, allocator);

  componentValue.AddMember("innerConeAngle", spotLight.innerConeAngle, allocator);
  componentValue.AddMember("outerConeAngle", spotLight.outerConeAngle, allocator);
}

void SceneSaver::serializeModel(const ecs::RenderModel*           model,
                                rapidjson::Value&                 componentValue,
                                rapidjson::MemoryPoolAllocator<>& allocator) {
  if (model && !model->filePath.empty()) {
    componentValue.AddMember("path", rapidjson::Value(model->filePath.string().c_str(), allocator), allocator);
  }
}

}  // namespace arise