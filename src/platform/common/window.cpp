#include "platform/common/window.h"

#include "utils/image/image_manager.h"
#include "utils/service/service_locator.h"

namespace arise {

bool Window::setWindowIcon(const std::filesystem::path& iconPath) {
  if (!m_window_) {
    LOG_ERROR("Cannot set window icon: window is null");
    return false;
  }

  auto imageManager = ServiceLocator::s_get<ImageManager>();
  if (!imageManager) {
    LOG_ERROR("Cannot set window icon: ImageManager not available");
    return false;
  }

  auto image = imageManager->getImage(iconPath);
  if (!image) {
    LOG_ERROR("Failed to load icon image: " + iconPath.string());
    return false;
  }

  SDL_Surface* surface = SDL_CreateRGBSurfaceFrom(const_cast<void*>(static_cast<const void*>(image->pixels.data())),
                                                  static_cast<int>(image->width),
                                                  static_cast<int>(image->height),
                                                  32,                                  // 32 bits per pixel (RGBA)
                                                  static_cast<int>(image->width * 4),  // pitch (bytes per row)
                                                  0x00'00'00'FF,                       // R mask
                                                  0x00'00'FF'00,                       // G mask
                                                  0x00'FF'00'00,                       // B mask
                                                  0xFF'00'00'00                        // A mask
  );

  if (!surface) {
    LOG_ERROR("Failed to create SDL surface for icon: " + std::string(SDL_GetError()));
    return false;
  }

  SDL_SetWindowIcon(m_window_, surface);

  SDL_FreeSurface(surface);

  LOG_INFO("Window icon set successfully from: " + iconPath.string());
  return true;
}

}  // namespace arise
