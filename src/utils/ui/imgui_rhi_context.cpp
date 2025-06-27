#include "utils/ui/imgui_rhi_context.h"

#include "gfx/rhi/backends/dx12/command_buffer_dx12.h"
#include "gfx/rhi/backends/dx12/device_dx12.h"
#include "gfx/rhi/backends/dx12/texture_dx12.h"
#include "gfx/rhi/backends/vulkan/command_buffer_vk.h"
#include "gfx/rhi/backends/vulkan/descriptor_vk.h"
#include "gfx/rhi/backends/vulkan/device_vk.h"
#include "gfx/rhi/backends/vulkan/render_pass_vk.h"
#include "gfx/rhi/backends/vulkan/sampler_vk.h"
#include "gfx/rhi/backends/vulkan/texture_vk.h"
#include "profiler/profiler.h"
#include "utils/logger/global_logger.h"

#include <imgui_impl_dx12.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_vulkan.h>

namespace arise {
namespace gfx {

ImGuiRHIContext::~ImGuiRHIContext() {
  shutdown();
}

bool ImGuiRHIContext::initialize(Window* window, rhi::Device* device, uint32_t swapChainBufferCount) {
  if (!window || !device) {
    GlobalLogger::Log(LogLevel::Error, "ImGuiRHIContext::initialize: Invalid window or device");
    return false;
  }

  m_window       = window;
  m_device       = device;
  m_renderingApi = device->getApiType();

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();

  ImGuiIO& io     = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
  io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

  ImGui::StyleColorsDark();
  if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
    ImGuiStyle& style                 = ImGui::GetStyle();
    style.WindowRounding              = 0.0f;
    style.Colors[ImGuiCol_WindowBg].w = 1.0f;
  }

  if (m_renderingApi == rhi::RenderingApi::Vulkan) {
    if (!ImGui_ImplSDL2_InitForVulkan(window->getNativeWindowHandle())) {
      GlobalLogger::Log(LogLevel::Error, "Failed to initialize ImGui SDL2 backend for Vulkan");
      return false;
    }
  } else if (m_renderingApi == rhi::RenderingApi::Dx12) {
    if (!ImGui_ImplSDL2_InitForD3D(window->getNativeWindowHandle())) {
      GlobalLogger::Log(LogLevel::Error, "Failed to initialize ImGui SDL2 backend for D3D");
      return false;
    }
  } else {
    GlobalLogger::Log(LogLevel::Error, "Unsupported rendering API for ImGui");
    return false;
  }

  createRenderPass();

  m_framebuffers.reserve(swapChainBufferCount);

  rhi::SamplerDesc samplerDesc;
  samplerDesc.minFilter     = rhi::TextureFilter::Linear;
  samplerDesc.magFilter     = rhi::TextureFilter::Linear;
  samplerDesc.addressModeU  = rhi::TextureAddressMode::Repeat;
  samplerDesc.addressModeV  = rhi::TextureAddressMode::Repeat;
  samplerDesc.addressModeW  = rhi::TextureAddressMode::Repeat;
  samplerDesc.maxAnisotropy = 1.0f;

  m_sampler = m_device->createSampler(samplerDesc);

  bool success = false;
  if (m_renderingApi == rhi::RenderingApi::Vulkan) {
    success = initializeVulkan(device, swapChainBufferCount);
  } else if (m_renderingApi == rhi::RenderingApi::Dx12) {
    success = initializeDx12(device, swapChainBufferCount);
  }

  if (success) {
    m_initialized = true;
    GlobalLogger::Log(LogLevel::Info, "ImGui RHI context initialized successfully");
  }

