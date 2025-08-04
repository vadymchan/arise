#include "pipeline_utils.h"

#include "gfx/rhi/interface/descriptor.h"
#include "gfx/rhi/interface/device.h"
#include "gfx/rhi/interface/shader.h"
#include "gfx/rhi/shader_reflection/pipeline_layout_builder.h"
#include "gfx/rhi/shader_reflection/shader_reflection_utils.h"
#include "utils/logger/global_logger.h"

namespace arise {
namespace gfx {
namespace rhi {
namespace pipeline_utils {

PipelineLayoutDesc generatePipelineLayoutFromShaders(const std::vector<Shader*>& shaders) {
  PipelineLayoutBuilder builder;

  for (const auto* shader : shaders) {
    if (shader) {
      builder.addShader(shader->getMeta());
    }
  }

  PipelineLayoutDesc result = builder.build();

  GlobalLogger::Log(LogLevel::Info, "Generated pipeline layout from shader reflection:");
  reflection_utils::logPipelineLayoutContents(result);

  return result;
}

std::vector<const DescriptorSetLayout*> createDescriptorSetLayouts(
    Device*                                            device,
    const PipelineLayoutDesc&                          pipelineLayout,
    std::vector<std::unique_ptr<DescriptorSetLayout>>& ownedLayouts) {
  std::vector<const DescriptorSetLayout*> layoutPtrs;

  for (const auto& setLayoutDesc : pipelineLayout.setLayouts) {
    if (!setLayoutDesc.bindings.empty()) {
      auto layout = device->createDescriptorSetLayout(setLayoutDesc);
      layoutPtrs.push_back(layout.get());
      ownedLayouts.push_back(std::move(layout));
    }
  }

  return layoutPtrs;
}

}  // namespace pipeline_utils
}  // namespace rhi
}  // namespace gfx
}  // namespace arise