#define SDL_MAIN_HANDLED

#include <core/application.h>
#include <core/engine.h>

using namespace arise;

#if (defined(_WIN32) || defined(_WIN64)) && defined(ARISE_WINDOWS_SUBSYSTEM)
#include <windows.h>
int WINAPI wWinMain(_In_ HINSTANCE     hInstance,
                    _In_opt_ HINSTANCE hPrevInstance,
                    _In_ PWSTR         pCmdLine,
                    _In_ int           nCmdShow) {
#else
auto main(int argc, char* argv[]) -> int {

#endif

  // Inform SDL that the program will handle its own initialization
  SDL_SetMainReady();

  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
    SDL_Log("Unable to initialize SDL: %s", SDL_GetError());
    return EXIT_FAILURE;
  }

  arise::Engine engine;

  engine.initialize();

  auto game = std::make_unique<arise::Application>();

  engine.setGame(game.get());

  game->setup();

  engine.run();

  SDL_Quit();

  return EXIT_SUCCESS;
}