#include "gfx/rhi/shader_reflection/pipeline_layout_builder.h"

#include "utils/logger/log.h"

namespace arise {
namespace gfx {
namespace rhi {

void PipelineLayoutBuilder::addShader(const ShaderMeta& meta) {
  // Merge push constant size
  m_maxPushConstantSize = std::max(m_maxPushConstantSize, meta.pushConstantSize);

  // Add all resource bindings
  for (const auto& binding : meta.bindings) {
    auto& setMap = m_setBindings[binding.set];
    auto  it     = setMap.find(binding.binding);

    if (it != setMap.end()) {
      mergeBinding_(it->second, binding);
    } else {
      setMap[binding.binding] = binding;
    }
  }
}

PipelineLayoutDesc PipelineLayoutBuilder::build() {
  PipelineLayoutDesc desc;
  desc.pushConstantSize = m_maxPushConstantSize;

  // Build descriptor set layouts for each set
  for (const auto& [setIndex, bindings] : m_setBindings) {
    if (bindings.empty()) {
      continue;
    }

    // Ensure we have enough set layouts
    if (desc.setLayouts.size() <= setIndex) {
      desc.setLayouts.resize(setIndex + 1);
    }

    DescriptorSetLayoutDesc& setLayout = desc.setLayouts[setIndex];
    setLayout.bindings.clear();
    setLayout.bindings.reserve(bindings.size());

    // Convert bindings to descriptor set layout bindings
    for (const auto& [bindingIndex, binding] : bindings) {
      DescriptorSetLayoutBindingDesc layoutBinding;
      layoutBinding.binding         = binding.binding;
      layoutBinding.type            = binding.type;
      layoutBinding.descriptorCount = binding.descriptorCount;
      layoutBinding.stageFlags      = binding.stageFlags;

      setLayout.bindings.push_back(layoutBinding);
    }
  }

  return desc;
}

void PipelineLayoutBuilder::mergeBinding_(ShaderResourceBinding& existing, const ShaderResourceBinding& newBinding) {
  // Validate that bindings are compatible
  if (existing.type != newBinding.type) {
    LOG_WARN("Binding type mismatch for set {} binding {}: {} vs {}",
             newBinding.set,
             newBinding.binding,
             existing.name,
             newBinding.name);
  }

  if (existing.descriptorCount != newBinding.descriptorCount) {
    GlobalLogger::Log(LogLevel::Warning,
                      "Descriptor count mismatch for set " + std::to_string(newBinding.set) + " binding "
                          + std::to_string(newBinding.binding) + ": " + std::to_string(existing.descriptorCount)
                          + " vs " + std::to_string(newBinding.descriptorCount));
  }

  existing.stageFlags = static_cast<ShaderStageFlag>(static_cast<uint32_t>(existing.stageFlags)
                                                     | static_cast<uint32_t>(newBinding.stageFlags));

  existing.nonUniform = existing.nonUniform || newBinding.nonUniform;
}

}  // namespace rhi
}  // namespace gfx
}  // namespace arise