  return success;
}

void ImGuiRHIContext::createRenderPass() {
  rhi::RenderPassDesc renderPassDesc;

  rhi::RenderPassAttachmentDesc colorAttachmentDesc;
  colorAttachmentDesc.format        = rhi::TextureFormat::Bgra8;
  colorAttachmentDesc.samples       = rhi::MSAASamples::Count1;
  colorAttachmentDesc.loadStoreOp   = rhi::AttachmentLoadStoreOp::LoadStore;
  colorAttachmentDesc.initialLayout = rhi::ResourceLayout::ColorAttachment;
  colorAttachmentDesc.finalLayout   = rhi::ResourceLayout::PresentSrc;
  renderPassDesc.colorAttachments.push_back(colorAttachmentDesc);

  m_renderPass = m_device->createRenderPass(renderPassDesc);
}

void ImGuiRHIContext::shutdown() {
  if (!m_initialized) {
    return;
  }

  m_framebuffers.clear();
  m_renderPass.reset();
  m_sampler.reset();

  if (m_renderingApi == rhi::RenderingApi::Vulkan) {
    ImGui_ImplVulkan_Shutdown();

    if (m_imguiPoolManager) {
      m_imguiPoolManager->release();
      m_imguiPoolManager.reset();
    }
  }
#ifdef ARISE_RHI_DX12
  else if (m_renderingApi == rhi::RenderingApi::Dx12) {
    m_dx12ImGuiDescriptorHeap.reset();

    ImGui_ImplDX12_Shutdown();
  }
#endif

  ImGui_ImplSDL2_Shutdown();
  ImGui::DestroyContext();

  m_initialized = false;
  m_window      = nullptr;
  m_device      = nullptr;
}

void ImGuiRHIContext::beginFrame() {
  if (!m_initialized) {
    return;
  }

  if (m_renderingApi == rhi::RenderingApi::Vulkan) {
    ImGui_ImplVulkan_NewFrame();
  } else if (m_renderingApi == rhi::RenderingApi::Dx12) {
    ImGui_ImplDX12_NewFrame();
  }

  ImGui_ImplSDL2_NewFrame();
  ImGui::NewFrame();
}

void ImGuiRHIContext::endFrame(rhi::CommandBuffer*       cmdBuffer,
                               rhi::Texture*             targetTexture,
                               const math::Dimension2i& viewportDimension,
                               uint32_t                  currentFrameIndex) {
  if (!m_initialized || !cmdBuffer || !targetTexture) {
    if (!m_initialized) {
      GlobalLogger::Log(LogLevel::Warning, "ImGuiRHIContext not initialized in endFrame");
    }
    if (!cmdBuffer) {
      GlobalLogger::Log(LogLevel::Warning, "Command buffer is null in endFrame");
    }
    if (!targetTexture) {
      GlobalLogger::Log(LogLevel::Warning, "Target texture is null in endFrame");
    }
    return;
  }

  ImGui::Render();

  GPU_ZONE_NC(cmdBuffer, "ImGui Render", color::ORANGE);

  if (m_renderingApi == rhi::RenderingApi::Vulkan) {
    renderImGuiVulkan(cmdBuffer, targetTexture, viewportDimension, currentFrameIndex);
  } else if (m_renderingApi == rhi::RenderingApi::Dx12) {
    renderImGuiDx12(cmdBuffer, targetTexture, viewportDimension, currentFrameIndex);
  }

  ImGuiIO& io = ImGui::GetIO();
  if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
    ImGui::UpdatePlatformWindows();
    ImGui::RenderPlatformWindowsDefault();
  }
}

rhi::Framebuffer* ImGuiRHIContext::getOrCreateFramebuffer(rhi::Texture*             targetTexture,
                                                          const math::Dimension2i& dimensions,
                                                          uint32_t                  frameIndex) {
  if (frameIndex >= m_framebuffers.size()) {
    m_framebuffers.resize(frameIndex + 1);
  }

  auto width  = dimensions.width() > 0 ? dimensions.width() : 1;
  auto height = dimensions.height() > 0 ? dimensions.height() : 1;

  if (!m_framebuffers[frameIndex]) {
    rhi::FramebufferDesc framebufferDesc;
    framebufferDesc.width  = width;
    framebufferDesc.height = height;
    framebufferDesc.colorAttachments.push_back(targetTexture);
    framebufferDesc.renderPass = m_renderPass.get();

    m_framebuffers[frameIndex] = m_device->createFramebuffer(framebufferDesc);
  }

  // TODO: add check and log that the texture pointer is the same as the one in the framebuffer
  return m_framebuffers[frameIndex].get();
}

void ImGuiRHIContext::resize(const math::Dimension2i& newDimensions) {
  if (m_currentFramebufferSize.width() == newDimensions.width()
      && m_currentFramebufferSize.height() == newDimensions.height()) {
    return;
  }

  m_currentFramebufferSize = newDimensions;

  m_framebuffers.clear();
}

