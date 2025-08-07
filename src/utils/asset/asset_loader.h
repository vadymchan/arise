#ifndef ARISE_ASSET_LOADER_H
#define ARISE_ASSET_LOADER_H

#include "profiler/profiler.h"
#include "utils/logger/log.h"
#include "utils/model/model_manager.h"
#include "utils/model/render_model_manager.h"
#include "utils/service/service_locator.h"
#include "utils/texture/texture_manager.h"

#include <atomic>
#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>
#include <variant>

namespace arise {

/**
 * Enum representing different types of loadable assets
 */
enum class AssetType {
  Model,
  Texture,
};

/**
 * Class handling asynchronous loading of assets
 */
class AssetLoader {
  public:
  using LoadCallback = std::function<void(bool success)>;

  AssetLoader()
      : m_running(false)
      , m_workerThread(nullptr) {
    LOG_INFO("AssetLoader created");
  }

  ~AssetLoader() { shutdown(); }

  void initialize() {
    if (m_running) {
      return;
    }

    m_running      = true;
    m_workerThread = std::make_unique<std::thread>(&AssetLoader::workerFunction, this);

    LOG_INFO("AssetLoader initialized");
  }

  void shutdown() {
    if (!m_running) {
      return;
    }

    {
      std::lock_guard<std::mutex> lock(m_queueMutex);
      m_running = false;
      m_condVar.notify_one();
    }

    if (m_workerThread && m_workerThread->joinable()) {
      m_workerThread->join();
      m_workerThread.reset();
    }

    LOG_INFO("AssetLoader shutdown");
  }

  void loadModel(const std::filesystem::path& filepath, LoadCallback callback = nullptr) {
    CPU_ZONE_NC("Load Model", color::BROWN);
    if (!m_running) {
      LOG_WARN("AssetLoader not running, initializing now");
      initialize();
    }

    std::string assetKey = createAssetKey(AssetType::Model, filepath.string());

    {
      std::lock_guard<std::mutex> lock(m_queueMutex);

      if (m_pendingAssets.find(assetKey) != m_pendingAssets.end()) {
        LOG_INFO("Asset already queued for loading: {}", filepath.string());
        if (callback) {
          m_pendingCallbacks[assetKey].push_back(callback);
        }
        return;
      }

      auto modelManager = ServiceLocator::s_get<RenderModelManager>();
      if (modelManager) {
        if (modelManager->hasRenderModel(filepath)) {
          LOG_INFO("Asset already loaded: {}", filepath.string());
          if (callback) {
            callback(true);
          }
          return;
        }
      }

      AssetRequest request;
      request.type = AssetType::Model;
      request.path = filepath;

      m_pendingAssets[assetKey] = true;

      if (callback) {
        m_pendingCallbacks[assetKey].push_back(callback);
      }

      m_requestQueue.push(request);
      m_condVar.notify_one();

      LOG_INFO("Queued asset for loading: {}", filepath.string());
    }
  }

  void loadTexture(const std::filesystem::path& filepath, LoadCallback callback = nullptr) {
    if (!m_running) {
      LOG_WARN("AssetLoader not running, initializing now");
      initialize();
    }

    std::string assetKey = createAssetKey(AssetType::Texture, filepath.string());

    {
      std::lock_guard<std::mutex> lock(m_queueMutex);

      if (m_pendingAssets.find(assetKey) != m_pendingAssets.end()) {
        LOG_INFO("Asset already queued for loading: {}", filepath.string());
        if (callback) {
          m_pendingCallbacks[assetKey].push_back(callback);
        }
        return;
      }

      auto textureManager = ServiceLocator::s_get<TextureManager>();
      if (textureManager) {
        if (textureManager->hasTexture(filepath.filename().string())) {
          LOG_INFO("Asset already loaded: {}", filepath.string());
          if (callback) {
            callback(true);
          }
          return;
        }
      }

      AssetRequest request;
      request.type = AssetType::Texture;
      request.path = filepath;

      m_pendingAssets[assetKey] = true;

      if (callback) {
        m_pendingCallbacks[assetKey].push_back(callback);
      }

      m_requestQueue.push(request);
      m_condVar.notify_one();

      LOG_INFO("Queued asset for loading: {}", filepath.string());
    }
  }

  bool cancelRequest(const std::filesystem::path& filepath, AssetType type) {
    std::string assetKey = createAssetKey(type, filepath.string());

    std::lock_guard<std::mutex> lock(m_queueMutex);

    auto it = m_pendingAssets.find(assetKey);
    if (it != m_pendingAssets.end()) {
      m_pendingAssets.erase(it);
      m_pendingCallbacks.erase(assetKey);

      LOG_INFO("Cancelled pending asset load: {}", filepath.string());
      return true;
    }

    return false;
  }

