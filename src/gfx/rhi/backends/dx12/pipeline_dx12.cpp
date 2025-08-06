#include "gfx/rhi/backends/dx12/pipeline_dx12.h"

#ifdef ARISE_USE_DX12

#include "gfx/rhi/backends/dx12/descriptor_dx12.h"
#include "gfx/rhi/backends/dx12/device_dx12.h"
#include "gfx/rhi/backends/dx12/render_pass_dx12.h"
#include "gfx/rhi/backends/dx12/rhi_enums_dx12.h"
#include "gfx/rhi/backends/dx12/shader_dx12.h"
#include "utils/logger/log.h"

#include <d3dcompiler.h>

namespace arise {
namespace gfx {
namespace rhi {

GraphicsPipelineDx12::GraphicsPipelineDx12(const GraphicsPipelineDesc& desc, DeviceDx12* device)
    : GraphicsPipeline(desc)
    , m_device_(device) {
  if (!initialize_()) {
    LOG_ERROR("Failed to initialize DirectX 12 graphics pipeline");
  }
}

bool GraphicsPipelineDx12::rebuild() {
  m_pipelineState_.Reset();
  if (createPipelineState_()) {
    LOG_INFO("Successfully rebuilt DirectX 12 graphics pipeline");
    m_updateFrame = -1;
    return true;
  } else {
    LOG_ERROR("Failed to rebuild DirectX 12 graphics pipeline");
    return false;
  }
}

bool GraphicsPipelineDx12::initialize_() {
  if (!createRootSignature_()) {
    LOG_ERROR("Failed to create root signature for DX12 pipeline");
    return false;
  }

  if (!createPipelineState_()) {
    LOG_ERROR("Failed to create pipeline state object for DX12 pipeline");
    return false;
  }

  return true;
}

bool GraphicsPipelineDx12::createRootSignature_() {
  // Create root signature based on descriptor set layouts
  // Each descriptor set layout becomes a descriptor table in a specific space

  // Create root parameters - one for each descriptor set layout
  std::vector<D3D12_ROOT_PARAMETER> rootParameters;

  std::vector<std::vector<D3D12_DESCRIPTOR_RANGE>> allRanges;
  allRanges.reserve(m_desc_.setLayouts.size());

  for (size_t i = 0; i < m_desc_.setLayouts.size(); ++i) {
    const auto* layoutBase = m_desc_.setLayouts[i];
    if (!layoutBase) {
      LOG_ERROR("Null descriptor set layout provided");
      continue;
    }

    const auto* layout = dynamic_cast<const DescriptorSetLayoutDx12*>(layoutBase);
    if (!layout) {
      LOG_ERROR("Invalid descriptor set layout type for DX12 pipeline");
      return false;
    }

    const auto& originalRanges = layout->getDescriptorRanges();
    if (originalRanges.empty()) {
      continue;
    }

    std::vector<D3D12_DESCRIPTOR_RANGE> newRanges;
    newRanges.reserve(originalRanges.size());

    for (const auto& originalRange : originalRanges) {
      D3D12_DESCRIPTOR_RANGE newRange = originalRange;
      newRange.RegisterSpace          = static_cast<UINT>(i);
      newRanges.push_back(newRange);
    }

    allRanges.push_back(std::move(newRanges));

    D3D12_ROOT_PARAMETER rootParam = {};
    rootParam.ParameterType        = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParam.ShaderVisibility     = determineShaderVisibility_(layout->getDesc().bindings);

    // Set descriptor table parameters with the new ranges
    rootParam.DescriptorTable.NumDescriptorRanges = static_cast<UINT>(allRanges.back().size());
    rootParam.DescriptorTable.pDescriptorRanges   = allRanges.back().data();

    rootParameters.push_back(rootParam);
  }

  // TODO: add static samplers (if needed)
  std::vector<D3D12_STATIC_SAMPLER_DESC> staticSamplers;

  D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
  rootSignatureDesc.NumParameters             = static_cast<UINT>(rootParameters.size());
  rootSignatureDesc.pParameters               = rootParameters.empty() ? nullptr : rootParameters.data();
  rootSignatureDesc.NumStaticSamplers         = static_cast<UINT>(staticSamplers.size());
  rootSignatureDesc.pStaticSamplers           = staticSamplers.empty() ? nullptr : staticSamplers.data();
  rootSignatureDesc.Flags                     = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

  ID3DBlob* signature = nullptr;
  ID3DBlob* error     = nullptr;
  HRESULT   hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error);

