#include "gfx/rhi/backends/dx12/buffer_dx12.h"

#ifdef ARISE_USE_DX12

#include "gfx/rhi/backends/dx12/device_dx12.h"
#include "utils/logger/log.h"
#include "utils/memory/align.h"

namespace arise {
namespace gfx {
namespace rhi {

//-----------------------------------------------------------------------------------
// BufferDx12 Implementation
//-----------------------------------------------------------------------------------

BufferDx12::BufferDx12(const BufferDesc& desc, DeviceDx12* device)
    : Buffer(desc)
    , m_device_(device) {
  D3D12_HEAP_TYPE heapType = D3D12_HEAP_TYPE_DEFAULT;  // GPU-only memory by default
  m_currentState_          = getInitialResourceState_();

  if (isUploadHeapBuffer_()) {
    heapType        = D3D12_HEAP_TYPE_UPLOAD;
    m_currentState_ = D3D12_RESOURCE_STATE_GENERIC_READ;
  } else if (isReadbackHeapBuffer_()) {
    heapType        = D3D12_HEAP_TYPE_READBACK;
    m_currentState_ = D3D12_RESOURCE_STATE_COPY_DEST;
  }

  D3D12_RESOURCE_DESC resourceDesc = {};
  resourceDesc.Dimension           = D3D12_RESOURCE_DIMENSION_BUFFER;
  resourceDesc.Alignment           = 0;
  resourceDesc.Width               = m_desc_.size;
  resourceDesc.Height              = 1;
  resourceDesc.DepthOrArraySize    = 1;
  resourceDesc.MipLevels           = 1;
  resourceDesc.Format              = DXGI_FORMAT_UNKNOWN;
  resourceDesc.SampleDesc.Count    = 1;
  resourceDesc.SampleDesc.Quality  = 0;
  resourceDesc.Layout              = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
  resourceDesc.Flags               = D3D12_RESOURCE_FLAG_NONE;

  if ((m_desc_.createFlags & BufferCreateFlag::Uav) != BufferCreateFlag::None) {
    resourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
  }

  D3D12MA::ALLOCATION_DESC allocationDesc = {};
  allocationDesc.HeapType                 = heapType;

  HRESULT hr = device->getAllocator()->CreateResource(
      &allocationDesc, &resourceDesc, m_currentState_, nullptr, &m_allocation_, IID_PPV_ARGS(&m_resource_));

  if (FAILED(hr)) {
    LOG_ERROR("Failed to create DirectX 12 buffer using D3D12MA");
    return;
  }

  // For upload heap buffers (CPU-accessible), map buffer memory immediately
  if (isUploadHeapBuffer_()) {
    D3D12_RANGE readRange = {0, 0};  // We do not intend to read
    hr                    = m_resource_->Map(0, &readRange, &m_mappedData_);

    if (FAILED(hr)) {
      LOG_ERROR("Failed to map buffer memory");
      m_mappedData_ = nullptr;
    } else {
      m_isMapped_ = true;
    }
  }

  if (!desc.debugName.empty() && m_resource_) {
    int size = MultiByteToWideChar(CP_UTF8, 0, desc.debugName.c_str(), -1, nullptr, 0);
    if (size > 0) {
      std::wstring wideName(size, 0);
      MultiByteToWideChar(CP_UTF8, 0, desc.debugName.c_str(), -1, &wideName[0], size);
      m_resource_->SetName(wideName.c_str());
    }
  }
}

BufferDx12::~BufferDx12() {
  if (m_isMapped_ && m_resource_) {
    D3D12_RANGE writtenRange = {0, static_cast<SIZE_T>(m_desc_.size)};
    m_resource_->Unmap(0, &writtenRange);
    m_mappedData_ = nullptr;
    m_isMapped_   = false;
  }
}

bool BufferDx12::update_(const void* data, size_t size, size_t offset) {
  if (!data || size == 0) {
    return false;
  }

  if (offset + size > m_desc_.size) {
    LOG_ERROR("Buffer update exceeds buffer size");
    return false;
  }

  // If this is an upload heap buffer, update it directly
  if (m_isMapped_ && m_mappedData_) {
    char* mappedCharPtr = static_cast<char*>(m_mappedData_);
    memcpy(mappedCharPtr + offset, data, size);
    return true;
  } else if (isUploadHeapBuffer_() && !m_isMapped_) {
    D3D12_RANGE readRange  = {0, 0};  // We do not intend to read
    void*       mappedData = nullptr;

    HRESULT hr = m_resource_->Map(0, &readRange, &mappedData);
    if (SUCCEEDED(hr)) {
      char* mappedCharPtr = static_cast<char*>(mappedData);
      memcpy(mappedCharPtr + offset, data, size);

      D3D12_RANGE writtenRange = {static_cast<SIZE_T>(offset), static_cast<SIZE_T>(offset + size)};
      m_resource_->Unmap(0, &writtenRange);
      return true;
    } else {
      LOG_ERROR("Failed to map buffer for update");
      return false;
    }
  }

  return false;
}

bool BufferDx12::map_(void** ppData, D3D12_RANGE* pReadRange) {
  if (!ppData) {
    return false;
  }

  if (m_isMapped_) {
    *ppData = m_mappedData_;
    return true;
  }

  if (!m_resource_) {
    return false;
  }

  if (!isUploadHeapBuffer_() && !isReadbackHeapBuffer_()) {
    LOG_ERROR("Cannot map a non-mappable buffer");
    return false;
  }

  HRESULT hr = m_resource_->Map(0, pReadRange, ppData);
  if (FAILED(hr)) {
    LOG_ERROR("Failed to map buffer");
    return false;
  }

  m_mappedData_ = *ppData;
  m_isMapped_   = true;
  return true;
}

void BufferDx12::unmap_(D3D12_RANGE* pWrittenRange) {
  if (!m_isMapped_ || !m_resource_) {
    return;
  }

  m_resource_->Unmap(0, pWrittenRange);
  m_mappedData_ = nullptr;
  m_isMapped_   = false;
}

bool BufferDx12::isUploadHeapBuffer_() const {
  auto isCpuAccess = (m_desc_.createFlags & BufferCreateFlag::CpuAccess) != BufferCreateFlag::None;
  auto isDynamic   = (m_desc_.type == BufferType::Dynamic);
  return isCpuAccess || isDynamic;
}

bool BufferDx12::isReadbackHeapBuffer_() const {
  auto isReadback = (m_desc_.createFlags & BufferCreateFlag::Readback) != BufferCreateFlag::None;
  return isReadback;
}

D3D12_RESOURCE_STATES BufferDx12::getInitialResourceState_() const {
  if ((m_desc_.createFlags & BufferCreateFlag::VertexBuffer) != BufferCreateFlag::None) {
    return D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
  }

  if ((m_desc_.createFlags & BufferCreateFlag::IndexBuffer) != BufferCreateFlag::None) {
    return D3D12_RESOURCE_STATE_INDEX_BUFFER;
  }

  if ((m_desc_.createFlags & BufferCreateFlag::Uav) != BufferCreateFlag::None) {
    return D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
  }

  if ((m_desc_.createFlags & BufferCreateFlag::IndirectCommand) != BufferCreateFlag::None) {
    return D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
  }

  if ((m_desc_.createFlags & BufferCreateFlag::Readback) != BufferCreateFlag::None) {
    return D3D12_RESOURCE_STATE_COPY_DEST;
  }

  LOG_WARN(
                    "BufferDx12" + m_desc_.debugName + "created without specific resource state. Defaulting to "
                    "D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER.");
  return D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
}

//-----------------------------------------------------------------------------------
// DirectBufferDx12 Implementation
//-----------------------------------------------------------------------------------

DirectBufferDx12::DirectBufferDx12(const BufferDesc& desc, DeviceDx12* device)
    : BufferDx12(desc, device)
    , m_stride_(desc.stride) {
  bool isDirectBuffer = (desc.createFlags & BufferCreateFlag::VertexBuffer) != BufferCreateFlag::None
                     || (desc.createFlags & BufferCreateFlag::IndexBuffer) != BufferCreateFlag::None
                     || (desc.createFlags & BufferCreateFlag::InstanceBuffer) != BufferCreateFlag::None;

  if (!isDirectBuffer) {
    LOG_WARN(
        "DirectBufferDx12 created without vertex, index, or instance buffer flags. "
        "Consider using DescriptorBufferDx12 for other buffer types.");
  }
}

bool DirectBufferDx12::isVertexBuffer() const {
  return (m_desc_.createFlags & BufferCreateFlag::VertexBuffer) != BufferCreateFlag::None;
}

bool DirectBufferDx12::isIndexBuffer() const {
  return (m_desc_.createFlags & BufferCreateFlag::IndexBuffer) != BufferCreateFlag::None;
}

bool DirectBufferDx12::isInstanceBuffer() const {
  return (m_desc_.createFlags & BufferCreateFlag::InstanceBuffer) != BufferCreateFlag::None;
}

//-----------------------------------------------------------------------------------
// DescriptorBufferDx12 Implementation
//-----------------------------------------------------------------------------------

DescriptorBufferDx12::DescriptorBufferDx12(const BufferDesc& desc, DeviceDx12* device)
    : BufferDx12(desc, device) {
  if (isConstantBuffer()) {
    createConstantBufferView();
  }

  if (isUnorderedAccessBuffer()) {
    createUnorderedAccessView();
  }

  if (isShaderResourceBuffer()) {
    createShaderResourceView();
  }

  if (!isConstantBuffer() && !isUnorderedAccessBuffer() && !isShaderResourceBuffer()) {
    if (desc.type == BufferType::Dynamic) {
      createConstantBufferView();
    } else {
      LOG_WARN(
          "DescriptorBufferDx12 created without CBV, UAV, or SRV flags. "
          "Consider using DirectBufferDx12 for vertex or index buffers.");
    }
  }
}

DescriptorBufferDx12::~DescriptorBufferDx12() {
  if (m_device_) {
    DescriptorHeapDx12* cpuHeap = m_device_->getCpuCbvSrvUavHeap();

    if (cpuHeap) {
      if (m_cbvDescriptorIndex_ != UINT32_MAX) {
        cpuHeap->free(m_cbvDescriptorIndex_);
      }

      if (m_uavDescriptorIndex_ != UINT32_MAX) {
        cpuHeap->free(m_uavDescriptorIndex_);
      }

      if (m_srvDescriptorIndex_ != UINT32_MAX) {
        cpuHeap->free(m_srvDescriptorIndex_);
      }
    }
  }
}

bool DescriptorBufferDx12::isConstantBuffer() const {
  return (m_desc_.createFlags & BufferCreateFlag::ConstantBuffer) != BufferCreateFlag::None;
}

bool DescriptorBufferDx12::isUnorderedAccessBuffer() const {
  return (m_desc_.createFlags & BufferCreateFlag::Uav) != BufferCreateFlag::None;
}

bool DescriptorBufferDx12::isShaderResourceBuffer() const {
  return (m_desc_.createFlags & BufferCreateFlag::ShaderResource) != BufferCreateFlag::None;
}

void DescriptorBufferDx12::createConstantBufferView() {
  if (!m_device_ || !m_resource_ || m_cbvDescriptorIndex_ != UINT32_MAX) {
    LOG_ERROR("CBV descriptor already created or invalid state");
    return;
  }

  DescriptorHeapDx12* cpuHeap = m_device_->getCpuCbvSrvUavHeap();
  if (!cpuHeap) {
    LOG_ERROR("CPU CBV/SRV/UAV descriptor heap not available");
    return;
  }

  m_cbvDescriptorIndex_ = cpuHeap->allocate();
  if (m_cbvDescriptorIndex_ == UINT32_MAX) {
    LOG_ERROR("Failed to allocate CBV descriptor");
    return;
  }

  m_cbvCpuHandle_ = cpuHeap->getCpuHandle(m_cbvDescriptorIndex_);

  D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
  cbvDesc.BufferLocation                  = m_resource_->GetGPUVirtualAddress();

  // Size must be aligned to 256 bytes for constant buffers
  cbvDesc.SizeInBytes = alignConstantBufferSize(m_desc_.size);

  m_device_->getDevice()->CreateConstantBufferView(&cbvDesc, m_cbvCpuHandle_);
}

void DescriptorBufferDx12::createUnorderedAccessView() {
  if (!m_device_ || !m_resource_ || m_uavDescriptorIndex_ != UINT32_MAX) {
    LOG_ERROR("UAV descriptor already created or invalid state");
    return;
  }

  DescriptorHeapDx12* cpuHeap = m_device_->getCpuCbvSrvUavHeap();
  if (!cpuHeap) {
    LOG_ERROR("CPU CBV/SRV/UAV descriptor heap not available");
    return;
  }

  // Allocate a descriptor
  m_uavDescriptorIndex_ = cpuHeap->allocate();
  if (m_uavDescriptorIndex_ == UINT32_MAX) {
    LOG_ERROR("Failed to allocate UAV descriptor");
    return;
  }

  m_uavCpuHandle_ = cpuHeap->getCpuHandle(m_uavDescriptorIndex_);

  D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
  uavDesc.Format                           = DXGI_FORMAT_UNKNOWN;
  uavDesc.ViewDimension                    = D3D12_UAV_DIMENSION_BUFFER;
  uavDesc.Buffer.FirstElement              = 0;
  // Size in bytes / 4 bytes per element (estimate)
  uavDesc.Buffer.NumElements          = static_cast<UINT>(m_desc_.size / 4);
  uavDesc.Buffer.StructureByteStride  = 0;
  uavDesc.Buffer.CounterOffsetInBytes = 0;
  uavDesc.Buffer.Flags                = D3D12_BUFFER_UAV_FLAG_NONE;

  m_device_->getDevice()->CreateUnorderedAccessView(m_resource_.Get(), nullptr, &uavDesc, m_uavCpuHandle_);
}

void DescriptorBufferDx12::createShaderResourceView() {
  if (!m_device_ || !m_resource_ || m_srvDescriptorIndex_ != UINT32_MAX) {
    LOG_ERROR("SRV descriptor already created or invalid state");
    return;
  }

  DescriptorHeapDx12* cpuHeap = m_device_->getCpuCbvSrvUavHeap();
  if (!cpuHeap) {
    LOG_ERROR("CPU CBV/SRV/UAV descriptor heap not available");
    return;
  }

  m_srvDescriptorIndex_ = cpuHeap->allocate();
  if (m_srvDescriptorIndex_ == UINT32_MAX) {
    LOG_ERROR("Failed to allocate SRV descriptor");
    return;
  }

  m_srvCpuHandle_ = cpuHeap->getCpuHandle(m_srvDescriptorIndex_);

  D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
  srvDesc.Format                          = DXGI_FORMAT_UNKNOWN;
  srvDesc.ViewDimension                   = D3D12_SRV_DIMENSION_BUFFER;
  srvDesc.Shader4ComponentMapping         = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
  srvDesc.Buffer.FirstElement             = 0;
  if (m_desc_.stride > 0) {
    // Structured Buffer
    srvDesc.Buffer.StructureByteStride = m_desc_.stride;
    srvDesc.Buffer.NumElements         = static_cast<UINT>(m_desc_.size / m_desc_.stride);
  } else {
    // Raw Buffer
    srvDesc.Buffer.StructureByteStride = 0;
    // Size in bytes / 4 bytes per element (estimate)
    srvDesc.Buffer.NumElements = static_cast<UINT>(m_desc_.size / 4);
  }

  srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

  m_device_->getDevice()->CreateShaderResourceView(m_resource_.Get(), &srvDesc, m_srvCpuHandle_);
}

}  // namespace rhi
}  // namespace gfx
}  // namespace arise

#endif  // ARISE_USE_DX12
