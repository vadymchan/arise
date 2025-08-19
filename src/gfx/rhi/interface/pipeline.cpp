#include "gfx/rhi/interface/pipeline.h"

#include "gfx/rhi/interface/descriptor.h"
#include "utils/logger/log.h"

namespace arise {
namespace gfx {
namespace rhi {

bool GraphicsPipeline::hasBindingSlot(uint32_t slot) const {
  if (m_reflectionLayout_.setLayouts.empty()) {
    if (slot >= m_desc_.setLayouts.size()) {
      return false;
    }

    const DescriptorSetLayout* layout = m_desc_.setLayouts[slot];
    if (layout == nullptr) {
      return false;
    }

    bool hasBindings = !layout->getDesc().bindings.empty();
    return hasBindings;
  }

  if (slot >= m_reflectionLayout_.setLayouts.size()) {
    return false;
  }

  const auto& layoutDesc = m_reflectionLayout_.setLayouts[slot];
  if (layoutDesc.bindings.empty()) {
    return false;
  }

  return true;
}

}  // namespace rhi
}  // namespace gfx
}  // namespace arise