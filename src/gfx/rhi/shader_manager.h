#ifndef ARISE_SHADER_MANAGER_H
#define ARISE_SHADER_MANAGER_H

#include "gfx/rhi/backends/dx12/dxc_util.h"
#include "gfx/rhi/common/rhi_enums.h"
#include "gfx/rhi/common/rhi_types.h"
#include "gfx/rhi/shader_reflection/shader_reflection_types.h"
#include "gfx/rhi/interface/device.h"
#include "gfx/rhi/interface/pipeline.h"
#include "gfx/rhi/interface/shader.h"
#include "utils/hot_reload/hot_reload_manager.h"
#include "utils/logger/global_logger.h"
#include "utils/service/service_locator.h"

#include <algorithm>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace arise {
namespace gfx {
namespace rhi {

/**
 * ShaderManager class for loading, compiling, and managing shaders
 *
 * Features:
 * - Automatic shader compilation from HLSL source
 * - Caching to prevent redundant loading
 * - Hot reloading of changed shaders during development
 * - Automatic shader stage detection from file extension
 */
class ShaderManager {
  public:
  /**
   * @param maxFramesDelay Number of frames to delay pipeline rebuilds after shader changes.
   *        This parameter is crucial for GPU synchronization - it ensures that all
   *        command buffers using the old pipeline complete execution before the pipeline
   *        is rebuilt. Should match or exceed the swapchain's buffer count.
   */
  ShaderManager(Device* device, uint32_t maxFramesDelay, bool enableHotReload = true)
      : m_device_(device)
      , m_enableHotReload_(enableHotReload)
      , m_maxFramesDelay_(maxFramesDelay) {}

  ~ShaderManager() { release(); }

  /**
   * Get a shader from path
   *
   * Loads and compiles the shader if not already loaded, otherwise
   * returns the cached shader.
   */
  Shader* getShader(const std::filesystem::path& path, const std::string& entryPoint = "main") {
    std::lock_guard<std::mutex> lock(m_mutex_);

    auto it = m_loadedShaders_.find(path);
    if (it != m_loadedShaders_.end()) {
      return it->second.get();
    }

    return createShader(path, entryPoint);
  }

  void reloadShader(const std::filesystem::path& path) {
    std::lock_guard<std::mutex> lock(m_mutex_);

    // Note: we consider the path to be relative to the current working directory
    auto rel = std::filesystem::relative(path, std::filesystem::current_path());
    // normalize the path
    rel = std::filesystem::path(rel.generic_string());

    auto it = m_loadedShaders_.find(rel);
    if (it == m_loadedShaders_.end()) {
      GlobalLogger::Log(LogLevel::Warning, "Cannot reload shader, not loaded: " + path.string());
      return;
    }

    Shader* shader = it->second.get();

    auto backend = (m_device_->getApiType() == RenderingApi::Vulkan) ? ShaderBackend::SPIRV : ShaderBackend::DXIL;
    std::wstring wEntryPoint(shader->getEntryPoint().begin(), shader->getEntryPoint().end());
    auto         compiledShader = DxcUtil::s_get().compileHlslFile(path, shader->getStage(), wEntryPoint, backend);

    if (!compiledShader) {
      GlobalLogger::Log(LogLevel::Error, "Shader compilation failed: " + path.string());
      return;
    }

    auto                 data = static_cast<const uint8_t*>(compiledShader->GetBufferPointer());
    size_t               size = compiledShader->GetBufferSize();
    std::vector<uint8_t> newCode(data, data + size);

    shader->reinitialize(newCode);

    auto pipelineIt = m_shaderPipelines_.find(rel);
    if (pipelineIt != m_shaderPipelines_.end()) {
      for (Pipeline* pipeline : pipelineIt->second) {
        pipeline->scheduleUpdate(m_maxFramesDelay_);
      }
    }

    GlobalLogger::Log(LogLevel::Info, "Reloaded shader: " + path.string());
  }

  void release() {
    std::lock_guard<std::mutex> lock(m_mutex_);

    m_loadedShaders_.clear();
    m_watchedDirs_.clear();
  }

  // Links a pipeline to a shader file for hot-reload tracking
  void registerPipelineForShader(Pipeline* pipeline, const std::filesystem::path& shaderPath) {
    std::lock_guard<std::mutex> lock(m_mutex_);
    m_shaderPipelines_[shaderPath].insert(pipeline);
  }

  void unregisterPipelineForShader(Pipeline* pipeline, const std::filesystem::path& shaderPath) {
    std::lock_guard<std::mutex> lock(m_mutex_);
    auto                        it = m_shaderPipelines_.find(shaderPath);
    if (it != m_shaderPipelines_.end()) {
      it->second.erase(pipeline);
    }
  }

