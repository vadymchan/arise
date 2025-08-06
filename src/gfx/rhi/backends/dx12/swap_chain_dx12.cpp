#include "gfx/rhi/backends/dx12/swap_chain_dx12.h"

#include "gfx/rhi/backends/dx12/descriptor_dx12.h"
#include "gfx/rhi/backends/dx12/device_dx12.h"
#include "gfx/rhi/backends/dx12/rhi_enums_dx12.h"
#include "gfx/rhi/backends/dx12/synchronization_dx12.h"
#include "gfx/rhi/backends/dx12/texture_dx12.h"
#include "platform/common/window.h"
#include "utils/logger/log.h"

#include <SDL_syswm.h>

namespace arise {
namespace gfx {
namespace rhi {

SwapChainDx12::SwapChainDx12(const SwapchainDesc& desc, DeviceDx12* device)
    : SwapChain(desc)
    , m_device_(device) {
  if (!createSwapChain_()) {
    LOG_ERROR("Failed to create swap chain (IDXGISwapChain)");
    return;
  }

  if (!createRenderTargetViews_()) {
    LOG_ERROR("Failed to create render target views for swap chain");
    return;
  }

  if (!createBackBufferTextures_()) {
    LOG_ERROR("Failed to create texture objects for swap chain back buffers");
    return;
  }
}

SwapChainDx12::~SwapChainDx12() {
  cleanup_();

  if (m_frameLatencyWaitableObject_) {
    CloseHandle(m_frameLatencyWaitableObject_);
    m_frameLatencyWaitableObject_ = nullptr;
  }
}

void SwapChainDx12::cleanup_() {
  m_textures_.clear();

  for (auto& backBuffer : m_backBuffers_) {
    backBuffer.Reset();
  }
  m_backBuffers_.clear();

  if (m_device_) {
    auto* rtvHeap = m_device_->getCpuRtvHeap();
    if (rtvHeap) {
      for (uint32_t index : m_backBuferRtvDescriptorIndices) {
        if (index != UINT32_MAX) {
          rtvHeap->free(index);
        }
      }
    }
  }
  m_backBuferRtvDescriptorIndices.clear();
}

bool SwapChainDx12::createSwapChain_() {
  auto          window = m_device_->getWindow();
  SDL_SysWMinfo wmInfo;
  SDL_VERSION(&wmInfo.version);
  if (!SDL_GetWindowWMInfo(static_cast<SDL_Window*>(window->getNativeWindowHandle()), &wmInfo)) {
    LOG_ERROR("Failed to get window handle" + std::string(SDL_GetError()));
    return false;
  }

  HWND hwnd = wmInfo.info.win.window;

  DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
  swapChainDesc.Width                 = m_desc_.width;
  swapChainDesc.Height                = m_desc_.height;
  swapChainDesc.Format                = g_getTextureFormatDx12(m_desc_.format);
  swapChainDesc.SampleDesc.Count      = 1;
  swapChainDesc.SampleDesc.Quality    = 0;
  swapChainDesc.BufferUsage           = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  swapChainDesc.BufferCount           = m_desc_.bufferCount;
  swapChainDesc.SwapEffect            = DXGI_SWAP_EFFECT_FLIP_DISCARD;
  swapChainDesc.Flags                 = 0;  // No special flags for now (like vsync or frame latency waitable object)

  ComPtr<IDXGISwapChain1> swapChain1;
  HRESULT                 hr = m_device_->getFactory()->CreateSwapChainForHwnd(
      m_device_->getCommandQueue(), hwnd, &swapChainDesc, nullptr, nullptr, &swapChain1);

  if (FAILED(hr)) {
    LOG_ERROR("Failed to create swap chain");
    return false;
  }

  hr = swapChain1.As(&m_swapChain_);
  if (FAILED(hr)) {
    LOG_ERROR("Failed to convert swap chain to IDXGISwapChain3");
    return false;
  }

  return true;
}

bool SwapChainDx12::createRenderTargetViews_() {
  DXGI_SWAP_CHAIN_DESC1 swapChainDesc;
  m_swapChain_->GetDesc1(&swapChainDesc);
  uint32_t bufferCount = swapChainDesc.BufferCount;

  m_backBuffers_.resize(bufferCount);

  m_backBuferRtvDescriptorIndices.resize(bufferCount, UINT32_MAX);

  auto* rtvHeap = m_device_->getCpuRtvHeap();
  if (!rtvHeap) {
    LOG_ERROR("RTV heap not available");
    return false;
  }

  for (uint32_t i = 0; i < bufferCount; i++) {
    HRESULT hr = m_swapChain_->GetBuffer(i, IID_PPV_ARGS(&m_backBuffers_[i]));
    if (FAILED(hr)) {
      LOG_ERROR("Failed to get swap chain buffer");
      return false;
    }

    m_backBuferRtvDescriptorIndices[i] = rtvHeap->allocate();
    if (m_backBuferRtvDescriptorIndices[i] == UINT32_MAX) {
      LOG_ERROR("Failed to allocate RTV descriptor");
      return false;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvHeap->getCpuHandle(m_backBuferRtvDescriptorIndices[i]);
    m_device_->getDevice()->CreateRenderTargetView(m_backBuffers_[i].Get(), nullptr, rtvHandle);
  }

  return true;
}

bool SwapChainDx12::createBackBufferTextures_() {
  uint32_t bufferCount = static_cast<uint32_t>(m_backBuffers_.size());
  m_textures_.resize(bufferCount);

  auto* rtvHeap = m_device_->getCpuRtvHeap();
  if (!rtvHeap) {
    return false;
  }

  for (uint32_t i = 0; i < bufferCount; i++) {
    TextureDesc textureDesc;
    textureDesc.type          = TextureType::Texture2D;
    textureDesc.format        = getFormat();
    textureDesc.width         = m_desc_.width;
    textureDesc.height        = m_desc_.height;
    textureDesc.depth         = 1;
    textureDesc.mipLevels     = 1;
    textureDesc.arraySize     = 1;
    textureDesc.sampleCount   = MSAASamples::Count1;
    textureDesc.createFlags   = TextureCreateFlag::Rtv;
    textureDesc.initialLayout = ResourceLayout::PresentSrc;
    textureDesc.debugName     = "back_buffer_texture";

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvHeap->getCpuHandle(m_backBuferRtvDescriptorIndices[i]);

    m_textures_[i] = std::make_unique<TextureDx12>(
        m_device_, textureDesc, m_backBuffers_[i].Get(), rtvHandle, D3D12_CPU_DESCRIPTOR_HANDLE{});
  }

  return true;
}

Texture* SwapChainDx12::getCurrentImage() {
  return m_textures_[m_currentFrameIndex].get();
}

bool SwapChainDx12::acquireNextImage(Semaphore* signalSemaphore) {
  m_currentFrameIndex = m_swapChain_->GetCurrentBackBufferIndex();
  if (m_frameLatencyWaitableObject_) {
    constexpr DWORD waitTimeOneSecond = 1000;
    DWORD           waitResult        = WaitForSingleObjectEx(m_frameLatencyWaitableObject_, waitTimeOneSecond, TRUE);
    if (waitResult != WAIT_OBJECT_0) {
      LOG_WARN("Failed to wait for frame latency waitable object");
    }
  }

  return true;
}

bool SwapChainDx12::present(Semaphore* waitSemaphore) {
  if (waitSemaphore) {
    SemaphoreDx12* semaphoreDx12 = dynamic_cast<SemaphoreDx12*>(waitSemaphore);
    if (semaphoreDx12) {
      semaphoreDx12->wait();
    }
  }

  UINT syncInterval = 0;  // No vsync
  UINT presentFlags = 0;

  HRESULT hr = m_swapChain_->Present(syncInterval, presentFlags);

  if (FAILED(hr)) {
    if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) {
      LOG_ERROR("GPU device removed or reset");
    } else {
      LOG_ERROR("Failed to present swap chain");
    }
    return false;
  }

  return true;
}

bool SwapChainDx12::resize(uint32_t width, uint32_t height) {
  if (width == m_desc_.width && height == m_desc_.height) {
    return true;
  }

  m_device_->waitIdle();

  m_desc_.width  = width;
  m_desc_.height = height;

  cleanup_();

  DXGI_SWAP_CHAIN_DESC1 swapChainDesc;
  m_swapChain_->GetDesc1(&swapChainDesc);

  HRESULT hr = m_swapChain_->ResizeBuffers(
      swapChainDesc.BufferCount, width, height, swapChainDesc.Format, swapChainDesc.Flags);

  if (FAILED(hr)) {
    LOG_ERROR("Failed to resize swap chain buffers");
    return false;
  }

  m_currentFrameIndex = m_swapChain_->GetCurrentBackBufferIndex();

  if (!createRenderTargetViews_() || !createBackBufferTextures_()) {
    LOG_ERROR("Failed to recreate render targets after resize");
    return false;
  }

  return true;
}

TextureFormat SwapChainDx12::getFormat() const {
  DXGI_SWAP_CHAIN_DESC1 desc;
  m_swapChain_->GetDesc1(&desc);
  return g_getTextureFormatDx12(desc.Format);
}

uint32_t SwapChainDx12::getWidth() const {
  return m_desc_.width;
}

uint32_t SwapChainDx12::getHeight() const {
  return m_desc_.height;
}

uint32_t SwapChainDx12::getBufferCount() const {
  DXGI_SWAP_CHAIN_DESC1 desc;
  m_swapChain_->GetDesc1(&desc);
  return desc.BufferCount;
}

}  // namespace rhi
}  // namespace gfx
}  // namespace arise