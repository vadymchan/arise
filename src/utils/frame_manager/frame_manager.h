#ifndef ARISE_FRAME_MANAGER_H
#define ARISE_FRAME_MANAGER_H

#include <cstdint>

namespace arise {

/**
 * Manages frame index logic for multi-buffered rendering.
 * Provides consistent frame indexing across the entire rendering pipeline.
 */
class FrameManager {
  public:
  explicit FrameManager(uint32_t framesInFlight = 2)
      : m_currentFrameIndex(0)
      , m_maxFramesInFlight(framesInFlight)
      , m_totalFrameCount(0) {}

  uint32_t getCurrentFrameIndex() const { return m_currentFrameIndex; }

  uint32_t getMaxFramesInFlight() const { return m_maxFramesInFlight; }

  uint64_t getTotalFrameCount() const { return m_totalFrameCount; }

  void advanceFrame() {
    m_currentFrameIndex = (m_currentFrameIndex + 1) % m_maxFramesInFlight;
    ++m_totalFrameCount;
  }

  private:
  uint32_t m_currentFrameIndex;
  uint32_t m_maxFramesInFlight;
  uint64_t m_totalFrameCount;
};

}  // namespace arise

#endif  // ARISE_FRAME_MANAGER_H