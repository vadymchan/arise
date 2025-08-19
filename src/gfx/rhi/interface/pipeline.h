#ifndef ARISE_PIPELINE_H
#define ARISE_PIPELINE_H

#include "gfx/rhi/common/rhi_enums.h"
#include "gfx/rhi/common/rhi_types.h"
#include "gfx/rhi/shader_reflection/shader_reflection_types.h"

#include <atomic>

namespace arise {
namespace gfx {
namespace rhi {

/*
 * A pipeline represents the configuration of the graphics/compute pipeline
 * stages and states for rendering or computation. It encapsulates shaders,
 * fixed-function state, and pipeline layout information.
 */
class Pipeline {
  public:
  virtual ~Pipeline() = default;

  virtual PipelineType getType() const = 0;

  bool needsUpdate() const { return m_updateFrame == 0; }

  void scheduleUpdate(uint32_t delayFrames) { m_updateFrame = delayFrames; }

  void decrementUpdateCounter() {
    if (m_updateFrame > 0) {
      m_updateFrame--;
    }
  }

  virtual bool rebuild() = 0;

  protected:
  std::atomic<int32_t> m_updateFrame{-1};
};

/**
 * Represents a complete graphics pipeline state object (PSO) that can be bound for rendering.
 * This includes vertex input configuration, shader stages, and fixed-function state.
 */
class GraphicsPipeline : public Pipeline {
  public:
  GraphicsPipeline(const GraphicsPipelineDesc& desc, const PipelineLayoutDesc& reflectionLayout = {})
      : m_desc_(desc)
      , m_reflectionLayout_(reflectionLayout) {}

  virtual ~GraphicsPipeline() = default;

  PipelineType getType() const { return PipelineType::Graphics; }

  const GraphicsPipelineDesc& getDesc() const { return m_desc_; }

  PrimitiveType getPrimitiveType() const { return m_desc_.inputAssembly.topology; }

  virtual bool hasBindingSlot(uint32_t slot) const;

  protected:
  GraphicsPipelineDesc m_desc_;
  PipelineLayoutDesc   m_reflectionLayout_; 
};

}  // namespace rhi
}  // namespace gfx
}  // namespace arise

#endif  // ARISE_PIPELINE_H