  if (FAILED(hr)) {
    std::string errorMsg = "Failed to serialize root signature: ";
    if (error) {
      errorMsg += std::string(static_cast<const char*>(error->GetBufferPointer()), error->GetBufferSize());
      error->Release();
    }
    LOG_ERROR(errorMsg);

    if (signature) {
      signature->Release();
    }

    return false;
  }

  hr = m_device_->getDevice()->CreateRootSignature(0,  // Node mask (single GPU)
                                                   signature->GetBufferPointer(),
                                                   signature->GetBufferSize(),
                                                   IID_PPV_ARGS(&m_rootSignature_));

  signature->Release();
  if (error) {
    error->Release();
  }

  if (FAILED(hr)) {
    LOG_ERROR("Failed to create root signature");
    return false;
  }

  return true;
}

bool GraphicsPipelineDx12::createPipelineState_() {
  ComPtr<ID3DBlob> vertexShader;
  ComPtr<ID3DBlob> pixelShader;
  ComPtr<ID3DBlob> domainShader;
  ComPtr<ID3DBlob> hullShader;
  ComPtr<ID3DBlob> geometryShader;

  if (!collectShaders_(vertexShader, pixelShader, domainShader, hullShader, geometryShader)) {
    LOG_ERROR("Failed to collect shaders for pipeline");
    return false;
  }

  std::vector<D3D12_INPUT_ELEMENT_DESC> inputLayout;

  for (const auto& attribute : m_desc_.vertexAttributes) {
    D3D12_INPUT_ELEMENT_DESC element = {};

    element.SemanticName      = attribute.semanticName.c_str();
    element.SemanticIndex     = attribute.location;
    element.Format            = g_getTextureFormatDx12(attribute.format);
    element.InputSlot         = attribute.binding;
    element.AlignedByteOffset = attribute.offset;

    for (const auto& binding : m_desc_.vertexBindings) {
      if (binding.binding == attribute.binding) {
        element.InputSlotClass       = g_getVertexInputRateDx12(binding.inputRate);
        element.InstanceDataStepRate = (binding.inputRate == VertexInputRate::Instance) ? 1 : 0;
        break;
      }
    }

    inputLayout.push_back(element);
  }

  D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};

  psoDesc.pRootSignature = m_rootSignature_.Get();

  if (vertexShader) {
    psoDesc.VS.pShaderBytecode = vertexShader->GetBufferPointer();
    psoDesc.VS.BytecodeLength  = vertexShader->GetBufferSize();
  }

  if (pixelShader) {
    psoDesc.PS.pShaderBytecode = pixelShader->GetBufferPointer();
    psoDesc.PS.BytecodeLength  = pixelShader->GetBufferSize();
  }

  if (domainShader) {
    psoDesc.DS.pShaderBytecode = domainShader->GetBufferPointer();
    psoDesc.DS.BytecodeLength  = domainShader->GetBufferSize();
  }

  if (hullShader) {
    psoDesc.HS.pShaderBytecode = hullShader->GetBufferPointer();
    psoDesc.HS.BytecodeLength  = hullShader->GetBufferSize();
  }

  if (geometryShader) {
    psoDesc.GS.pShaderBytecode = geometryShader->GetBufferPointer();
    psoDesc.GS.BytecodeLength  = geometryShader->GetBufferSize();
  }

  // stream output (not used) - for capturing vertex/geometry shader output into a GPU buffer
  psoDesc.StreamOutput.pSODeclaration   = nullptr;
  psoDesc.StreamOutput.NumEntries       = 0;
  psoDesc.StreamOutput.pBufferStrides   = nullptr;
  psoDesc.StreamOutput.NumStrides       = 0;
  psoDesc.StreamOutput.RasterizedStream = 0;

  // blend state
  psoDesc.BlendState.AlphaToCoverageEnable  = m_desc_.multisample.alphaToCoverageEnable;
  psoDesc.BlendState.IndependentBlendEnable = TRUE;  // Allow different blend states for each render target

