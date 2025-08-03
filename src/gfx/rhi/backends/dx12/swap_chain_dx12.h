#ifndef ARISE_SWAP_CHAIN_DX12_H
#define ARISE_SWAP_CHAIN_DX12_H

#include "gfx/rhi/interface/swap_chain.h"
#include "platform/windows/windows_platform_setup.h"

#ifdef ARISE_USE_DX12

#include <memory>
#include <vector>

namespace arise {
namespace gfx {
namespace rhi {

class DeviceDx12;
class TextureDx12;

class SwapChainDx12 : public SwapChain {
  public:
  SwapChainDx12(const SwapchainDesc& desc, DeviceDx12* device);
  ~SwapChainDx12() override;

  Texture* getCurrentImage() override;
  bool acquireNextImage(Semaphore* signalSemaphore = nullptr) override;
  bool present(Semaphore* waitSemaphore = nullptr)            override;
  bool     resize(uint32_t width, uint32_t height) override;

  TextureFormat getFormat() const override;
  uint32_t      getWidth() const override;
  uint32_t      getHeight() const override;
  uint32_t      getBufferCount() const override;

  // DX12-specific methods
  IDXGISwapChain3* getNativeSwapChain() const { return m_swapChain_.Get(); }

  private:
  void cleanup_();
  bool createSwapChain_();
  bool createRenderTargetViews_();
  bool createBackBufferTextures_();

  DeviceDx12*             m_device_;
  ComPtr<IDXGISwapChain3> m_swapChain_;

  std::vector<ComPtr<ID3D12Resource>>       m_backBuffers_;
  std::vector<std::unique_ptr<TextureDx12>> m_textures_;

  std::vector<uint32_t> m_backBuferRtvDescriptorIndices; 

  HANDLE   m_frameLatencyWaitableObject_ = nullptr;  // For better frame pacing
};

}  // namespace rhi
}  // namespace gfx
}  // namespace arise

#endif  // ARISE_USE_DX12

#endif  // ARISE_SWAP_CHAIN_DX12_H