  bool isAssetPending(const std::filesystem::path& filepath, AssetType type) const {
    std::string assetKey = createAssetKey(type, filepath.string());

    std::lock_guard<std::mutex> lock(m_queueMutex);
    return m_pendingAssets.find(assetKey) != m_pendingAssets.end();
  }

  size_t getPendingAssetCount() const {
    std::lock_guard<std::mutex> lock(m_queueMutex);
    return m_requestQueue.size();
  }

  private:
  struct AssetRequest {
    AssetType             type;
    std::filesystem::path path;
  };

  void workerFunction() {
    while (m_running) {
      AssetRequest request;
      bool         hasRequest = false;

      {
        std::unique_lock<std::mutex> lock(m_queueMutex);
        m_condVar.wait(lock, [this] { return !m_running || !m_requestQueue.empty(); });

        if (!m_running) {
          break;
        }

        if (!m_requestQueue.empty()) {
          request = m_requestQueue.front();
          m_requestQueue.pop();
          hasRequest = true;
        }
      }

      if (hasRequest) {
        processRequest(request);
      }
    }
  }

  void processRequest(const AssetRequest& request) {
    std::string assetKey = createAssetKey(request.type, request.path.string());

    {
      std::lock_guard<std::mutex> lock(m_queueMutex);
      if (m_pendingAssets.find(assetKey) == m_pendingAssets.end()) {
        return;
      }
    }

    bool success = false;

    switch (request.type) {
      case AssetType::Model:
        success = loadModelInternal_(request.path);
        break;
      case AssetType::Texture:
        success = loadTextureInternal_(request.path);
        break;
      default:
        LOG_ERROR("Unknown asset type for: {}", request.path.string());
        break;
    }

    std::vector<LoadCallback> callbacks;

    {
      std::lock_guard<std::mutex> lock(m_queueMutex);

      auto callbackIt = m_pendingCallbacks.find(assetKey);
      if (callbackIt != m_pendingCallbacks.end()) {
        callbacks = std::move(callbackIt->second);
        m_pendingCallbacks.erase(callbackIt);
      }

      m_pendingAssets.erase(assetKey);
    }

    for (const auto& callback : callbacks) {
      callback(success);
    }
  }

  bool loadModelInternal_(const std::filesystem::path& filepath) {
    LOG_INFO("Loading model: {}", filepath.string());

    if (auto renderModelManager = ServiceLocator::s_get<RenderModelManager>()) {
      ecs::RenderModel* gpuModel = renderModelManager->getRenderModel(filepath);
      if (gpuModel) {
        LOG_INFO("Successfully loaded GPU model: {}", filepath.string());
        return true;
      }
      LOG_WARN("GPU model load failed – will attempt CPU-only load for: {}", filepath.string());
    }

    if (auto cpuModelManager = ServiceLocator::s_get<ModelManager>()) {
      ecs::Model* cpuModel = cpuModelManager->getModel(filepath);
      bool        success  = (cpuModel != nullptr);

      if (success) {
        LOG_INFO("Successfully loaded CPU model (no GPU resources yet): {}", filepath.string());
      } else {
        LOG_ERROR("Failed to load CPU model: {}", filepath.string());
      }

      return success;
    }

    LOG_ERROR("Neither GPU nor CPU model managers are available for: {}", filepath.string());
    return false;
  }

  bool loadTextureInternal_(const std::filesystem::path& filepath) {
    LOG_INFO("Loading texture: {}", filepath.string());

    auto textureManager = ServiceLocator::s_get<TextureManager>();
    if (!textureManager) {
      LOG_ERROR("Cannot load texture, TextureManager not available");
      return false;
    }

    gfx::rhi::Texture* texture = textureManager->createTextureFromFile(filepath);

    bool success = (texture != nullptr);

    if (success) {
      LOG_INFO("Successfully loaded texture: {}", filepath.string());
    } else {
      LOG_ERROR("Failed to load texture: {}", filepath.string());
    }

    return success;
  }

  std::string createAssetKey(AssetType type, const std::string& path) const {
    return std::to_string(static_cast<int>(type)) + ":" + path;
  }

  std::atomic<bool>            m_running;
  std::unique_ptr<std::thread> m_workerThread;

  mutable std::mutex       m_queueMutex;
  std::condition_variable  m_condVar;
  std::queue<AssetRequest> m_requestQueue;

  std::unordered_map<std::string, bool>                      m_pendingAssets;
  std::unordered_map<std::string, std::vector<LoadCallback>> m_pendingCallbacks;
};

}  // namespace arise

#endif  // ARISE_ASSET_LOADER_H