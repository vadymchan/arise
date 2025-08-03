#ifndef ARISE_PIPELINE_LAYOUT_BUILDER_H
#define ARISE_PIPELINE_LAYOUT_BUILDER_H

#include "gfx/rhi/common/rhi_types.h"
#include "shader_reflection_types.h"

#include <map>

namespace arise {
namespace gfx {
namespace rhi {

/**
 * Helper class to build PipelineLayoutDesc from multiple ShaderMeta instances
 */
class PipelineLayoutBuilder {
  public:
  void addShader(const ShaderMeta& meta);

  PipelineLayoutDesc build();

  private:
  std::map<uint32_t, std::map<uint32_t, ShaderResourceBinding>> m_setBindings;  // set -> binding -> resource
  uint32_t                                                      m_maxPushConstantSize = 0;

  void mergeBinding_(ShaderResourceBinding& existing, const ShaderResourceBinding& newBinding);
};

}  // namespace rhi
}  // namespace gfx
}  // namespace arise

#endif  // ARISE_PIPELINE_LAYOUT_BUILDER_H