#ifndef ARISE_DXC_UTIL_DX_H
#define ARISE_DXC_UTIL_DX_H

// TODO: consider moving this file to another directory

#include "gfx/rhi/common/rhi_enums.h"
#include "gfx/rhi/common/rhi_types.h"
#include "gfx/rhi/shader_reflection/shader_reflection_types.h"
#include "platform/windows/windows_platform_setup.h"
#include "utils/logger/log.h"

#include <codecvt>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <locale>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#if defined(_WIN32)
#include <Windows.h>
#define DXC_COMPILER_LIBRARY L"dxcompiler.dll"
#define GET_DXC_SYMBOL       GetProcAddress
using DxcLibHandle = HMODULE;
#else
#include <dlfcn.h>
#if defined(__APPLE__)
#define DXC_COMPILER_LIBRARY "libdxcompiler.dylib"
#elif defined(__linux__)
#define DXC_COMPILER_LIBRARY "libdxcompiler.so"
#else
#error "Unsupported platform. Please define DXC_COMPILER_LIBRARY for your platform."
#endif
#define GET_DXC_SYMBOL dlsym
using DxcLibHandle = void*;
#endif

#include <dxc/dxcapi.h>

namespace arise {

enum class ShaderBackend {
  DXIL,
  SPIRV
};

struct OptionalShaderParams {
  // include directories passed to -I
  std::vector<std::wstring> includeDirs;
  // macro definitions passed to -D
  std::vector<std::wstring> preprocessorDefs;
  // additional flags
  std::vector<std::wstring> extraArgs;
};

// helper function to wrap any DXC/COM pointer in a std::shared_ptr
template <typename T>
std::shared_ptr<T> MakeDxcSharedPtr(T* rawPtr) {
  // Custom deleter calls ->Release() when refcount hits zero
  return std::shared_ptr<T>(rawPtr, [](T* p) {
    if (p) {
      p->Release();
    }
  });
}

class DxcUtil {
  public:
  static DxcUtil& s_get() {
    static DxcUtil instance;
    if (!instance.initialize()) {
      LOG_ERROR("Failed to initialize DXC library.");
    }

    return instance;
  }

  // read from disk, then compile
  std::shared_ptr<IDxcBlob> compileHlslFile(const std::filesystem::path& shaderPath,
                                            gfx::rhi::ShaderStageFlag    stage,
                                            const std::wstring&          entryPoint,
                                            ShaderBackend                backend,
                                            const OptionalShaderParams&  optionalParams = {}) {
    std::string code = readFile_(shaderPath);
    if (code.empty()) {
      LOG_ERROR("Failed to read shader file: {}", shaderPath.string());
      return nullptr;
    }
    return compileHlslCode(code, stage, entryPoint, backend, optionalParams);
  }

  // compile from a raw string
  std::shared_ptr<IDxcBlob> compileHlslCode(const std::string&          hlslCode,
                                            gfx::rhi::ShaderStageFlag   stage,
                                            const std::wstring&         entryPoint,
                                            ShaderBackend               backend,
                                            const OptionalShaderParams& optionalParams = {});

  gfx::rhi::ShaderMeta reflectShader(const std::shared_ptr<IDxcBlob>& shaderBlob,
                                     gfx::rhi::ShaderStageFlag        stage,
                                     ShaderBackend                    backend);

  private:
  DxcUtil() = default;

  ~DxcUtil() {
    if (m_libHandle) {
#if defined(_WIN32)
      FreeLibrary(m_libHandle);
#else
      dlclose(m_libHandle);
#endif
      m_libHandle = nullptr;
    }
  }

  DxcUtil(const DxcUtil&)            = delete;
  DxcUtil& operator=(const DxcUtil&) = delete;

  bool initialize();

  std::wstring getTargetProfile_(gfx::rhi::ShaderStageFlag stage);

  static std::string readFile_(const std::filesystem::path& path);

  static std::string wstring_to_utf8_(const std::wstring& wstr);

  gfx::rhi::ShaderMeta reflectDxil_(const std::shared_ptr<IDxcBlob>& shaderBlob, gfx::rhi::ShaderStageFlag stage);

  gfx::rhi::ShaderMeta reflectSpirv_(const std::shared_ptr<IDxcBlob>& shaderBlob, gfx::rhi::ShaderStageFlag stage);

  gfx::rhi::ShaderBindingType convertDxilResourceType_(D3D_SHADER_INPUT_TYPE type);

  DxcLibHandle          m_libHandle   = nullptr;
  DxcCreateInstanceProc m_dxcCreateFn = nullptr;
};

}  // namespace arise

#endif  // ARISE_DXC_UTIL_DX_H
