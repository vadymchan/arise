#include "platform/common/window.h"

#include "utils/image/image_manager.h"
#include "utils/service/service_locator.h"

namespace arise {

bool Window::setWindowIcon(const std::filesystem::path& iconPath) {
  if (!m_window_) {
    GlobalLogger::Log(LogLevel::Error, "Cannot set window icon: window is null");
    return false;
  }

  auto imageManager = ServiceLocator::s_get<ImageManager>();
  if (!imageManager) {
    GlobalLogger::Log(LogLevel::Error, "Cannot set window icon: ImageManager not available");
    return false;
  }

  auto image = imageManager->getImage(iconPath);
  if (!image) {
    GlobalLogger::Log(LogLevel::Error, "Failed to load icon image: " + iconPath.string());
    return false;
  }

  SDL_Surface* surface = SDL_CreateRGBSurfaceFrom(
      const_cast<void*>(static_cast<const void*>(image->pixels.data())),
      static_cast<int>(image->width),
      static_cast<int>(image->height),
      32,  // 32 bits per pixel (RGBA)
      static_cast<int>(image->width * 4),  // pitch (bytes per row)
      0x000000FF,  // R mask
      0x0000FF00,  // G mask
      0x00FF0000,  // B mask
      0xFF000000   // A mask
  );

  if (!surface) {
    GlobalLogger::Log(LogLevel::Error, "Failed to create SDL surface for icon: " + std::string(SDL_GetError()));
    return false;
  }

  SDL_SetWindowIcon(m_window_, surface);
  
  SDL_FreeSurface(surface);

  GlobalLogger::Log(LogLevel::Info, "Window icon set successfully from: " + iconPath.string());
  return true;
}

}  // namespace arise