  // blend state for each render target
  for (uint32_t i = 0; i < 8; i++) {
    if (i < m_desc_.colorBlend.attachments.size()) {
      const auto& attachment = m_desc_.colorBlend.attachments[i];

      psoDesc.BlendState.RenderTarget[i].BlendEnable   = attachment.blendEnable;
      psoDesc.BlendState.RenderTarget[i].LogicOpEnable = m_desc_.colorBlend.logicOpEnable;

      // blend operations
      psoDesc.BlendState.RenderTarget[i].SrcBlend  = g_getBlendFactorDx12(attachment.srcColorBlendFactor);
      psoDesc.BlendState.RenderTarget[i].DestBlend = g_getBlendFactorDx12(attachment.dstColorBlendFactor);
      psoDesc.BlendState.RenderTarget[i].BlendOp   = g_getBlendOpDx12(attachment.colorBlendOp);

      psoDesc.BlendState.RenderTarget[i].SrcBlendAlpha  = g_getBlendFactorDx12(attachment.srcAlphaBlendFactor);
      psoDesc.BlendState.RenderTarget[i].DestBlendAlpha = g_getBlendFactorDx12(attachment.dstAlphaBlendFactor);
      psoDesc.BlendState.RenderTarget[i].BlendOpAlpha   = g_getBlendOpDx12(attachment.alphaBlendOp);

      // logic operation
      psoDesc.BlendState.RenderTarget[i].LogicOp = g_getLogicOpDx12(m_desc_.colorBlend.logicOp);

      // write mask
      psoDesc.BlendState.RenderTarget[i].RenderTargetWriteMask = g_getColorMaskDx12(attachment.colorWriteMask);
    } else {
      psoDesc.BlendState.RenderTarget[i].BlendEnable           = FALSE;
      psoDesc.BlendState.RenderTarget[i].LogicOpEnable         = FALSE;
      psoDesc.BlendState.RenderTarget[i].SrcBlend              = D3D12_BLEND_ONE;
      psoDesc.BlendState.RenderTarget[i].DestBlend             = D3D12_BLEND_ZERO;
      psoDesc.BlendState.RenderTarget[i].BlendOp               = D3D12_BLEND_OP_ADD;
      psoDesc.BlendState.RenderTarget[i].SrcBlendAlpha         = D3D12_BLEND_ONE;
      psoDesc.BlendState.RenderTarget[i].DestBlendAlpha        = D3D12_BLEND_ZERO;
      psoDesc.BlendState.RenderTarget[i].BlendOpAlpha          = D3D12_BLEND_OP_ADD;
      psoDesc.BlendState.RenderTarget[i].LogicOp               = D3D12_LOGIC_OP_NOOP;
      psoDesc.BlendState.RenderTarget[i].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    }
  }

  for (int i = 0; i < 4; i++) {
    m_blendFactors_[i] = m_desc_.colorBlend.blendConstants[i];
  }

  psoDesc.SampleMask = m_desc_.multisample.sampleMask;

  psoDesc.RasterizerState.FillMode              = g_getPolygonModeDx12(m_desc_.rasterization.polygonMode);
  psoDesc.RasterizerState.CullMode              = g_getCullModeDx12(m_desc_.rasterization.cullMode);
  psoDesc.RasterizerState.FrontCounterClockwise = (m_desc_.rasterization.frontFace == FrontFace::Ccw) ? TRUE : FALSE;
  psoDesc.RasterizerState.DepthBias             = static_cast<INT>(m_desc_.rasterization.depthBiasConstantFactor);
  psoDesc.RasterizerState.DepthBiasClamp        = m_desc_.rasterization.depthBiasClamp;
  psoDesc.RasterizerState.SlopeScaledDepthBias  = m_desc_.rasterization.depthBiasSlopeFactor;
  psoDesc.RasterizerState.DepthClipEnable = m_desc_.rasterization.depthClampEnable ? FALSE : TRUE;  // Inverted logic
  psoDesc.RasterizerState.MultisampleEnable
      = (m_desc_.multisample.rasterizationSamples != MSAASamples::Count1) ? TRUE : FALSE;
  psoDesc.RasterizerState.AntialiasedLineEnable = FALSE;  // Not used in this example
  psoDesc.RasterizerState.ForcedSampleCount     = 0;      // Use the sample count from the render target
  psoDesc.RasterizerState.ConservativeRaster    = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

  psoDesc.DepthStencilState.DepthEnable = m_desc_.depthStencil.depthTestEnable;
  psoDesc.DepthStencilState.DepthWriteMask
      = m_desc_.depthStencil.depthWriteEnable ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
  psoDesc.DepthStencilState.DepthFunc        = g_getCompareOpDx12(m_desc_.depthStencil.depthCompareOp);
  psoDesc.DepthStencilState.StencilEnable    = m_desc_.depthStencil.stencilTestEnable;
  psoDesc.DepthStencilState.StencilReadMask  = static_cast<UINT8>(m_desc_.depthStencil.front.compareMask);
  psoDesc.DepthStencilState.StencilWriteMask = static_cast<UINT8>(m_desc_.depthStencil.front.writeMask);

  psoDesc.DepthStencilState.FrontFace.StencilFailOp      = g_getStencilOpDx12(m_desc_.depthStencil.front.failOp);
  psoDesc.DepthStencilState.FrontFace.StencilDepthFailOp = g_getStencilOpDx12(m_desc_.depthStencil.front.depthFailOp);
  psoDesc.DepthStencilState.FrontFace.StencilPassOp      = g_getStencilOpDx12(m_desc_.depthStencil.front.passOp);
  psoDesc.DepthStencilState.FrontFace.StencilFunc        = g_getCompareOpDx12(m_desc_.depthStencil.front.compareOp);

