#ifndef ARISE_VIEWPORT_CONTEXT_H
#define ARISE_VIEWPORT_CONTEXT_H

#include <string>
#include <unordered_set>

namespace arise {

class ViewportContext {
  public:
  void updateViewport(int x, int y, int width, int height) {
    m_viewportX      = x;
    m_viewportY      = y;
    m_viewportWidth  = width;
    m_viewportHeight = height;
  }

  void updateMousePosition(int x, int y) {
    m_mouseX = x;
    m_mouseY = y;
  }

  int getViewportX() const { return m_viewportX; }
  int getViewportY() const { return m_viewportY; }
  int getViewportWidth() const { return m_viewportWidth; }
  int getViewportHeight() const { return m_viewportHeight; }
  int getMouseX() const { return m_mouseX; }
  int getMouseY() const { return m_mouseY; }

  void focusViewport(const std::string& id) { focusedViewports.insert(id); }
  void unfocusViewport(const std::string& id) { focusedViewports.erase(id); }
  void clearFocus() { focusedViewports.clear(); }
  bool isViewportFocused(const std::string& id) const {
    return focusedViewports.empty() || focusedViewports.count(id) > 0;
  }

  private:
  int                             m_viewportX      = 0;
  int                             m_viewportY      = 0;
  int                             m_viewportWidth  = 0;
  int                             m_viewportHeight = 0;
  int                             m_mouseX         = 0;
  int                             m_mouseY         = 0;
  std::unordered_set<std::string> focusedViewports;
};

}  // namespace arise

#endif  // ARISE_VIEWPORT_CONTEXT_H
