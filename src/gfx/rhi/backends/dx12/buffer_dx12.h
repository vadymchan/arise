#ifndef ARISE_BUFFER_DX12_H
#define ARISE_BUFFER_DX12_H

#include "gfx/rhi/interface/buffer.h"
#include "platform/windows/windows_platform_setup.h"

#include <D3D12MemAlloc.h>

#ifdef ARISE_USE_DX12

namespace arise {
namespace gfx {
namespace rhi {

class DeviceDx12;

/**
 * This class serves as a foundation for all buffer types in DirectX 12. We separate buffers into two categories:
 * 1. DirectBufferDx12 - for vertex/index/instance buffers accessed directly via GPU virtual address
 * 2. DescriptorBufferDx12 - for constant/storage buffers accessed through descriptor tables
 */
class BufferDx12 : public Buffer {
  public:
  BufferDx12(const BufferDesc& desc, DeviceDx12* device);
  virtual ~BufferDx12();

  ID3D12Resource* getResource() const { return m_resource_.Get(); }

  D3D12_GPU_VIRTUAL_ADDRESS getGPUVirtualAddress() const {
    return m_resource_ ? m_resource_->GetGPUVirtualAddress() : 0;
  }

  D3D12_RESOURCE_STATES getCurrentState() const { return m_currentState_; }

  void updateCurrentState(D3D12_RESOURCE_STATES newState) { m_currentState_ = newState; }

  D3D12MA::Allocation* getAllocation() const { return m_allocation_.Get(); }

  protected:
  friend class DeviceDx12;
  // update_ method is called from DeviceDx12
  bool update_(const void* data, size_t size, size_t offset);

  bool map_(void** ppData, D3D12_RANGE* pReadRange);
  void unmap_(D3D12_RANGE* pWrittenRange);

  bool                  isUploadHeapBuffer_() const;
  bool                  isReadbackHeapBuffer_() const;
  D3D12_RESOURCE_STATES getInitialResourceState_() const;

  DeviceDx12*                 m_device_ = nullptr;
  ComPtr<ID3D12Resource>      m_resource_;
  ComPtr<D3D12MA::Allocation> m_allocation_;
  void*                       m_mappedData_   = nullptr;  // For persistently mapped buffers
  bool                        m_isMapped_     = false;
  D3D12_RESOURCE_STATES       m_currentState_ = D3D12_RESOURCE_STATE_COMMON;
};

/**
 * DirectBufferDx12 represents buffers that are accessed directly via GPU virtual address.
 *
 * This class is used for vertex, index, and instance buffers which don't require descriptors.
 * Instead, they are bound directly to the command list using their GPU virtual address.
 */
class DirectBufferDx12 : public BufferDx12 {
  public:
  DirectBufferDx12(const BufferDesc& desc, DeviceDx12* device);
  ~DirectBufferDx12() = default;

  bool isVertexBuffer() const;
  bool isIndexBuffer() const;
  bool isInstanceBuffer() const;

  // Vertex/Index buffer specific methods
  uint32_t getStride() const { return m_stride_; }

  private:
  uint32_t m_stride_ = 0;  // Size of a single vertex/instance element
};

/**
 * DescriptorBufferDx12 represents buffers that are accessed through descriptors in DirectX 12.
 *
 * This class is used for constant buffers (CBV), unordered access buffers (UAV),
 * and shader resource buffers (SRV) which require descriptor handles to be accessed
 * by shaders. These buffers are bound via descriptor tables in the root signature.
 */
class DescriptorBufferDx12 : public BufferDx12 {
  public:
  DescriptorBufferDx12(const BufferDesc& desc, DeviceDx12* device);
  ~DescriptorBufferDx12() override;

  bool hasCbvHandle() const { return m_cbvDescriptorIndex_ != UINT32_MAX; }

  bool hasUavHandle() const { return m_uavDescriptorIndex_ != UINT32_MAX; }

  bool hasSrvHandle() const { return m_srvDescriptorIndex_ != UINT32_MAX; }

  D3D12_CPU_DESCRIPTOR_HANDLE getCbvCpuHandle() const { return m_cbvCpuHandle_; }

  D3D12_CPU_DESCRIPTOR_HANDLE getUavCpuHandle() const { return m_uavCpuHandle_; }

  D3D12_CPU_DESCRIPTOR_HANDLE getSrvCpuHandle() const { return m_srvCpuHandle_; }

  bool isConstantBuffer() const;
  bool isUnorderedAccessBuffer() const;
  bool isShaderResourceBuffer() const;

  void createConstantBufferView();
  void createUnorderedAccessView();
  void createShaderResourceView();

  private:
  uint32_t m_cbvDescriptorIndex_ = UINT32_MAX;
  uint32_t m_uavDescriptorIndex_ = UINT32_MAX;
  uint32_t m_srvDescriptorIndex_ = UINT32_MAX;

  D3D12_CPU_DESCRIPTOR_HANDLE m_cbvCpuHandle_ = {};
  D3D12_CPU_DESCRIPTOR_HANDLE m_uavCpuHandle_ = {};
  D3D12_CPU_DESCRIPTOR_HANDLE m_srvCpuHandle_ = {};
};

}  // namespace rhi
}  // namespace gfx
}  // namespace arise

#endif  // ARISE_USE_DX12
#endif  // ARISE_BUFFER_DX12_H