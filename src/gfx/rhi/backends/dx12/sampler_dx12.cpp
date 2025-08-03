#include "gfx/rhi/backends/dx12/sampler_dx12.h"

#ifdef ARISE_USE_DX12

#include "gfx/rhi/backends/dx12/descriptor_dx12.h"
#include "gfx/rhi/backends/dx12/device_dx12.h"
#include "gfx/rhi/backends/dx12/rhi_enums_dx12.h"
#include "utils/logger/global_logger.h"

namespace arise {
namespace gfx {
namespace rhi {

SamplerDx12::SamplerDx12(const SamplerDesc& desc, DeviceDx12* device)
    : Sampler(desc)
    , m_device_(device) {
  m_dx12Desc_.Filter = g_getTextureFilterDx12(desc.minFilter, desc.magFilter, desc.compareEnable);

  m_dx12Desc_.AddressU = g_getTextureAddressModeDx12(desc.addressModeU);
  m_dx12Desc_.AddressV = g_getTextureAddressModeDx12(desc.addressModeV);
  m_dx12Desc_.AddressW = g_getTextureAddressModeDx12(desc.addressModeW);

  m_dx12Desc_.MipLODBias = desc.mipLodBias;
  m_dx12Desc_.MinLOD     = desc.minLod;
  m_dx12Desc_.MaxLOD     = desc.maxLod;

  m_dx12Desc_.MaxAnisotropy = static_cast<UINT>(desc.anisotropyEnable ? desc.maxAnisotropy : 1);

  m_dx12Desc_.ComparisonFunc = desc.compareEnable ? g_getCompareOpDx12(desc.compareOp) : D3D12_COMPARISON_FUNC_NEVER;

  memcpy(m_dx12Desc_.BorderColor, desc.borderColor, sizeof(float) * 4);

  auto* samplerHeap = device->getCpuSamplerHeap();
  if (!samplerHeap) {
    GlobalLogger::Log(LogLevel::Error, "Sampler heap not available");
    return;
  }

  m_descriptorIndex_ = samplerHeap->allocate();
  if (m_descriptorIndex_ == UINT32_MAX) {
    GlobalLogger::Log(LogLevel::Error, "Failed to allocate sampler descriptor");
    return;
  }

  m_cpuHandle_ = samplerHeap->getCpuHandle(m_descriptorIndex_);

  device->getDevice()->CreateSampler(&m_dx12Desc_, m_cpuHandle_);
}

SamplerDx12::~SamplerDx12() {
  if (m_device_ && m_descriptorIndex_ != UINT32_MAX) {
    auto* samplerHeap = m_device_->getCpuSamplerHeap();
    if (samplerHeap) {
      samplerHeap->free(m_descriptorIndex_);
    }
    m_descriptorIndex_ = UINT32_MAX;
  }
}

}  // namespace rhi
}  // namespace gfx
}  // namespace arise

#endif  // ARISE_USE_DX12