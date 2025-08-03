#ifndef ARISE_SHADER_DX12_H
#define ARISE_SHADER_DX12_H

#include "gfx/rhi/interface/shader.h"
#include "platform/windows/windows_platform_setup.h"

#ifdef ARISE_USE_DX12

#include<mutex>

namespace arise {
namespace gfx {
namespace rhi {

class DeviceDx12;

/**
 * Stores compiled shader bytecode (DXIL) that can be used in pipeline state objects.
 */
class ShaderDx12 : public Shader {
  public:
  ShaderDx12(const ShaderDesc& desc, DeviceDx12* device);
  ~ShaderDx12() = default;

  ShaderDx12(const ShaderDx12&)            = delete;
  ShaderDx12& operator=(const ShaderDx12&) = delete;

    void initialize(const std::vector<uint8_t>& code) override;
  void release() override;

  // DX12-specific methods
  const D3D12_SHADER_BYTECODE& getBytecode() const { return m_bytecode_; }

  ID3DBlob* getShaderBlob() const { return m_shaderBlob_.Get(); }

  private:
  DeviceDx12*           m_device_;
  D3D12_SHADER_BYTECODE m_bytecode_;
  ComPtr<ID3DBlob>      m_shaderBlob_;  // Holds the shader bytecode memory
  std::mutex            m_mutex_;
};

}  // namespace rhi
}  // namespace gfx
}  // namespace arise

#endif  // ARISE_USE_DX12

#endif  // ARISE_SHADER_DX12_H