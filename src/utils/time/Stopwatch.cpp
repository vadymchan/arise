#include "utils/time/stopwatch.h"

namespace arise {

void Stopwatch::start() {
  m_isRunning_ = true;
  m_startTime_ = Clock::now();
}

void Stopwatch::stop() {
  m_isRunning_  = false;
  m_startTime_  = TimePoint{};
  m_pausedTime_ = TimePoint{};
}

void Stopwatch::resume() {
  if (!m_isRunning_ && m_pausedTime_ != TimePoint{}) {
    m_startTime_  += Clock::now() - m_pausedTime_;
    m_isRunning_   = true;
    m_pausedTime_  = TimePoint{};
  }
}

void Stopwatch::pause() {
  if (m_isRunning_) {
    m_pausedTime_ = Clock::now();
    m_isRunning_  = false;
  }
}

void Stopwatch::reset() {
  m_startTime_  = Clock::now();
  m_pausedTime_ = TimePoint{};
}

auto Stopwatch::isRunning() const -> bool {
  return m_isRunning_;
}

}  // namespace arise