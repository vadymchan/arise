#include "gfx/rhi/backends/dx12/command_buffer_dx12.h"

#ifdef ARISE_USE_DX12

#include "gfx/rhi/backends/dx12/buffer_dx12.h"
#include "gfx/rhi/backends/dx12/descriptor_dx12.h"
#include "gfx/rhi/backends/dx12/device_dx12.h"
#include "gfx/rhi/backends/dx12/framebuffer_dx12.h"
#include "gfx/rhi/backends/dx12/pipeline_dx12.h"
#include "gfx/rhi/backends/dx12/render_pass_dx12.h"
#include "gfx/rhi/backends/dx12/rhi_enums_dx12.h"
#include "gfx/rhi/backends/dx12/texture_dx12.h"
#include "profiler/gpu.h"
#include "utils/color/color.h"
#include "utils/logger/log.h"

#include <algorithm>

#if defined(USE_PIX)
#include <pix3.h>
#endif

namespace arise {
namespace gfx {
namespace rhi {

//-------------------------------------------------------------------------
// CommandBufferDx12 implementation
//-------------------------------------------------------------------------

CommandBufferDx12::CommandBufferDx12(DeviceDx12*                       device,
                                     ComPtr<ID3D12GraphicsCommandList> commandList,
                                     ID3D12CommandAllocator*           commandAllocator)
    : m_device_(device)
    , m_commandList_(std::move(commandList))
    , m_commandAllocator_(commandAllocator) {
}

CommandBufferDx12::~CommandBufferDx12() {
  if (m_device_ && m_commandAllocator_) {
    m_device_->returnCommandAllocator(m_commandAllocator_);
    m_commandAllocator_ = nullptr;
  }
}

void CommandBufferDx12::begin() {
  if (m_isRecording_) {
    LOG_WARN("Command buffer is already recording");
    return;
  }

  m_isRecording_ = true;
}

void CommandBufferDx12::end() {
  if (!m_isRecording_) {
    LOG_WARN("Command buffer is not recording");
    return;
  }

  if (m_isRenderPassActive_) {
    endRenderPass();
  }

  HRESULT hr = m_commandList_->Close();
  if (FAILED(hr)) {
    LOG_ERROR("Failed to close command list");
    return;
  }

  m_isRecording_ = false;
}

void CommandBufferDx12::reset() {
  if (m_isRecording_) {
    LOG_WARN("Cannot reset while recording");
    return;
  }

  HRESULT hr = m_commandAllocator_->Reset();
  if (FAILED(hr)) {
    LOG_ERROR("Failed to reset command allocator");
    return;
  }

  hr = m_commandList_->Reset(m_commandAllocator_, nullptr);
  if (FAILED(hr)) {
    LOG_ERROR("Failed to reset command list");
    return;
  }

  m_currentPipeline_    = nullptr;
  m_currentRenderPass_  = nullptr;
  m_currentFramebuffer_ = nullptr;
  m_isRenderPassActive_ = false;
}

void CommandBufferDx12::setPipeline(Pipeline* pipeline) {
  if (!m_isRecording_) {
    LOG_ERROR("Command buffer is not recording");
    return;
  }

  auto pipelineDx12 = dynamic_cast<GraphicsPipelineDx12*>(pipeline);
  if (!pipelineDx12) {
    LOG_ERROR("Invalid pipeline type");
    return;
  }

  m_commandList_->SetPipelineState(pipelineDx12->getPipelineState());  // PSO

  // root signature (graphics / compute)
  auto pipelineType = pipelineDx12->getType();
  if (pipelineType == PipelineType::Graphics) {
    m_commandList_->SetGraphicsRootSignature(pipelineDx12->getRootSignature());

    m_commandList_->OMSetBlendFactor(pipelineDx12->getBlendFactors().data());

    const auto& desc = pipelineDx12->getDesc();
    if (desc.depthStencil.stencilTestEnable) {
      uint32_t frontRef = desc.depthStencil.front.reference;
      uint32_t backRef  = desc.depthStencil.back.reference;

      if (frontRef != backRef) {
        LOG_WARN("DX12 limitation: Different front/back stencil references not supported. Using front reference.");
      }

      m_commandList_->OMSetStencilRef(frontRef);
    }

    D3D_PRIMITIVE_TOPOLOGY topology = g_getPrimitiveTopologyDx12(pipelineDx12->getPrimitiveType());
    m_commandList_->IASetPrimitiveTopology(topology);

  } else if (pipelineType == PipelineType::Compute) {
    m_commandList_->SetComputeRootSignature(pipelineDx12->getRootSignature());
  }

  m_currentPipeline_ = pipelineDx12;
}

void CommandBufferDx12::setViewport(const Viewport& viewport) {
  if (!m_isRecording_) {
    LOG_ERROR("Command buffer is not recording");
    return;
  }

  D3D12_VIEWPORT viewportDx12;
  viewportDx12.TopLeftX = viewport.x;
  viewportDx12.TopLeftY = viewport.y;
  viewportDx12.Width    = viewport.width;
  viewportDx12.Height   = viewport.height;
  viewportDx12.MinDepth = viewport.minDepth;
  viewportDx12.MaxDepth = viewport.maxDepth;

  m_commandList_->RSSetViewports(1, &viewportDx12);
}

void CommandBufferDx12::setScissor(const ScissorRect& scissor) {
  if (!m_isRecording_) {
    LOG_ERROR("Command buffer is not recording");
    return;
  }

  D3D12_RECT scissorDx12;
  scissorDx12.left   = scissor.x;
  scissorDx12.top    = scissor.y;
  scissorDx12.right  = scissor.x + scissor.width;
  scissorDx12.bottom = scissor.y + scissor.height;

  m_commandList_->RSSetScissorRects(1, &scissorDx12);
}

void CommandBufferDx12::bindVertexBuffer(uint32_t binding, Buffer* buffer, uint64_t offset) {
  if (!m_isRecording_) {
    LOG_ERROR("Command buffer is not recording");
    return;
  }

  DirectBufferDx12* directBuffer = dynamic_cast<DirectBufferDx12*>(buffer);
  if (!directBuffer) {
    LOG_ERROR("Buffer must be a DirectBufferDx12 type");
    return;
  }

  if (!directBuffer->isVertexBuffer() && !directBuffer->isInstanceBuffer()) {
    LOG_WARN("Buffer is not marked as vertex or instance buffer");
  }

  D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
  vertexBufferView.BufferLocation = directBuffer->getGPUVirtualAddress() + offset;
  vertexBufferView.SizeInBytes    = static_cast<UINT>(directBuffer->getSize() - offset);
  vertexBufferView.StrideInBytes  = directBuffer->getStride();

  m_commandList_->IASetVertexBuffers(binding, 1, &vertexBufferView);
}

void CommandBufferDx12::bindIndexBuffer(Buffer* buffer, uint64_t offset, bool use32BitIndices) {
  if (!m_isRecording_) {
    LOG_ERROR("Command buffer is not recording");
    return;
  }

  BufferDx12* bufferDx12 = dynamic_cast<BufferDx12*>(buffer);
  if (!bufferDx12) {
    LOG_ERROR("Invalid buffer type");
    return;
  }

  DirectBufferDx12* directBuffer = dynamic_cast<DirectBufferDx12*>(buffer);
  if (directBuffer && !directBuffer->isIndexBuffer()) {
    LOG_WARN("Buffer is not marked as index buffer");
  }

  D3D12_INDEX_BUFFER_VIEW indexBufferView;
  indexBufferView.BufferLocation = bufferDx12->getGPUVirtualAddress() + offset;
  indexBufferView.SizeInBytes    = static_cast<UINT>(bufferDx12->getSize() - offset);
  indexBufferView.Format         = use32BitIndices ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT;

  m_commandList_->IASetIndexBuffer(&indexBufferView);
}

void CommandBufferDx12::bindDescriptorSet(uint32_t rootParameterIndex, DescriptorSet* set) {
  if (!m_isRecording_) {
    LOG_ERROR("Command buffer is not recording");
    return;
  }

  if (!m_currentPipeline_) {
    LOG_ERROR("No active pipeline");
    return;
  }

  DescriptorSetDx12* descriptorSetDx12 = dynamic_cast<DescriptorSetDx12*>(set);
  if (!descriptorSetDx12) {
    LOG_ERROR("Invalid descriptor set type");
    return;
  }

  auto descriptorSetLayoutDx12 = dynamic_cast<const DescriptorSetLayoutDx12*>(descriptorSetDx12->getLayout());
  if (!descriptorSetLayoutDx12) {
    LOG_ERROR("Invalid descriptor set layout type");
    return;
  }

  // true - sampler, false - srv/cbv/uav
  auto isSampler = descriptorSetLayoutDx12->isSamplerLayout();

  auto                        frameResourcesManager = m_device_->getFrameResourcesManager();
  auto                        currentFrameIndex     = frameResourcesManager->getCurrentFrameIndex();
  D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle;
  if (isSampler) {
    gpuHandle = descriptorSetDx12->getGpuSamplerHandle(currentFrameIndex);
  } else {
    gpuHandle = descriptorSetDx12->getGpuSrvUavCbvHandle(currentFrameIndex);
  }
  m_commandList_->SetGraphicsRootDescriptorTable(rootParameterIndex, gpuHandle);

  switch (m_currentPipeline_->getType()) {
    case PipelineType::Graphics:
      m_commandList_->SetGraphicsRootDescriptorTable(rootParameterIndex, gpuHandle);
      break;
    case PipelineType::Compute:
      m_commandList_->SetComputeRootDescriptorTable(rootParameterIndex, gpuHandle);
      break;
  }
}

void CommandBufferDx12::draw(uint32_t vertexCount, uint32_t firstVertex) {
  if (!m_isRecording_ || !m_isRenderPassActive_) {
    LOG_ERROR("Command buffer is not recording or render pass is not active");
    return;
  }

  m_commandList_->DrawInstanced(vertexCount, 1, firstVertex, 0);
}

void CommandBufferDx12::drawIndexed(uint32_t indexCount, uint32_t firstIndex, int32_t vertexOffset) {
  if (!m_isRecording_ || !m_isRenderPassActive_) {
    LOG_ERROR("Command buffer is not recording or render pass is not active");
    return;
  }

  m_commandList_->DrawIndexedInstanced(indexCount, 1, firstIndex, vertexOffset, 0);
}

void CommandBufferDx12::drawInstanced(uint32_t vertexCount,
                                      uint32_t instanceCount,
                                      uint32_t firstVertex,
                                      uint32_t firstInstance) {
  if (!m_isRecording_ || !m_isRenderPassActive_) {
    LOG_ERROR("Command buffer is not recording or render pass is not active");
    return;
  }

  m_commandList_->DrawInstanced(vertexCount, instanceCount, firstVertex, firstInstance);
}

void CommandBufferDx12::drawIndexedInstanced(
    uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance) {
  if (!m_isRecording_ || !m_isRenderPassActive_) {
    LOG_ERROR("Command buffer is not recording or render pass is not active");
    return;
  }
  m_commandList_->DrawIndexedInstanced(indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
}

void CommandBufferDx12::resourceBarrier(const ResourceBarrierDesc& barrier) {
  if (!m_isRecording_) {
    LOG_ERROR("Command buffer is not recording");
    return;
  }

  if (!barrier.texture) {
    LOG_ERROR("Null texture");
    return;
  }

  TextureDx12* textureDx12 = dynamic_cast<TextureDx12*>(barrier.texture);
  if (!textureDx12) {
    LOG_ERROR("Invalid texture type");
    return;
  }

  auto oldState = g_getResourceLayoutDx12(barrier.oldLayout);
  auto newState = g_getResourceLayoutDx12(barrier.newLayout);

  if (oldState == newState) {
    // TODO: uncomment this if needed
    // LOG_WARN("Skipping redundant barrier, states are identical");
    return;
  }

  D3D12_RESOURCE_BARRIER barrierDx12 = {};
  barrierDx12.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  barrierDx12.Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
  barrierDx12.Transition.pResource   = textureDx12->getResource();
  barrierDx12.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
  barrierDx12.Transition.StateBefore = oldState;
  barrierDx12.Transition.StateAfter  = newState;

  m_commandList_->ResourceBarrier(1, &barrierDx12);

  textureDx12->updateCurrentState_(barrier.newLayout);
}

void CommandBufferDx12::beginRenderPass(RenderPass*                    renderPass,
                                        Framebuffer*                   framebuffer,
                                        const std::vector<ClearValue>& clearValues) {
  if (!m_isRecording_) {
    LOG_ERROR("Command buffer is not recording");
    return;
  }

  if (m_isRenderPassActive_) {
    LOG_ERROR("Render pass is already active");
    return;
  }

  RenderPassDx12*  renderPassDx12  = dynamic_cast<RenderPassDx12*>(renderPass);
  FramebufferDx12* framebufferDx12 = dynamic_cast<FramebufferDx12*>(framebuffer);

  if (!renderPassDx12 || !framebufferDx12) {
    LOG_ERROR("Invalid render pass or framebuffer type");
    return;
  }

  framebufferDx12->transitionToRenderTargetState(this);

  uint32_t numRtvs = framebufferDx12->getColorAttachmentCount();
  if (numRtvs > 0) {
    m_commandList_->OMSetRenderTargets(numRtvs,
                                       framebufferDx12->getRTVHandles(),
                                       FALSE,
                                       framebufferDx12->hasDSV() ? framebufferDx12->getDsvHandle() : nullptr);

    // Clear RTVs if requested (TODO: in separate method)
    for (uint32_t i = 0; i < numRtvs && i < clearValues.size(); i++) {
      if (renderPassDx12->shouldClearColor(i)) {
        m_commandList_->ClearRenderTargetView(framebufferDx12->getRTVHandles()[i], clearValues[i].color, 0, nullptr);
      }
    }
  }

  // Clear depth/stencil if needed (TODO: in separate method)
  if (framebufferDx12->hasDSV() && renderPassDx12->shouldClearDepthStencil()) {
    D3D12_CLEAR_FLAGS clearFlags = D3D12_CLEAR_FLAG_DEPTH;
    if (renderPassDx12->shouldClearStencil()) {
      clearFlags |= D3D12_CLEAR_FLAG_STENCIL;
    }

    float   depth   = 1.0f;
    uint8_t stencil = 0;

    if (!clearValues.empty()) {
      size_t dsIndex = numRtvs < clearValues.size() ? numRtvs : 0;
      if (dsIndex < clearValues.size()) {
        depth   = clearValues[dsIndex].depthStencil.depth;
        stencil = static_cast<uint8_t>(clearValues[dsIndex].depthStencil.stencil);
      }
    }

    m_commandList_->ClearDepthStencilView(*framebufferDx12->getDsvHandle(), clearFlags, depth, stencil, 0, nullptr);
  }

  m_currentRenderPass_  = renderPassDx12;
  m_currentFramebuffer_ = framebufferDx12;
  m_isRenderPassActive_ = true;
}

void CommandBufferDx12::endRenderPass() {
  if (!m_isRecording_ || !m_isRenderPassActive_) {
    LOG_ERROR("Command buffer is not recording or render pass is not active");
    return;
  }

  if (m_currentFramebuffer_ && m_currentRenderPass_) {
    m_currentFramebuffer_->transitionToResourceState(this, m_currentRenderPass_);
  }

  m_isRenderPassActive_ = false;
  m_currentRenderPass_  = nullptr;
  m_currentFramebuffer_ = nullptr;
}

void CommandBufferDx12::copyBuffer(
    Buffer* srcBuffer, Buffer* dstBuffer, uint64_t srcOffset, uint64_t dstOffset, uint64_t size) {
  if (!m_isRecording_) {
    LOG_ERROR("Command buffer is not recording");
    return;
  }

  BufferDx12* srcBufferDx12 = dynamic_cast<BufferDx12*>(srcBuffer);
  BufferDx12* dstBufferDx12 = dynamic_cast<BufferDx12*>(dstBuffer);

  if (!srcBufferDx12 || !dstBufferDx12) {
    LOG_ERROR("Invalid buffer type");
    return;
  }

  uint64_t copySize = size;
  if (copySize == 0) {
    // copy from offset to end of source buffer
    copySize = srcBufferDx12->getSize() - srcOffset;
  }

  if (dstOffset + copySize > dstBufferDx12->getSize()) {
    LOG_ERROR("Copy operation exceeds destination buffer size");
    return;
  }

  m_commandList_->CopyBufferRegion(
      dstBufferDx12->getResource(), dstOffset, srcBufferDx12->getResource(), srcOffset, copySize);
}

void CommandBufferDx12::copyBufferToTexture(Buffer*  srcBuffer,
                                            Texture* dstTexture,
                                            uint32_t mipLevel,
                                            uint32_t arrayLayer) {
  if (!m_isRecording_) {
    LOG_ERROR("Command buffer is not recording");
    return;
  }

  BufferDx12*  srcBufferDx12  = dynamic_cast<BufferDx12*>(srcBuffer);
  TextureDx12* dstTextureDx12 = dynamic_cast<TextureDx12*>(dstTexture);

  if (!srcBufferDx12 || !dstTextureDx12) {
    LOG_ERROR("Invalid buffer or texture type");
    return;
  }

  // transition to copy state
  D3D12_RESOURCE_BARRIER barrier = {};
  barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  barrier.Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
  barrier.Transition.pResource   = dstTextureDx12->getResource();
  barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
  barrier.Transition.StateBefore = dstTextureDx12->getResourceState();
  barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_COPY_DEST;

  m_commandList_->ResourceBarrier(1, &barrier);

  UINT subresourceIndex
      = D3D12CalcSubresource(mipLevel, arrayLayer, 0, dstTextureDx12->getMipLevels(), dstTextureDx12->getArraySize());

  D3D12_TEXTURE_COPY_LOCATION dstLocation = {};
  dstLocation.pResource                   = dstTextureDx12->getResource();
  dstLocation.Type                        = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
  dstLocation.SubresourceIndex            = subresourceIndex;

  D3D12_TEXTURE_COPY_LOCATION srcLocation = {};
  srcLocation.pResource                   = srcBufferDx12->getResource();
  srcLocation.Type                        = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;

  D3D12_RESOURCE_DESC textureFootprintInfo = dstTextureDx12->getResource()->GetDesc();

  UINT   numRows;
  UINT64 rowSizeInBytes, totalBytes;

  m_device_->getDevice()->GetCopyableFootprints(&textureFootprintInfo,
                                                subresourceIndex,
                                                1,  // Number of subresources to copy
                                                0,  // Buffer offset
                                                &srcLocation.PlacedFootprint,
                                                &numRows,
                                                &rowSizeInBytes,
                                                &totalBytes);

  m_commandList_->CopyTextureRegion(&dstLocation, 0, 0, 0, &srcLocation, nullptr);

  barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
  barrier.Transition.StateAfter  = dstTextureDx12->getResourceState();
  m_commandList_->ResourceBarrier(1, &barrier);
}

void CommandBufferDx12::copyTextureToBuffer(Texture* srcTexture,
                                            Buffer*  dstBuffer,
                                            uint32_t mipLevel,
                                            uint32_t arrayLayer) {
  if (!m_isRecording_) {
    LOG_ERROR("Command buffer is not recording");
    return;
  }

  TextureDx12* srcTextureDx12 = dynamic_cast<TextureDx12*>(srcTexture);
  BufferDx12*  dstBufferDx12  = dynamic_cast<BufferDx12*>(dstBuffer);

  if (!srcTextureDx12 || !dstBufferDx12) {
    LOG_ERROR("Invalid texture or buffer type");
    return;
  }

  // transition to copy state
  D3D12_RESOURCE_BARRIER barrier = {};
  barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  barrier.Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
  barrier.Transition.pResource   = srcTextureDx12->getResource();
  barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
  barrier.Transition.StateBefore = srcTextureDx12->getResourceState();
  barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_COPY_SOURCE;

  m_commandList_->ResourceBarrier(1, &barrier);

  UINT subresourceIndex
      = D3D12CalcSubresource(mipLevel, arrayLayer, 0, srcTextureDx12->getMipLevels(), srcTextureDx12->getArraySize());

  D3D12_TEXTURE_COPY_LOCATION srcLocation = {};
  srcLocation.pResource                   = srcTextureDx12->getResource();
  srcLocation.Type                        = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
  srcLocation.SubresourceIndex            = subresourceIndex;

  D3D12_TEXTURE_COPY_LOCATION dstLocation = {};
  dstLocation.pResource                   = dstBufferDx12->getResource();
  dstLocation.Type                        = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;

  D3D12_RESOURCE_DESC textureFootprintInfo = srcTextureDx12->getResource()->GetDesc();

  UINT   numRows;
  UINT64 rowSizeInBytes, totalBytes;

  m_device_->getDevice()->GetCopyableFootprints(&textureFootprintInfo,
                                                subresourceIndex,
                                                1,  // Number of subresources to copy
                                                0,  // Buffer offset
                                                &dstLocation.PlacedFootprint,
                                                &numRows,
                                                &rowSizeInBytes,
                                                &totalBytes);

  m_commandList_->CopyTextureRegion(&dstLocation, 0, 0, 0, &srcLocation, nullptr);

  barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
  barrier.Transition.StateAfter  = srcTextureDx12->getResourceState();
  m_commandList_->ResourceBarrier(1, &barrier);
}

void CommandBufferDx12::copyTexture(Texture* srcTexture,
                                    Texture* dstTexture,
                                    uint32_t srcMipLevel,
                                    uint32_t srcArrayLayer,
                                    uint32_t dstMipLevel,
                                    uint32_t dstArrayLayer) {
  if (!m_isRecording_) {
    LOG_ERROR("Command buffer is not recording");
    return;
  }

  auto* srcTexDx12 = dynamic_cast<TextureDx12*>(srcTexture);
  auto* dstTexDx12 = dynamic_cast<TextureDx12*>(dstTexture);
  if (!srcTexDx12 || !dstTexDx12) {
    LOG_ERROR("Invalid texture type");
    return;
  }

  ResourceLayout        srcInitialStateAbstract = srcTexDx12->getCurrentLayoutType();
  D3D12_RESOURCE_STATES srcInitialState         = srcTexDx12->getResourceState();
  ResourceLayout        dstInitialStateAbstract = dstTexDx12->getCurrentLayoutType();
  D3D12_RESOURCE_STATES dstInitialState         = dstTexDx12->getResourceState();

  // SOURCE -> COPY_SRC
  if (srcInitialState != D3D12_RESOURCE_STATE_COPY_SOURCE) {
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.pResource   = srcTexDx12->getResource();
    barrier.Transition.StateBefore = srcInitialState;
    barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_COPY_SOURCE;
    m_commandList_->ResourceBarrier(1, &barrier);

    srcTexDx12->updateCurrentState_(ResourceLayout::TransferSrc);
  }

  // DEST -> COPY_DST
  if (dstInitialState != D3D12_RESOURCE_STATE_COPY_DEST) {
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.pResource   = dstTexDx12->getResource();
    barrier.Transition.StateBefore = dstInitialState;
    barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_COPY_DEST;
    m_commandList_->ResourceBarrier(1, &barrier);

    dstTexDx12->updateCurrentState_(ResourceLayout::TransferDst);
  }

  // actual copy
  const UINT srcSub
      = D3D12CalcSubresource(srcMipLevel, srcArrayLayer, 0, srcTexDx12->getMipLevels(), srcTexDx12->getArraySize());
  const UINT dstSub
      = D3D12CalcSubresource(dstMipLevel, dstArrayLayer, 0, dstTexDx12->getMipLevels(), dstTexDx12->getArraySize());

  D3D12_TEXTURE_COPY_LOCATION srcLoc{};
  srcLoc.pResource        = srcTexDx12->getResource();
  srcLoc.Type             = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
  srcLoc.SubresourceIndex = srcSub;

  D3D12_TEXTURE_COPY_LOCATION dstLoc{};
  dstLoc.pResource        = dstTexDx12->getResource();
  dstLoc.Type             = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
  dstLoc.SubresourceIndex = dstSub;

  m_commandList_->CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, nullptr);

  if (srcInitialState != D3D12_RESOURCE_STATE_COPY_SOURCE) {
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.pResource   = srcTexDx12->getResource();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
    barrier.Transition.StateAfter  = srcInitialState;
    m_commandList_->ResourceBarrier(1, &barrier);

    srcTexDx12->updateCurrentState_(srcInitialStateAbstract);
  }

  if (dstInitialState != D3D12_RESOURCE_STATE_COPY_DEST) {
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.pResource   = dstTexDx12->getResource();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.StateAfter  = dstInitialState;
    m_commandList_->ResourceBarrier(1, &barrier);

    dstTexDx12->updateCurrentState_(dstInitialStateAbstract);
  }
}

void CommandBufferDx12::clearColor(Texture* texture, const float color[4], uint32_t mipLevel, uint32_t arrayLayer) {
  if (!m_isRecording_) {
    LOG_ERROR("Command buffer is not recording");
    return;
  }

  TextureDx12* textureDx12 = dynamic_cast<TextureDx12*>(texture);
  if (!textureDx12) {
    LOG_ERROR("Invalid texture type");
    return;
  }

  // Get RTV handle for this mip/array slice
  D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = textureDx12->getRtvHandle(mipLevel, arrayLayer);
  if (rtvHandle.ptr == 0) {
    LOG_ERROR("No valid RTV handle for texture");
    return;
  }

  // Transition to render target state
  D3D12_RESOURCE_BARRIER barrier = {};
  barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  barrier.Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
  barrier.Transition.pResource   = textureDx12->getResource();
  barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
  barrier.Transition.StateBefore = textureDx12->getResourceState();
  barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_RENDER_TARGET;

  m_commandList_->ResourceBarrier(1, &barrier);

  m_commandList_->ClearRenderTargetView(rtvHandle, color, 0, nullptr);

  barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
  barrier.Transition.StateAfter  = textureDx12->getResourceState();
  m_commandList_->ResourceBarrier(1, &barrier);
}

void CommandBufferDx12::clearDepthStencil(
    Texture* texture, float depth, uint8_t stencil, uint32_t mipLevel, uint32_t arrayLayer) {
  if (!m_isRecording_) {
    LOG_ERROR("Command buffer is not recording");
    return;
  }

  TextureDx12* textureDx12 = dynamic_cast<TextureDx12*>(texture);
  if (!textureDx12) {
    LOG_ERROR("Invalid texture type");
    return;
  }

  D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = textureDx12->getDsvHandle(mipLevel, arrayLayer);
  if (dsvHandle.ptr == 0) {
    LOG_ERROR("No valid DSV handle for texture");
    return;
  }

  bool hasStencil = g_isDepthFormat(textureDx12->getFormat()) && !g_isDepthOnlyFormat(textureDx12->getFormat());

  D3D12_CLEAR_FLAGS clearFlags = D3D12_CLEAR_FLAG_DEPTH;
  if (hasStencil) {
    clearFlags |= D3D12_CLEAR_FLAG_STENCIL;
  }

  // Transition texture to depth write state
  D3D12_RESOURCE_BARRIER barrier = {};
  barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  barrier.Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
  barrier.Transition.pResource   = textureDx12->getResource();
  barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
  barrier.Transition.StateBefore = textureDx12->getResourceState();
  barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_DEPTH_WRITE;

  m_commandList_->ResourceBarrier(1, &barrier);

  m_commandList_->ClearDepthStencilView(dsvHandle, clearFlags, depth, stencil, 0, nullptr);

  barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_DEPTH_WRITE;
  barrier.Transition.StateAfter  = textureDx12->getResourceState();
  m_commandList_->ResourceBarrier(1, &barrier);
}

void CommandBufferDx12::beginDebugMarker(const std::string& name, const float color[4]) {
  if (name.empty()) {
    return;
  }

#if defined(USE_PIX)
  UINT64 pixColor = PIX_COLOR_DEFAULT;
  if (color) {
    pixColor = PIX_COLOR(static_cast<BYTE>(std::clamp(color[0], 0.f, 1.f) * 255),
                         static_cast<BYTE>(std::clamp(color[1], 0.f, 1.f) * 255),
                         static_cast<BYTE>(std::clamp(color[2], 0.f, 1.f) * 255));
  }
  PIXBeginEvent(m_commandList_.Get(), pixColor, "%s", name.c_str());
#else
  m_commandList_->BeginEvent(1, name.c_str(), static_cast<UINT>(name.length()));
#endif
}

void CommandBufferDx12::endDebugMarker() {
#if defined(USE_PIX)
  PIXEndEvent(m_commandList_.Get());
#else
  m_commandList_->EndEvent();
#endif
}

void CommandBufferDx12::insertDebugMarker(const std::string& name, const float color[4]) {
  if (name.empty()) {
    return;
  }

#if defined(USE_PIX)
  UINT64 pixColor = PIX_COLOR_DEFAULT;
  if (color) {
    pixColor = PIX_COLOR(static_cast<BYTE>(std::clamp(color[0], 0.f, 1.f) * 255),
                         static_cast<BYTE>(std::clamp(color[1], 0.f, 1.f) * 255),
                         static_cast<BYTE>(std::clamp(color[2], 0.f, 1.f) * 255));
  }
  PIXSetMarker(m_commandList_.Get(), pixColor, "%s", name.c_str());
#else
  m_commandList_->SetMarker(1, name.c_str(), static_cast<UINT>(name.length()));
#endif
}

void CommandBufferDx12::bindDescriptorHeaps() {
  ID3D12DescriptorHeap* heaps[2];
  uint32_t              heapCount = 0;

  auto frameResourcesManager = m_device_->getFrameResourcesManager();
  if (!frameResourcesManager) {
    return;
  }

  auto cbvSrvUavHeap = frameResourcesManager->getCurrentCbvSrvUavHeap()->getHeap();
  if (cbvSrvUavHeap) {
    heaps[heapCount++] = cbvSrvUavHeap;
  }

  auto samplerHeap = frameResourcesManager->getCurrentSamplerHeap()->getHeap();
  if (samplerHeap) {
    heaps[heapCount++] = samplerHeap;
  }

  if (heapCount > 0) {
    m_commandList_->SetDescriptorHeaps(heapCount, heaps);
  }
}

//-------------------------------------------------------------------------
// CommandAllocatorManager implementation
//-------------------------------------------------------------------------

CommandAllocatorManager::~CommandAllocatorManager() {
  release();
}

bool CommandAllocatorManager::initialize(ID3D12Device* device, uint32_t allocatorCount) {
  if (!device) {
    LOG_ERROR("Invalid device");
    return false;
  }

  release();

  m_device = device;

  // Create the initial pool of command allocators
  for (uint32_t i = 0; i < allocatorCount; ++i) {
    ComPtr<ID3D12CommandAllocator> allocator;
    HRESULT hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator));

