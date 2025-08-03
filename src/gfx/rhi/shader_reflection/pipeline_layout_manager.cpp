#include "gfx/rhi/shader_reflection/pipeline_layout_manager.h"

#include "gfx/rhi/interface/device.h"
#include "utils/logger/global_logger.h"

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

  GlobalLogger::Log(LogLevel::Info,
                    "Created and managing " + std::to_string(layoutPtrs.size()) + " descriptor set layouts");

  return layoutPtrs;
}

void PipelineLayoutManager::cleanup() {
  GlobalLogger::Log(LogLevel::Info, "Cleaning up " + std::to_string(m_managedLayouts.size()) + " managed layouts");
  m_managedLayouts.clear();
}

}  // namespace rhi
}  // namespace gfx
}  // namespace arise