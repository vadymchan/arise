#ifndef ARISE_CGLTF_COMMON_H
#define ARISE_CGLTF_COMMON_H

#ifdef ARISE_USE_CGLTF

#include <cgltf.h>

#include <filesystem>
#include <memory>
#include <unordered_map>

namespace arise {

class CgltfSceneCache {
  public:
  static std::shared_ptr<cgltf_data> getOrLoad(const std::filesystem::path& path) {
    auto absolutePath = std::filesystem::absolute(path).string();
    auto it           = s_cache.find(absolutePath);
    if (it != s_cache.end()) {
      if (auto ptr = it->second.lock()) {
        return ptr;
      }
    }

    cgltf_options opts{};
    cgltf_data*   raw = nullptr;
    if (cgltf_parse_file(&opts, absolutePath.c_str(), &raw) != cgltf_result_success
        || cgltf_load_buffers(&opts, raw, absolutePath.c_str()) != cgltf_result_success) {
      if (raw) {
        cgltf_free(raw);
      }
      return {};
    }

    auto scene            = std::shared_ptr<cgltf_data>(raw, [](cgltf_data* d) { cgltf_free(d); });
    s_cache[absolutePath] = scene;
    return scene;
  }

  private:
  static inline std::unordered_map<std::string, std::weak_ptr<cgltf_data>> s_cache;
};

}  // namespace arise

#endif  // ARISE_USE_CGLTF

#endif  // ARISE_CGLTF_COMMON_H