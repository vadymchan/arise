#ifndef ARISE_WINDOW_H
#define ARISE_WINDOW_H

#include "event/event.h"
#include "utils/enum/enum_util.h"
#include "utils/logger/log.h"

#include <SDL.h>
#include <math_library/dimension.h>
#include <math_library/point.h>

#include <filesystem>
#include <string>
#include <utility>

namespace arise {

class Window {
  public:
  enum class Flags : uint32_t {
    None              = 0,
    Fullscreen        = SDL_WINDOW_FULLSCREEN,
    Maximized         = SDL_WINDOW_MAXIMIZED,
    Shown             = SDL_WINDOW_SHOWN,
    Borderless        = SDL_WINDOW_BORDERLESS,
    Resizable         = SDL_WINDOW_RESIZABLE,
    AllowHighDpi      = SDL_WINDOW_ALLOW_HIGHDPI,
    FullscreenDesktop = SDL_WINDOW_FULLSCREEN_DESKTOP,
    Vulkan            = SDL_WINDOW_VULKAN
  };

  Window(std::string title, math::Dimension2i size, math::Point2i position, Flags flags)
      : m_title_{std::move(title)}
      , m_size_{std::move(size)}
      , m_position_{std::move(position)}
      , m_flags_{flags}
      , m_window_{SDL_CreateWindow(m_title_.c_str(),
                                   m_position_.x(),
                                   m_position_.y(),
                                   m_size_.width(),
                                   m_size_.height(),
                                   static_cast<std::uint32_t>(m_flags_))} {
    if (!m_window_) {
      std::string errorMessage = SDL_GetError();
      LOG_ERROR("Failed to create SDL window: {}", errorMessage);
    }

    int actualWidth, actualHeight;
    SDL_GetWindowSize(m_window_, &actualWidth, &actualHeight);
    m_size_ = math::Dimension2i(actualWidth, actualHeight);
  }

  Window(const Window&) = delete;
  Window(Window&&)      = delete;

  ~Window() {
    if (m_window_) {
      SDL_DestroyWindow(m_window_);
      m_window_ = nullptr;
    }
  }

  // clang-format off

  [[nodiscard]] auto getTitle() const -> const std::string& { return m_title_; }
  [[nodiscard]] auto getSize() const -> const math::Dimension2i& { return m_size_; }
  [[nodiscard]] auto getPosition() const -> const math::Point2i& { return m_position_; }
  [[nodiscard]] auto getFlags() const -> Flags { return m_flags_; }
  /**
   * This function provides access to the internal SDL_Window handle. 
   * It is important to use this handle with caution as any modifications 
   * or incorrect usage can lead to undefined behavior in the windowing 
   * system. The handle should not be stored persistently, and it's discouraged 
   * to modify the window state outside of the designed interface of the Window class.
   */
  [[nodiscard]] auto getNativeWindowHandle() const ->  SDL_Window* { return m_window_; }
  [[nodiscard]] auto isMinimized() const -> bool { return (SDL_GetWindowFlags(m_window_) & SDL_WINDOW_MINIMIZED) != 0; }

  // clang-format on

  void onResize(const WindowEvent& event) {
    m_size_ = math::Dimension2i(event.data1, event.data2);
    SDL_SetWindowSize(m_window_, event.data1, event.data2);
  }

  /**
   * Sets the window icon from an image file
   * @param iconPath Path to the icon image file (supports PNG, JPG, etc.)
   * @return true if the icon was successfully set, false otherwise
   */
  bool setWindowIcon(const std::filesystem::path& iconPath);

  auto operator=(const Window&) -> Window& = delete;
  auto operator=(Window&&) -> Window&      = delete;

  private:
  std::string       m_title_;
  math::Dimension2i m_size_;
  math::Point2i     m_position_;
  Flags             m_flags_;
  SDL_Window*       m_window_{nullptr};
};

DECLARE_ENUM_BIT_OPERATORS(Window::Flags)

}  // namespace arise

#endif  // ARISE_WINDOW
