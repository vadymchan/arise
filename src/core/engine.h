#ifndef ARISE_ENGINE_H
#define ARISE_ENGINE_H

#include "editor/editor.h"
#include "platform/common/window.h"

#include <memory>

namespace arise {

class Application;

class Engine {
  public:
  Engine()              = default;
  Engine(const Engine&) = delete;
  Engine(Engine&&)      = delete;

  ~Engine();

  auto initialize() -> bool;
  void render();
  void run();

  Window* getWindow() const { return m_window_.get(); }

  void setGame(Application* game);

  void onClose(const ApplicationEvent& event) { m_isRunning_ = false; }

  auto operator=(const Engine&) -> Engine&  = delete;
  auto operator=(const Engine&&) -> Engine& = delete;

  private:
  void processEvents_();

  void update_(float deltaTime);

  bool                                     m_isRunning_{false};
  gfx::renderer::ApplicationRenderMode     m_applicationMode = gfx::renderer::ApplicationRenderMode::Game;
  std::unique_ptr<Window>                  m_window_;
  std::unique_ptr<gfx::renderer::Renderer> m_renderer_;
  std::unique_ptr<Editor>                  m_editor_;

  Application* m_application_;
};

}  // namespace arise

#endif  // ARISE_ENGINE_H
