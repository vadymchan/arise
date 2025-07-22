#ifndef ARISE_DESCRIPTOR_DX12_H
#define ARISE_DESCRIPTOR_DX12_H

#include "gfx/rhi/interface/descriptor.h"
#include "platform/windows/windows_platform_setup.h"

#ifdef ARISE_RHI_DX12

#include <mutex>
#include <vector>

namespace arise {
namespace gfx {
namespace rhi {

class DeviceDx12;
class BufferDx12;
class TextureDx12;
class SamplerDx12;

/**
 * In DirectX 12, this corresponds to a portion of the root signature.
 * We map Vulkan's descriptor set concept to a segment of descriptors in a DX12 descriptor table.
 * So basically DescriptorSetLayout is a single descriptor table in root parameter.
 */
class DescriptorSetLayoutDx12 : public DescriptorSetLayout {
  public:
  DescriptorSetLayoutDx12(const DescriptorSetLayoutDesc& desc, DeviceDx12* device);
  ~DescriptorSetLayoutDx12() = default;

  DescriptorSetLayoutDx12(const DescriptorSetLayoutDx12&)            = delete;
  DescriptorSetLayoutDx12& operator=(const DescriptorSetLayoutDx12&) = delete;

  // DX12-specific methods
  const std::unordered_map<D3D12_DESCRIPTOR_RANGE_TYPE, std::vector<DescriptorSetLayoutBindingDesc>>&
      getBindingsByType() const {
    return m_bindingsByType;
  }

  // true if Sampler, false if CBV/SRV/UAV
  bool isSamplerLayout() const { return m_isSamplerLayout; }

  const std::vector<D3D12_DESCRIPTOR_RANGE>& getDescriptorRanges() const { return m_descriptorRanges; }

  uint32_t getTotalDescriptors() const { return m_totalDescriptors_; }

  private:
  DeviceDx12* m_device_{};

  std::unordered_map<D3D12_DESCRIPTOR_RANGE_TYPE, std::vector<DescriptorSetLayoutBindingDesc>> m_bindingsByType;

  std::vector<D3D12_DESCRIPTOR_RANGE> m_descriptorRanges;

  bool     m_isSamplerLayout   = false;
  uint32_t m_totalDescriptors_ = 0;
};

/**
 * In DirectX 12, this corresponds to a region of a descriptor heap
 * that contains the actual descriptors (CBV/SRV/UAV and samplers).
 */
class DescriptorSetDx12 : public DescriptorSet {
  public:
  DescriptorSetDx12(DeviceDx12* device, const DescriptorSetLayoutDx12* layout);
  ~DescriptorSetDx12() override;

  DescriptorSetDx12(const DescriptorSetDx12&)            = delete;
  DescriptorSetDx12& operator=(const DescriptorSetDx12&) = delete;

  // Resource binding methods
  void setUniformBuffer(uint32_t binding, Buffer* buffer, uint64_t offset = 0, uint64_t range = 0) override;
  void setTextureSampler(uint32_t binding, Texture* texture, Sampler* sampler) override;
  void setTexture(uint32_t binding, Texture* texture, ResourceLayout layout = ResourceLayout::ShaderReadOnly) override;
  void setSampler(uint32_t binding, Sampler* sampler) override;
  void setStorageBuffer(uint32_t binding, Buffer* buffer, uint64_t offset = 0, uint64_t range = 0) override;

  const DescriptorSetLayout* getLayout() const { return m_layout_; }

  // DX12-specific methods

  [[nodiscard]] D3D12_GPU_DESCRIPTOR_HANDLE getGpuSrvUavCbvHandle(uint32_t frame) const;

  [[nodiscard]] D3D12_GPU_DESCRIPTOR_HANDLE getGpuSamplerHandle(uint32_t frame) const;

  private:
  uint32_t findBindingOffset_(D3D12_DESCRIPTOR_RANGE_TYPE rangeType, uint32_t binding) const;

  DeviceDx12*                    m_device_ = nullptr;
  const DescriptorSetLayoutDx12* m_layout_ = nullptr;

  // Resource references
  std::vector<uint32_t> m_srvUavCbvIndices_{};
  std::vector<uint32_t> m_samplerIndices_{};
};

/**
 * This class manages a descriptor heap in DirectX 12. It handles allocation and deallocation of descriptors and
 * provides CPU and GPU handles for the descriptors.
 */
class DescriptorHeapDx12 {
  public:
  DescriptorHeapDx12() = default;
  ~DescriptorHeapDx12();

  bool initialize(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t count, bool shaderVisible);

  void release();

  ID3D12DescriptorHeap* getHeap() const { return m_heap.Get(); }

  D3D12_CPU_DESCRIPTOR_HANDLE getCpuHandle(uint32_t index) const;

  D3D12_GPU_DESCRIPTOR_HANDLE getGpuHandle(uint32_t index) const;

  uint32_t allocate(uint32_t count = 1);

  void free(uint32_t index);

  void freeBlock(uint32_t baseIndex, uint32_t count);

  void copyDescriptors(const DescriptorHeapDx12* srcHeap, uint32_t srcIndex, uint32_t dstIndex, uint32_t count);

  uint32_t getDescriptorSize() const { return m_descriptorSize; }

  uint32_t getCapacity() const { return m_capacity; }

  uint32_t getAllocatedCount() const { return m_allocated; }

  bool isShaderVisible() const { return m_shaderVisible; }

  D3D12_DESCRIPTOR_HEAP_TYPE getType() const { return m_type; }

  private:
  ComPtr<ID3D12DescriptorHeap> m_heap;
  std::vector<bool>            m_freeList;
  D3D12_DESCRIPTOR_HEAP_TYPE   m_type           = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
  ID3D12Device*                m_device         = nullptr;
  uint32_t                     m_descriptorSize = 0;
  uint32_t                     m_capacity       = 0;
  uint32_t                     m_allocated      = 0;
  bool                         m_shaderVisible  = false;
  mutable std::mutex           m_heapMutex;
};

/**
 * Manages frame-specific resources in DirectX 12, such as per-frame descriptor heaps.
 */
// TODO: rename this class
class FrameResourcesManager {
  public:
  FrameResourcesManager() = default;
  ~FrameResourcesManager();

  bool initialize(DeviceDx12* device, uint32_t frameCount);
  void release();

  DescriptorHeapDx12* getCurrentCbvSrvUavHeap();
  DescriptorHeapDx12* getCurrentSamplerHeap();

  DescriptorHeapDx12* getCbvSrvUavHeap(uint32_t frameIndex);

  DescriptorHeapDx12* getSamplerHeap(uint32_t frameIndex);

  uint32_t getCurrentFrameIndex() const { return m_currentFrame; }

  uint32_t getFrameCount() const { return m_frameCount; }

  void nextFrame();

  private:
  DeviceDx12* m_device       = nullptr;
  uint32_t    m_frameCount   = 0;
  uint32_t    m_currentFrame = 0;
  std::mutex  m_frameResourcesMutex;

  std::vector<std::unique_ptr<DescriptorHeapDx12>> m_cbvSrvUavHeaps;
  std::vector<std::unique_ptr<DescriptorHeapDx12>> m_samplerHeaps;
};

}  // namespace rhi
}  // namespace gfx
}  // namespace arise

#endif  // ARISE_RHI_DX12

#endif  // ARISE_DESCRIPTOR_DX12_H