ImTextureID ImGuiRHIContext::createTextureID(rhi::Texture* texture, uint32_t currentIndex) {
  if (!m_initialized || !texture) {
    GlobalLogger::Log(LogLevel::Warning,
                      "Cannot create texture ID - ImGuiRHIContext not initialized or texture is null");
    return (ImTextureID)0;
  }

  ImTextureID texID = 0;

  if (m_renderingApi == rhi::RenderingApi::Vulkan) {
    // Use ImGui_ImplVulkan_AddTexture for Vulkan which creates a descriptor set
    auto textureVk = static_cast<rhi::TextureVk*>(texture);
    auto samplerVk = static_cast<rhi::SamplerVk*>(m_sampler.get());

    texID = reinterpret_cast<ImTextureID>(ImGui_ImplVulkan_AddTexture(
        samplerVk->getSampler(), textureVk->getImageView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
  }
#ifdef ARISE_RHI_DX12
  else if (m_renderingApi == rhi::RenderingApi::Dx12) {
    // For DX12, we copy the texture's descriptor to the ImGui descriptor heap
    auto textureDx12 = static_cast<rhi::TextureDx12*>(texture);
    auto deviceDx12  = static_cast<rhi::DeviceDx12*>(m_device);

    // Get the SRV CPU handle from the texture
    D3D12_CPU_DESCRIPTOR_HANDLE srcCpuHandle = textureDx12->getSrvHandle();

    // We'll use the index currently used by ImGui for its font texture
    // This is a simplification as requested - we're not tracking indices
    // ImGui expects descriptors at index 1+
    uint32_t indexOffset = 1;  // Offset for the font texture

    uint32_t destIndex = indexOffset + currentIndex;

    // Copy the descriptor
    deviceDx12->getDevice()->CopyDescriptorsSimple(
        1, m_dx12ImGuiDescriptorHeap->getCpuHandle(destIndex), srcCpuHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    // Return the GPU handle as texture ID
    texID = (ImTextureID)m_dx12ImGuiDescriptorHeap->getGpuHandle(destIndex).ptr;
  }
#endif

  return texID;
}

void ImGuiRHIContext::releaseTextureID(ImTextureID textureID) {
  if (!m_initialized || !textureID) {
    return;
  }

  if (m_renderingApi == rhi::RenderingApi::Vulkan) {
    auto deviceVk = static_cast<rhi::DeviceVk*>(m_device);

    // TODO: in future, consider ring buffer for descriptor sets
    VkDescriptorSet set = reinterpret_cast<VkDescriptorSet>(textureID);
    vkFreeDescriptorSets(deviceVk->getDevice(), m_imguiPoolManager->getPool(), 1, &set);
  }

  // For DirectX 12, we don't need to do anything special since we're
  // just using a fixed location in the descriptor heap
#ifdef ARISE_RHI_DX12
  // No cleanup needed for DX12 in this simple approach
#endif
}

bool ImGuiRHIContext::initializeVulkan(rhi::Device* device, uint32_t swapChainBufferCount) {
  auto deviceVk     = static_cast<rhi::DeviceVk*>(device);
  auto renderPassVk = static_cast<rhi::RenderPassVk*>(m_renderPass.get());

  m_imguiPoolManager = std::make_unique<rhi::DescriptorPoolManager>();
  if (!m_imguiPoolManager->initialize(deviceVk->getDevice(), 1000)) {
    GlobalLogger::Log(LogLevel::Error, "Failed to initialize ImGui descriptor pool manager");
    return false;
  }

  ImGui_ImplVulkan_InitInfo initInfo = {};
  initInfo.Instance                  = deviceVk->getInstance();
  initInfo.PhysicalDevice            = deviceVk->getPhysicalDevice();
  initInfo.Device                    = deviceVk->getDevice();
  initInfo.QueueFamily               = deviceVk->getQueueFamilyIndices().graphicsFamily.value_or(0);
  initInfo.Queue                     = deviceVk->getGraphicsQueue();
  initInfo.PipelineCache             = VK_NULL_HANDLE;
  initInfo.DescriptorPool            = m_imguiPoolManager->getPool();
  initInfo.MinImageCount             = swapChainBufferCount;
  initInfo.ImageCount                = swapChainBufferCount;
  initInfo.MSAASamples               = VK_SAMPLE_COUNT_1_BIT;
  initInfo.RenderPass                = renderPassVk->getRenderPass();

  if (!ImGui_ImplVulkan_Init(&initInfo)) {
    GlobalLogger::Log(LogLevel::Error, "Failed to initialize ImGui Vulkan implementation");
    return false;
  }

  // Upload fonts
  // ImGui_ImplVulkan_CreateFontsTexture();

  GlobalLogger::Log(LogLevel::Info, "Successfully initialized ImGui Vulkan backend");
  return true;
}

bool ImGuiRHIContext::initializeDx12(rhi::Device* device, uint32_t swapChainBufferCount) {
  auto deviceDx12 = static_cast<rhi::DeviceDx12*>(device);

  m_dx12ImGuiDescriptorHeap = std::make_unique<rhi::DescriptorHeapDx12>();
  if (!m_dx12ImGuiDescriptorHeap->initialize(deviceDx12->getDevice(),
                                             D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
                                             128,  // Reduced number of descriptors since we're using simple approach
                                             true  // Shader visible
                                             )) {
    GlobalLogger::Log(LogLevel::Error, "Failed to create ImGui descriptor heap");
    return false;
  }

  // Initialize ImGui DX12
  ImGui_ImplDX12_InitInfo initInfo      = {};
  initInfo.Device                       = deviceDx12->getDevice();
  initInfo.CommandQueue                 = deviceDx12->getCommandQueue();
  initInfo.NumFramesInFlight            = swapChainBufferCount;
  initInfo.RTVFormat                    = DXGI_FORMAT_B8G8R8A8_UNORM;
  initInfo.SrvDescriptorHeap            = m_dx12ImGuiDescriptorHeap->getHeap();
  initInfo.LegacySingleSrvCpuDescriptor = m_dx12ImGuiDescriptorHeap->getCpuHandle(0);
  initInfo.LegacySingleSrvGpuDescriptor = m_dx12ImGuiDescriptorHeap->getGpuHandle(0);

  if (!ImGui_ImplDX12_Init(&initInfo)) {
    GlobalLogger::Log(LogLevel::Error, "Failed to initialize ImGui DX12 implementation");
    return false;
  }

  if (!ImGui_ImplDX12_CreateDeviceObjects()) {
    GlobalLogger::Log(LogLevel::Error, "Failed to create ImGui DX12 device objects");
    return false;
  }

  GlobalLogger::Log(LogLevel::Info, "Successfully initialized ImGui DirectX 12 backend");
  return true;
}

void ImGuiRHIContext::renderImGuiVulkan(rhi::CommandBuffer*       cmdBuffer,
                                        rhi::Texture*             targetTexture,
                                        const math::Dimension2i& viewportDimension,
                                        uint32_t                  currentFrameIndex) {
  auto cmdBufferVk = static_cast<rhi::CommandBufferVk*>(cmdBuffer);

  {
    GPU_ZONE_NC(cmdBuffer, "UI Resource Barrier", color::RED);
    gfx::rhi::ResourceBarrierDesc backBufferBarrier;
    backBufferBarrier.texture   = targetTexture;
    backBufferBarrier.oldLayout = targetTexture->getCurrentLayoutType();
    backBufferBarrier.newLayout = gfx::rhi::ResourceLayout::ColorAttachment;
    cmdBuffer->resourceBarrier(backBufferBarrier);
  }

  rhi::Framebuffer* framebuffer = getOrCreateFramebuffer(targetTexture, viewportDimension, currentFrameIndex);

  std::vector<rhi::ClearValue> clearValues(1);

  cmdBuffer->beginRenderPass(m_renderPass.get(), framebuffer, clearValues);

  auto width  = viewportDimension.width() > 0 ? viewportDimension.width() : 1;
  auto height = viewportDimension.height() > 0 ? viewportDimension.height() : 1;

  rhi::Viewport viewport;
  viewport.x        = 0.0f;
  viewport.y        = 0.0f;
  viewport.width    = width;
  viewport.height   = height;
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;
  cmdBuffer->setViewport(viewport);

  rhi::ScissorRect scissor;
  scissor.x      = 0;
  scissor.y      = 0;
  scissor.width  = width;
  scissor.height = height;
  cmdBuffer->setScissor(scissor);

  {
    GPU_ZONE_NC(cmdBuffer, "UI Draw Calls", color::GREEN);
    ImDrawData* drawData = ImGui::GetDrawData();
    ImGui_ImplVulkan_RenderDrawData(drawData, cmdBufferVk->getCommandBuffer());
  }

  cmdBuffer->endRenderPass();
}

void ImGuiRHIContext::renderImGuiDx12(rhi::CommandBuffer*       cmdBuffer,
                                      rhi::Texture*             targetTexture,
                                      const math::Dimension2i& viewportDimension,
                                      uint32_t                  currentFrameIndex) {
  auto cmdBufferDx12 = static_cast<rhi::CommandBufferDx12*>(cmdBuffer);

  rhi::Framebuffer* framebuffer = getOrCreateFramebuffer(targetTexture, viewportDimension, currentFrameIndex);

  std::vector<rhi::ClearValue> clearValues(1);
  cmdBuffer->beginRenderPass(m_renderPass.get(), framebuffer, clearValues);

  rhi::Viewport viewport;
  viewport.x        = 0.0f;
  viewport.y        = 0.0f;
  viewport.width    = viewportDimension.width();
  viewport.height   = viewportDimension.height();
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;
  cmdBuffer->setViewport(viewport);

  rhi::ScissorRect scissor;
  scissor.x      = 0;
  scissor.y      = 0;
  scissor.width  = viewportDimension.width();
  scissor.height = viewportDimension.height();
  cmdBuffer->setScissor(scissor);

  {
    GPU_ZONE_NC(cmdBuffer, "UI Setup", color::PURPLE);
    ID3D12DescriptorHeap* heaps[] = {m_dx12ImGuiDescriptorHeap->getHeap()};
    cmdBufferDx12->getCommandList()->SetDescriptorHeaps(1, heaps);
  }

  {
    GPU_ZONE_NC(cmdBuffer, "UI Draw Calls", color::GREEN);
    ImDrawData* drawData = ImGui::GetDrawData();
    ImGui_ImplDX12_RenderDrawData(drawData, cmdBufferDx12->getCommandList());
  }

  cmdBuffer->endRenderPass();
}

}  // namespace gfx
}  // namespace arise