  psoDesc.DepthStencilState.BackFace.StencilFailOp      = g_getStencilOpDx12(m_desc_.depthStencil.back.failOp);
  psoDesc.DepthStencilState.BackFace.StencilDepthFailOp = g_getStencilOpDx12(m_desc_.depthStencil.back.depthFailOp);
  psoDesc.DepthStencilState.BackFace.StencilPassOp      = g_getStencilOpDx12(m_desc_.depthStencil.back.passOp);
  psoDesc.DepthStencilState.BackFace.StencilFunc        = g_getCompareOpDx12(m_desc_.depthStencil.back.compareOp);

  psoDesc.InputLayout.pInputElementDescs = inputLayout.data();
  psoDesc.InputLayout.NumElements        = static_cast<UINT>(inputLayout.size());

  psoDesc.PrimitiveTopologyType = g_getPrimitiveTopologyTypeOnlyDx12(m_desc_.inputAssembly.topology);

  RenderPassDx12* renderPassDx12 = dynamic_cast<RenderPassDx12*>(m_desc_.renderPass);
  if (!renderPassDx12) {
    LOG_ERROR("Invalid render pass for DX12 pipeline");
    return false;
  }

  const auto& renderTargetFormats = renderPassDx12->getColorFormats();

  psoDesc.NumRenderTargets = static_cast<UINT>(renderTargetFormats.size());
  for (UINT i = 0; i < psoDesc.NumRenderTargets; i++) {
    psoDesc.RTVFormats[i] = renderTargetFormats[i];
  }

  psoDesc.DSVFormat = renderPassDx12->getDepthStencilFormat();

  psoDesc.SampleDesc.Count   = g_getMSAASampleCount(m_desc_.multisample.rasterizationSamples);
  psoDesc.SampleDesc.Quality = 0;

  psoDesc.NodeMask                        = 0;
  psoDesc.CachedPSO.pCachedBlob           = nullptr;
  psoDesc.CachedPSO.CachedBlobSizeInBytes = 0;

  psoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

  HRESULT hr = m_device_->getDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState_));

  if (FAILED(hr)) {
    LOG_ERROR("Failed to create graphics pipeline state object");
    return false;
  }

  return true;
}

bool GraphicsPipelineDx12::collectShaders_(ComPtr<ID3DBlob>& vertexShader,
                                           ComPtr<ID3DBlob>& pixelShader,
                                           ComPtr<ID3DBlob>& domainShader,
                                           ComPtr<ID3DBlob>& hullShader,
                                           ComPtr<ID3DBlob>& geometryShader) {
  for (Shader* shader : m_desc_.shaders) {
    if (!shader) {
      LOG_ERROR("Null shader pointer in pipeline description");
      return false;
    }

    ShaderDx12* shaderDx12 = dynamic_cast<ShaderDx12*>(shader);
    if (!shaderDx12) {
      LOG_ERROR("Invalid shader type for DX12 pipeline");
      return false;
    }

    switch (shaderDx12->getStage()) {
      case ShaderStageFlag::Vertex:
        vertexShader = shaderDx12->getShaderBlob();
        break;
      case ShaderStageFlag::Fragment:
        pixelShader = shaderDx12->getShaderBlob();
        break;
      case ShaderStageFlag::TessellationEvaluation:
        domainShader = shaderDx12->getShaderBlob();
        break;
      case ShaderStageFlag::TessellationControl:
        hullShader = shaderDx12->getShaderBlob();
        break;
      case ShaderStageFlag::Geometry:
        geometryShader = shaderDx12->getShaderBlob();
        break;
      default:
        LOG_WARN("Unsupported shader stage for DX12 pipeline");
        break;
    }
  }

  return true;
}

D3D12_SHADER_VISIBILITY GraphicsPipelineDx12::determineShaderVisibility_(
    const std::vector<DescriptorSetLayoutBindingDesc>& bindings) {
  if (bindings.empty()) {
    return D3D12_SHADER_VISIBILITY_ALL;
  }

  ShaderStageFlag commonFlag = bindings[0].stageFlags;
  for (size_t i = 1; i < bindings.size(); i++) {
    if (bindings[i].stageFlags != commonFlag) {
      return D3D12_SHADER_VISIBILITY_ALL;
    }
  }

  return g_getShaderVisibilityDx12(commonFlag);
}

}  // namespace rhi
}  // namespace gfx
}  // namespace arise

#endif  // ARISE_USE_DX12