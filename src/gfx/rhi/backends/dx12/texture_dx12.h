#ifndef ARISE_TEXTURE_DX12_H
#define ARISE_TEXTURE_DX12_H

#include "gfx/rhi/backends/dx12/rhi_enums_dx12.h"
#include "gfx/rhi/interface/texture.h"
#include "platform/windows/windows_platform_setup.h"

#include <D3D12MemAlloc.h>

#ifdef ARISE_USE_DX12

namespace arise {
namespace gfx {
namespace rhi {

class DeviceDx12;

class TextureDx12 : public Texture {
  public:
  TextureDx12(const TextureDesc& desc, DeviceDx12* device);

  /**
   * Create a texture from an existing resource (for swap chain images)
   *
   * When created with this constructor, the texture does not own the resource
   * or the descriptor handles that are passed in. This means it will not attempt
   * to free them when destroyed. The owner (SwapChain) remains responsible
   * for freeing these resources.
   */
  TextureDx12(DeviceDx12*                 device,
              const TextureDesc&          desc,
              ID3D12Resource*             existingResource,
              D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = {},
              D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = {});

  ~TextureDx12() override;

  TextureDx12(const TextureDx12&)            = delete;
  TextureDx12& operator=(const TextureDx12&) = delete;

  // DX12-specific methods
  ID3D12Resource* getResource() const { return m_resource_.Get(); }

  DXGI_FORMAT getDxgiFormat() const { return m_dxgiFormat_; }

  D3D12_RESOURCE_STATES getResourceState() const { return g_getResourceLayoutDx12(m_currentLayout_); }

  D3D12_CPU_DESCRIPTOR_HANDLE getRtvHandle(uint32_t mipLevel = 0, uint32_t arraySlice = 0) const;
  D3D12_CPU_DESCRIPTOR_HANDLE getDsvHandle(uint32_t mipLevel = 0, uint32_t arraySlice = 0) const;

  D3D12_CPU_DESCRIPTOR_HANDLE getSrvHandle() const { return m_srvHandle_; }

  uint32_t getSrvDescriptorIndex() const { return m_srvDescriptorIndex_; }

  D3D12_CPU_DESCRIPTOR_HANDLE getUavHandle(uint32_t mipLevel = 0) const;

  D3D12MA::Allocation* getAllocation() const { return m_allocation_.Get(); }

  // Updates texture data (for staging uploads)
  void update(const void* data, size_t dataSize, uint32_t mipLevel = 0, uint32_t arrayLayer = 0);

  private:
  friend class CommandBufferDx12;
  // Only CommandBufferDx12 should update state through barriers
  void updateCurrentState_(ResourceLayout state);

  bool createResource_();
  bool createViews_();

  DeviceDx12* m_device_;

  ComPtr<ID3D12Resource>      m_resource_;
  ComPtr<D3D12MA::Allocation> m_allocation_;

  DXGI_FORMAT m_dxgiFormat_ = DXGI_FORMAT_UNKNOWN;

  D3D12_CPU_DESCRIPTOR_HANDLE              m_rtvHandle_ = {};
  D3D12_CPU_DESCRIPTOR_HANDLE              m_dsvHandle_ = {};
  D3D12_CPU_DESCRIPTOR_HANDLE              m_srvHandle_ = {};
  std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> m_uavHandles_;

  bool              m_ownsRtvDescriptor = true;
  bool              m_ownsDsvDescriptor = true;
  bool              m_ownsSrvDescriptor = true;
  std::vector<bool> m_ownsUavDescriptors;

  uint32_t              m_rtvDescriptorIndex_ = UINT32_MAX;
  uint32_t              m_dsvDescriptorIndex_ = UINT32_MAX;
  uint32_t              m_srvDescriptorIndex_ = UINT32_MAX;
  std::vector<uint32_t> m_uavDescriptorIndices_;
};

}  // namespace rhi
}  // namespace gfx
}  // namespace arise

#endif  // ARISE_USE_DX12
#endif  // ARISE_TEXTURE_DX12_H