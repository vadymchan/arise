#ifndef ARISE_PIPELINE_VK_H
#define ARISE_PIPELINE_VK_H

#include "gfx/rhi/interface/pipeline.h"

#include <vulkan/vulkan.h>

#include <memory>
#include <vector>

namespace arise {
namespace gfx {
namespace rhi {

class DeviceVk;
class ShaderVk;
class DescriptorSetLayoutVk;
class RenderPassVk;

class GraphicsPipelineVk : public GraphicsPipeline {
  public:
  GraphicsPipelineVk(const GraphicsPipelineDesc& desc, const PipelineLayoutDesc& reflectionLayout, DeviceVk* device);
  ~GraphicsPipelineVk() override;

  GraphicsPipelineVk(const GraphicsPipelineVk&)            = delete;
  GraphicsPipelineVk& operator=(const GraphicsPipelineVk&) = delete;

  bool rebuild() override;

  // Vulkan-specific methods
  VkPipeline getPipeline() const { return m_pipeline_; }

  VkPipelineLayout getPipelineLayout() const { return m_pipelineLayout_; }

  private:
  bool initialize_();

  bool createPipeline_();
  bool createShaderStages_(std::vector<VkPipelineShaderStageCreateInfo>& shaderStages);
  bool createVertexInputState_(VkPipelineVertexInputStateCreateInfo& vertexInputInfo);
  bool createInputAssemblyState_(VkPipelineInputAssemblyStateCreateInfo& inputAssembly);
  bool createRasterizationState_(VkPipelineRasterizationStateCreateInfo& rasterizer);
  bool createMultisampleState_(VkPipelineMultisampleStateCreateInfo& multisampling);
  bool createDepthStencilState_(VkPipelineDepthStencilStateCreateInfo& depthStencil);
  bool createColorBlendState_(VkPipelineColorBlendStateCreateInfo& colorBlending);
  bool createPipelineLayout_();

  DeviceVk* m_device_;

  VkPipeline       m_pipeline_;
  VkPipelineLayout m_pipelineLayout_;

  std::vector<std::unique_ptr<DescriptorSetLayout>> m_ownedLayouts_;  // to avoid dangling pointers
};

}  // namespace rhi
}  // namespace gfx
}  // namespace arise

#endif  // ARISE_PIPELINE_VK_H