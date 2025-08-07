#include "gfx/rhi/shader_reflection/shader_reflection_utils.h"

#include "utils/logger/log.h"

namespace arise {
namespace gfx {
namespace rhi {
namespace reflection_utils {

void logPipelineLayoutContents(const PipelineLayoutDesc& layout) {
  LOG_INFO("Pipeline Layout Description:");
  LOG_INFO("  Push Constant Size: {} bytes", layout.pushConstantSize);
  LOG_INFO("  Descriptor Sets: {}", layout.setLayouts.size());

  for (size_t setIndex = 0; setIndex < layout.setLayouts.size(); ++setIndex) {
    const auto& setLayout = layout.setLayouts[setIndex];
    LOG_INFO("    Set {}: {} bindings", setIndex, setLayout.bindings.size());

    for (const auto& binding : setLayout.bindings) {
      LOG_INFO("      Binding {}: {} (count: {})",
               binding.binding,
               bindingTypeToString(binding.type),
               binding.descriptorCount);
    }
  }
}

const char* bindingTypeToString(ShaderBindingType type) {
  switch (type) {
    case ShaderBindingType::Sampler:
      return "Sampler";
    case ShaderBindingType::TextureSrv:
      return "TextureSrv";
    case ShaderBindingType::TextureUav:
      return "TextureUav";
    case ShaderBindingType::Uniformbuffer:
      return "UniformBuffer";
    case ShaderBindingType::BufferSrv:
      return "BufferSrv";
    default:
      return "Unknown";
  }
}

}  // namespace reflection_utils
}  // namespace rhi
}  // namespace gfx
}  // namespace arise