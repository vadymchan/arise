#ifndef ARISE_SAMPLER_DX12_H
#define ARISE_SAMPLER_DX12_H

#include "gfx/rhi/interface/sampler.h"
#include "platform/windows/windows_platform_setup.h"

#ifdef ARISE_USE_DX12

namespace arise {
namespace gfx {
namespace rhi {

class DeviceDx12;

class SamplerDx12 : public Sampler {
  public:
  SamplerDx12(const SamplerDesc& desc, DeviceDx12* device);
  ~SamplerDx12() override;

  SamplerDx12(const SamplerDx12&)            = delete;
  SamplerDx12& operator=(const SamplerDx12&) = delete;

  // DirectX 12-specific methods
  D3D12_CPU_DESCRIPTOR_HANDLE getCpuHandle() const { return m_cpuHandle_; }

  uint32_t getDescriptorIndex() const { return m_descriptorIndex_; }

  D3D12_SAMPLER_DESC getDx12Desc() const { return m_dx12Desc_; }

  bool isValid() const { return m_descriptorIndex_ != UINT32_MAX; }

  private:
  DeviceDx12*                 m_device_;
  uint32_t                    m_descriptorIndex_ = UINT32_MAX;
  D3D12_CPU_DESCRIPTOR_HANDLE m_cpuHandle_       = {};
  D3D12_SAMPLER_DESC          m_dx12Desc_        = {};
};

}  // namespace rhi
}  // namespace gfx
}  // namespace arise

#endif  // ARISE_USE_DX12
#endif  // ARISE_SAMPLER_DX12_H