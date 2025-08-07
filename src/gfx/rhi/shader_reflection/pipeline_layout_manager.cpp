#include "gfx/rhi/shader_reflection/pipeline_layout_manager.h"

#include "gfx/rhi/interface/device.h"
#include "utils/logger/log.h"

namespace arise {
namespace gfx {
namespace rhi {

std::vector<const DescriptorSetLayout*> PipelineLayoutManager::createAndManageLayouts(
    Device* device, const PipelineLayoutDesc& pipelineLayout) {
  std::vector<const DescriptorSetLayout*> layoutPtrs;
  layoutPtrs.reserve(pipelineLayout.setLayouts.size());

  for (const auto& setLayoutDesc : pipelineLayout.setLayouts) {
    if (!setLayoutDesc.bindings.empty()) {
      auto layout = device->createDescriptorSetLayout(setLayoutDesc);
      layoutPtrs.push_back(layout.get());
      m_managedLayouts.push_back(std::move(layout));
    }
  }

  LOG_INFO("Created and managing {} descriptor set layouts", layoutPtrs.size());

  return layoutPtrs;
}

void PipelineLayoutManager::cleanup() {
  LOG_INFO("Cleaning up {} managed layouts", m_managedLayouts.size());
  m_managedLayouts.clear();
}

}  // namespace rhi
}  // namespace gfx
}  // namespace arise