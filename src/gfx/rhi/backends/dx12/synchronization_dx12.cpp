#include "gfx/rhi/backends/dx12/synchronization_dx12.h"

#ifdef ARISE_USE_DX12

#include "gfx/rhi/backends/dx12/device_dx12.h"
#include "utils/logger/global_logger.h"

namespace arise {
namespace gfx {
namespace rhi {

//-------------------------------------------------------------------------
// FenceDx12 implementation
//-------------------------------------------------------------------------

FenceDx12::FenceDx12(const FenceDesc& desc, DeviceDx12* device)
    : Fence(desc)
    , m_device_(device)
    , m_fenceValue_(desc.signaled ? 1 : 0) {
  HRESULT hr = device->getDevice()->CreateFence(m_fenceValue_, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence_));

  if (FAILED(hr)) {
    GlobalLogger::Log(LogLevel::Error, "Failed to create DirectX12 fence");
    return;
  }

  m_event_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);
  if (m_event_ == nullptr) {
    GlobalLogger::Log(LogLevel::Error, "Failed to create fence event handle");
  }
}

FenceDx12::~FenceDx12() {
  if (m_event_) {
    CloseHandle(m_event_);
    m_event_ = nullptr;
  }
}

void FenceDx12::reset() {
  m_signaled_ = false;
}

bool FenceDx12::wait(uint64_t timeout) {
  if (m_fence_->GetCompletedValue() >= m_fenceValue_) {
    m_signaled_ = true;
    return true;
  }

  HRESULT hr = m_fence_->SetEventOnCompletion(m_fenceValue_, m_event_);
  if (FAILED(hr)) {
    GlobalLogger::Log(LogLevel::Error, "Failed to set fence completion event");
    return false;
  }

  constexpr auto kNanosecondsPerMillisecond = 1'000'000;
  DWORD timeoutMs = (timeout == UINT64_MAX) ? INFINITE : static_cast<DWORD>(timeout / kNanosecondsPerMillisecond);

  DWORD result = WaitForSingleObject(m_event_, timeoutMs);
  if (result == WAIT_OBJECT_0) {
    m_signaled_ = true;
    return true;
  }

  return false;
}

bool FenceDx12::isSignaled() {
  if (m_fence_->GetCompletedValue() >= m_fenceValue_) {
    m_signaled_ = true;
    return true;
  }

  return false;
}

void FenceDx12::signal(ID3D12CommandQueue* queue) {
  m_fenceValue_++;

  HRESULT hr = queue->Signal(m_fence_.Get(), m_fenceValue_);
  if (FAILED(hr)) {
    GlobalLogger::Log(LogLevel::Error, "Failed to signal DirectX12 fence");
  }
}

//-------------------------------------------------------------------------
// SemaphoreDx12 implementation
//-------------------------------------------------------------------------

SemaphoreDx12::SemaphoreDx12(DeviceDx12* device)
    : m_device_(device) {
  HRESULT hr = device->getDevice()->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence_));
  if (FAILED(hr)) {
    GlobalLogger::Log(LogLevel::Error, "Failed to create DirectX12 fence for semaphore");
  }

  m_event_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);
  if (m_event_ == nullptr) {
    GlobalLogger::Log(LogLevel::Error, "Failed to create event for DirectX12 semaphore");
  }
}

SemaphoreDx12::~SemaphoreDx12() {
  if (m_event_) {
    CloseHandle(m_event_);
    m_event_ = nullptr;
  }
}

void SemaphoreDx12::signal(ID3D12CommandQueue* queue) {
  m_value_++;

  HRESULT hr = queue->Signal(m_fence_.Get(), m_value_);
  if (FAILED(hr)) {
    GlobalLogger::Log(LogLevel::Error, "Failed to signal DirectX12 semaphore");
  }
}

void SemaphoreDx12::wait() {
  if (m_fence_->GetCompletedValue() < m_value_) {
    HRESULT hr = m_fence_->SetEventOnCompletion(m_value_, m_event_);
    if (FAILED(hr)) {
      GlobalLogger::Log(LogLevel::Error, "Failed to set completion event for DirectX12 semaphore");
      return;
    }

    WaitForSingleObject(m_event_, INFINITE);
  }
}

}  // namespace rhi
}  // namespace gfx
}  // namespace arise

#endif  // ARISE_USE_DX12