  private:
  Shader* createShader(const std::filesystem::path& path, const std::string& entryPoint) {
    auto shader = createShaderObject(path, entryPoint);
    if (!shader) {
      GlobalLogger::Log(LogLevel::Error, "Failed to create shader: " + path.string());
      return nullptr;
    }

    Shader* rawPtr = shader.get();

    m_loadedShaders_[path] = std::move(shader);

    if (m_enableHotReload_ && ServiceLocator::s_get<HotReloadManager>()) {
      watchDirectoryForChanges(path);
    }

    return rawPtr;
  }

  auto createShaderObject(const std::filesystem::path& path, const std::string& entryPoint) -> std::unique_ptr<Shader> {
    ShaderStageFlag stage = deduceStageFromPath(path);

    auto backend = (m_device_->getApiType() == RenderingApi::Vulkan) ? ShaderBackend::SPIRV : ShaderBackend::DXIL;

    // string -> wstring
    std::wstring wEntryPoint(entryPoint.begin(), entryPoint.end());

    auto compiledShader = DxcUtil::s_get().compileHlslFile(path, stage, wEntryPoint, backend);

    if (!compiledShader) {
      GlobalLogger::Log(LogLevel::Error, "Shader compilation failed: " + path.string());
      return nullptr;
    }

    // shader reflection
    ShaderMeta meta = DxcUtil::s_get().reflectShader(compiledShader, stage, backend);

    ShaderDesc desc;
    desc.stage      = stage;
    desc.entryPoint = entryPoint;

    // Copy shader bytecode
    auto   data = static_cast<const uint8_t*>(compiledShader->GetBufferPointer());
    size_t size = compiledShader->GetBufferSize();
    desc.code.assign(data, data + size);

    auto shader = m_device_->createShader(desc);
    if (shader) {
      shader->setMeta(meta);
    }

    return shader;
  }

  ShaderStageFlag deduceStageFromPath(const std::filesystem::path& path) {
    // Get first extension from filename (e.g., ".vs.hlsl" -> ".vs")
    std::filesystem::path stem     = path.stem();
    std::string           stageExt = stem.extension().string();

    // Convert to lowercase
    std::transform(stageExt.begin(), stageExt.end(), stageExt.begin(), ::tolower);

    if (stageExt == ".vs") {
      return ShaderStageFlag::Vertex;
    } else if (stageExt == ".ps" || stageExt == ".fs") {
      return ShaderStageFlag::Fragment;
    } else if (stageExt == ".gs") {
      return ShaderStageFlag::Geometry;
    } else if (stageExt == ".cs") {
      return ShaderStageFlag::Compute;
    } else if (stageExt == ".hs") {
      return ShaderStageFlag::TessellationControl;
    } else if (stageExt == ".ds") {
      return ShaderStageFlag::TessellationEvaluation;
    } else if (stageExt == ".rgen") {
      return ShaderStageFlag::RaytracingRaygen;
    } else if (stageExt == ".rmiss") {
      return ShaderStageFlag::RaytracingMiss;
    } else if (stageExt == ".rchit") {
      return ShaderStageFlag::RaytracingClosesthit;
    } else if (stageExt == ".rahit") {
      return ShaderStageFlag::RaytracingAnyhit;
    }

    // Default to All if unable to determine
    GlobalLogger::Log(LogLevel::Warning,
                      "Could not determine shader stage from extension: " + stageExt + ". Using All as default.");
    return ShaderStageFlag::All;
  }

  /**
   * Set up directory watching for hot reload
   */
  void watchDirectoryForChanges(const std::filesystem::path& filePath) {
    auto dir = filePath.parent_path();
    if (dir.empty()) {
      dir = ".";
    }

    auto alreadyWatched = m_watchedDirs_.contains(dir);
    if (alreadyWatched) {
      return;
    }

    m_watchedDirs_.insert(dir);

    auto hotReloadManager = ServiceLocator::s_get<HotReloadManager>();
    if (hotReloadManager) {
      hotReloadManager->watchFileModifications(dir.string(), [this](const wtr::event& e) {
        auto changedPath = std::filesystem::path(e.path_name);
        reloadShader(changedPath);
      });
    }
  }

  private:
  Device*                                                                  m_device_;
  bool                                                                     m_enableHotReload_;
  std::mutex                                                               m_mutex_;
  std::unordered_map<std::filesystem::path, std::unique_ptr<Shader>>       m_loadedShaders_;
  std::unordered_set<std::filesystem::path>                                m_watchedDirs_;
  std::unordered_map<std::filesystem::path, std::unordered_set<Pipeline*>> m_shaderPipelines_;
  uint32_t                                                                 m_maxFramesDelay_;
};

}  // namespace rhi
}  // namespace gfx
}  // namespace arise

#endif  // ARISE_SHADER_MANAGER_H