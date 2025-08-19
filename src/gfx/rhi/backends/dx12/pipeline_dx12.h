#ifndef ARISE_PIPELINE_DX12_H
#define ARISE_PIPELINE_DX12_H

#include "gfx/rhi/interface/pipeline.h"
#include "platform/windows/windows_platform_setup.h"

#ifdef ARISE_USE_DX12

#include <array>
#include <memory>
#include <unordered_map>
#include <vector>

namespace arise {
namespace gfx {
namespace rhi {

class DeviceDx12;
class ShaderDx12;
class RenderPassDx12;
class DescriptorSetLayoutDx12;

class GraphicsPipelineDx12 : public GraphicsPipeline {
  public:
  GraphicsPipelineDx12(const GraphicsPipelineDesc& desc,
                       const PipelineLayoutDesc&   reflectionLayout,
                       DeviceDx12*                 device);
  ~GraphicsPipelineDx12() = default;

  GraphicsPipelineDx12(const GraphicsPipelineDx12&)            = delete;
  GraphicsPipelineDx12& operator=(const GraphicsPipelineDx12&) = delete;

  bool rebuild() override;

  const std::array<float, 4>& getBlendFactors() const { return m_blendFactors_; }

  // DirectX 12-specific methods
  ID3D12PipelineState* getPipelineState() const { return m_pipelineState_.Get(); }

  ID3D12RootSignature* getRootSignature() const { return m_rootSignature_.Get(); }

  private:
  bool initialize_();

  bool createRootSignature_();
  bool createPipelineState_();

  bool collectShaders_(ComPtr<ID3DBlob>& vertexShader,
                       ComPtr<ID3DBlob>& pixelShader,
                       ComPtr<ID3DBlob>& domainShader,
                       ComPtr<ID3DBlob>& hullShader,
                       ComPtr<ID3DBlob>& geometryShader);

  D3D12_SHADER_VISIBILITY determineShaderVisibility_(const std::vector<DescriptorSetLayoutBindingDesc>& bindings);

  DeviceDx12* m_device_;

  ComPtr<ID3D12PipelineState> m_pipelineState_;
  ComPtr<ID3D12RootSignature> m_rootSignature_;

  // Store blend factors separately since D3D12 doesn't include them in the blend state
  std::array<float, 4> m_blendFactors_;

  std::vector<std::unique_ptr<DescriptorSetLayout>> m_ownedLayouts_;  // to avoid dangling pointers
};

}  // namespace rhi
}  // namespace gfx
}  // namespace arise

#endif  // ARISE_USE_DX12
#endif  // ARISE_PIPELINE_DX12_H