#ifndef ARISE_PIPELINE_LAYOUT_MANAGER_H
#define ARISE_PIPELINE_LAYOUT_MANAGER_H

#include "gfx/rhi/common/rhi_types.h"
#include "gfx/rhi/interface/descriptor.h"
#include "shader_reflection_types.h"

#include <memory>
#include <vector>

namespace arise {
namespace gfx {
namespace rhi {

class Device;

/**
 * Manages lifetime of automatically created descriptor set layouts.
 * Solves the problem of keeping layouts alive for pipeline duration
 */
class PipelineLayoutManager {
  public:
  std::vector<const DescriptorSetLayout*> createAndManageLayouts(Device*                   device,
                                                                 const PipelineLayoutDesc& pipelineLayout);

  void cleanup();

  private:
  std::vector<std::unique_ptr<DescriptorSetLayout>> m_managedLayouts;
};

}  // namespace rhi
}  // namespace gfx
}  // namespace arise

#endif  // ARISE_PIPELINE_LAYOUT_MANAGER_H