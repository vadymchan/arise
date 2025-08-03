// scene/scene_saver.h
#ifndef ARISE_SCENE_SAVER_H
#define ARISE_SCENE_SAVER_H

#include "config/config.h"
#include "scene/scene.h"

#include <filesystem>
#include <string>

namespace arise {

namespace ecs {
struct Transform;
struct Camera;
struct Light;
struct DirectionalLight;
struct PointLight;
struct SpotLight;
struct RenderModel;
}

class SceneSaver {
  public:
  
  static bool saveScene(Scene* scene, const std::string& sceneName, const std::filesystem::path& filePath);

  private:
  // Serializes registry entities/components to JSON
  static void serializeRegistry(const Registry&                   registry,
                                rapidjson::Document&              document,
                                rapidjson::MemoryPoolAllocator<>& allocator);

  // Specialized component serialization methods
  static void serializeTransform(const ecs::Transform&                  transform,
                                 rapidjson::Value&                 componentValue,
                                 rapidjson::MemoryPoolAllocator<>& allocator);
  static void serializeCamera(const ecs::Camera&                     camera,
                              rapidjson::Value&                 componentValue,
                              rapidjson::MemoryPoolAllocator<>& allocator);
  static void serializeLight(const ecs::Light&                      light,
                             rapidjson::Value&                 componentValue,
                             rapidjson::MemoryPoolAllocator<>& allocator);
  static void serializeDirectionalLight(const ecs::DirectionalLight&      dirLight,
                                        rapidjson::Value&                 componentValue,
                                        rapidjson::MemoryPoolAllocator<>& allocator);
  static void serializePointLight(const ecs::PointLight&            pointLight,
                                  rapidjson::Value&                 componentValue,
                                  rapidjson::MemoryPoolAllocator<>& allocator);
  static void serializeSpotLight(const ecs::SpotLight&                  spotLight,
                                 rapidjson::Value&                 componentValue,
                                 rapidjson::MemoryPoolAllocator<>& allocator);
  static void serializeModel(const ecs::RenderModel*           model,
                             rapidjson::Value&                 componentValue,
                             rapidjson::MemoryPoolAllocator<>& allocator);
};

}  // namespace arise

#endif  // ARISE_SCENE_SAVER_H