#ifndef ARISE_RUNTIME_SETTINGS_H
#define ARISE_RUNTIME_SETTINGS_H

#include "config/config_manager.h"
#include "gfx/rhi/common/rhi_enums.h"
#include "utils/path_manager/path_manager.h"
#include "utils/service/service_locator.h"

#include <math_library/vector.h>

namespace arise {

class RuntimeSettings {
  public:
  static RuntimeSettings& s_get();

  RuntimeSettings(const RuntimeSettings&)            = delete;
  RuntimeSettings& operator=(const RuntimeSettings&) = delete;

  const math::Vector3f& getWorldUp() const;

  void updateFromConfig();

  private:
  RuntimeSettings();
  math::Vector3f m_worldUp_;
};

}  // namespace arise

#endif  // ARISE_RUNTIME_SETTINGS_H
