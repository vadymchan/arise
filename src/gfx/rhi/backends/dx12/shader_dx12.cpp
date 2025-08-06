#include "gfx/rhi/backends/dx12/shader_dx12.h"

#include "gfx/rhi/backends/dx12/device_dx12.h"
#include "utils/logger/log.h"

#ifdef ARISE_USE_DX12

#include <d3dcompiler.h>

namespace arise {
namespace gfx {
namespace rhi {

ShaderDx12::ShaderDx12(const ShaderDesc& desc, DeviceDx12* device)
    : Shader(desc)
    , m_device_(device) {
  initialize(desc.code);
}

void ShaderDx12::initialize(const std::vector<uint8_t>& code) {
  std::lock_guard<std::mutex> lock(m_mutex_);

  m_desc_.code = code;

  // Create a blob to hold the shader bytecode
  HRESULT hr = D3DCreateBlob(code.size(), &m_shaderBlob_);
  if (FAILED(hr)) {
    LOG_ERROR("Failed to create shader blob for DirectX 12 shader");
    return;
  }

  // Copy the shader bytecode into the blob
  memcpy(m_shaderBlob_->GetBufferPointer(), code.data(), code.size());

  m_bytecode_.pShaderBytecode = m_shaderBlob_->GetBufferPointer();
  m_bytecode_.BytecodeLength  = m_shaderBlob_->GetBufferSize();
}

void ShaderDx12::release() {
  std::lock_guard<std::mutex> lock(m_mutex_);

  if (m_shaderBlob_) {
    m_shaderBlob_.Reset();
    m_bytecode_ = {};
  }
}

}  // namespace rhi
}  // namespace gfx
}  // namespace arise

#endif  // ARISE_USE_DX12