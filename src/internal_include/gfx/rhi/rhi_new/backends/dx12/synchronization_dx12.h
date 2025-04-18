#ifndef GAME_ENGINE_SYNCHRONIZATION_DX12_H
#define GAME_ENGINE_SYNCHRONIZATION_DX12_H

#include "gfx/rhi/rhi_new/interface/synchronization.h"
#include "platform/windows/windows_platform_setup.h"

#ifdef GAME_ENGINE_RHI_DX12

namespace game_engine {
namespace gfx {
namespace rhi {

class DeviceDx12;

class FenceDx12 : public Fence {
  public:
  FenceDx12(const FenceDesc& desc, DeviceDx12* device);
  ~FenceDx12() override;

  void reset() override;
  bool wait(uint64_t timeout = UINT64_MAX) override;
  bool isSignaled() override;

  // DirectX12-specific methods
  ID3D12Fence* getFence() const { return m_fence_.Get(); }

  void signal(ID3D12CommandQueue* queue);

  uint64_t getValue() const { return m_fenceValue_; }

  private:
  DeviceDx12*         m_device_ = nullptr;
  ComPtr<ID3D12Fence> m_fence_;
  HANDLE              m_event_      = nullptr;
  uint64_t            m_fenceValue_ = 0;
};

/**
 * @note Unlike Vulkan, DirectX 12 does not have a native semaphore concept.
 * Instead, all synchronization is accomplished through ID3D12Fence objects.
 *
 * This class emulates Vulkan-style semaphore behavior using a DirectX 12 fence,
 * providing a consistent API across both rendering backends. While previous
 * implementations omitted this class, we've included it to:
 *   - Maintain a unified abstraction layer
 *   - Preserve the same API contract regardless of backend
 *   - Simplify cross-API development for educational purposes
 *
 * Internally, this implementation uses ID3D12Fence with appropriate value
 * tracking to achieve semaphore-like semantics.
 */
class SemaphoreDx12 : public Semaphore {
  public:
  SemaphoreDx12(DeviceDx12* device);
  ~SemaphoreDx12() override;

  // DirectX12-specific methods
  void signal(ID3D12CommandQueue* queue);
  void wait();

  private:
  DeviceDx12*         m_device_ = nullptr;
  ComPtr<ID3D12Fence> m_fence_;
  HANDLE              m_event_ = nullptr;
  uint64_t            m_value_ = 0;
};

}  // namespace rhi
}  // namespace gfx
}  // namespace game_engine

#endif  // GAME_ENGINE_RHI_DX12
#endif  // GAME_ENGINE_SYNCHRONIZATION_DX12_H