    if (FAILED(hr)) {
      LOG_ERROR("Failed to create command allocator");
      return false;
    }

    m_availableAllocators.push_back(allocator);
  }

  return true;
}

void CommandAllocatorManager::release() {
  m_availableAllocators.clear();
  m_usedAllocators.clear();
  m_device = nullptr;
}

ID3D12CommandAllocator* CommandAllocatorManager::getCommandAllocator() {
  if (!m_device) {
    LOG_ERROR("Manager not initialized");
    return nullptr;
  }

  // Try to reuse an existing allocator
  if (!m_availableAllocators.empty()) {
    ComPtr<ID3D12CommandAllocator> allocator = m_availableAllocators.back();
    m_availableAllocators.pop_back();

    allocator->Reset();

    m_usedAllocators.push_back(allocator);

    return allocator.Get();
  }

  ComPtr<ID3D12CommandAllocator> allocator;
  HRESULT hr = m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator));

  if (FAILED(hr)) {
    LOG_ERROR("Failed to create command allocator");
    return nullptr;
  }

  m_usedAllocators.push_back(allocator);

  return allocator.Get();
}

void CommandAllocatorManager::returnCommandAllocator(ID3D12CommandAllocator* allocator) {
  if (!allocator) {
    return;
  }

  for (auto it = m_usedAllocators.begin(); it != m_usedAllocators.end(); ++it) {
    if (it->Get() == allocator) {
      m_availableAllocators.push_back(*it);
      m_usedAllocators.erase(it);
      return;
    }
  }

  LOG_WARN("Allocator not found in used list");
}

}  // namespace rhi
}  // namespace gfx
}  // namespace arise

#endif  // ARISE_USE_DX12