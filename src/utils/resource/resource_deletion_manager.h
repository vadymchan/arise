// resource_deletion_manager.h
#ifndef ARISE_RESOURCE_DELETION_MANAGER_H
#define ARISE_RESOURCE_DELETION_MANAGER_H

#include "utils/logger/global_logger.h"

#include <functional>
#include <string>
#include <vector>

namespace arise {

/**
 * @brief Manages deferred deletion of GPU resources to ensure they are not deleted while in use
 */
class ResourceDeletionManager {
  public:
  ResourceDeletionManager()  = default;
  ~ResourceDeletionManager() = default;

  void setDefaultFrameDelay(uint32_t frameDelay) { m_defaultFrameDelay = frameDelay; }

  void setCurrentFrame(uint64_t currentFrame) {
    m_currentFrame = currentFrame;
    processPendingDeletions();
  }

  template <typename T>
  void enqueueForDeletion(T*                      resource,
                          std::function<void(T*)> deleter,
                          const std::string&      resourceName,
                          const std::string&      resourceType,
                          uint32_t                frameDelay = 0) {
    if (!resource) {
      return;
    }

    if (frameDelay == 0) {
      frameDelay = m_defaultFrameDelay;
    }

    auto deletionCallback = [deleter, resource, resourceName, resourceType]() {
      deleter(resource);
      GlobalLogger::Log(LogLevel::Info, resourceType + " '" + resourceName + "' has been deleted");
    };

    m_pendingDeletions.push_back({m_currentFrame + frameDelay, deletionCallback, resourceName, resourceType});

    GlobalLogger::Log(LogLevel::Debug,
                      resourceType + " '" + resourceName + "' scheduled for deletion in " + std::to_string(frameDelay)
                          + " frames (frame " + std::to_string(m_currentFrame + frameDelay) + ")");
  }

  void processPendingDeletions() {
    if (m_pendingDeletions.empty()) {
      return;
    }

    size_t initialSize = m_pendingDeletions.size();

    std::vector<PendingDeletion> remainingDeletions;
    remainingDeletions.reserve(initialSize);

    size_t deletionCount = 0;

    for (const auto& deletion : m_pendingDeletions) {
      if (deletion.frameToDelete <= m_currentFrame) {
        deletion.deletionCallback();
        deletionCount++;
      } else {
        remainingDeletions.push_back(deletion);
      }
    }

    m_pendingDeletions = std::move(remainingDeletions);

    if (deletionCount > 0) {
      GlobalLogger::Log(LogLevel::Debug,
                        "Deleted " + std::to_string(deletionCount) + " resources, "
                            + std::to_string(m_pendingDeletions.size())
                            + " still pending (current frame: " + std::to_string(m_currentFrame) + ")");
    }
  }

  std::vector<std::string> getPendingResourceNames() const {
    std::vector<std::string> pendingResourceNames;
    pendingResourceNames.reserve(m_pendingDeletions.size());

    for (const auto& deletion : m_pendingDeletions) {
      pendingResourceNames.push_back(deletion.resourceType + ": " + deletion.resourceName + " (at frame "
                                     + std::to_string(deletion.frameToDelete) + ")");
    }
    return pendingResourceNames;
  }

  void clearPendingDeletions(bool executeCallbacks = true) {
    if (executeCallbacks) {
      for (const auto& deletion : m_pendingDeletions) {
        deletion.deletionCallback();
      }
    }
    m_pendingDeletions.clear();
  }

  size_t getPendingDeletionCount() const { return m_pendingDeletions.size(); }

  private:
  struct PendingDeletion {
    uint64_t              frameToDelete;
    std::function<void()> deletionCallback;
    std::string           resourceName;
    std::string           resourceType;
  };

  std::vector<PendingDeletion> m_pendingDeletions;
  uint64_t                     m_currentFrame      = 0;
  uint32_t                     m_defaultFrameDelay = 2;
};

}  // namespace arise

#endif  // ARISE_RESOURCE_DELETION_MANAGER_H