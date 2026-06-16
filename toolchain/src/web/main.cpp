#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#include <SDL3/SDL.h>
#include <fmt/base.h>
#include <fmt/core.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>

#include "sam32/emulator/emulator.hpp"
#include "sam32/emulator/gui.hpp"

struct AppState {
  SDL_Window* window;
  SDL_Renderer* renderer;
  sam32::Emulator* emu;
  sam32::gui::EmulatorGui* gui;
  bool done;
};

void main_loop_step(void* arg) {
  AppState* state = static_cast<AppState*>(arg);

  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    ImGui_ImplSDL3_ProcessEvent(&event);
    if (event.type == SDL_EVENT_QUIT)
      state->done = true;
    if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
        event.window.windowID == SDL_GetWindowID(state->window))
      state->done = true;
  }

  ImGui_ImplSDLRenderer3_NewFrame();
  ImGui_ImplSDL3_NewFrame();
  ImGui::NewFrame();

  state->gui->render();

  ImGui::Render();
  SDL_SetRenderDrawColor(state->renderer, 45, 45, 45, 255);
  SDL_RenderClear(state->renderer);
  ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), state->renderer);
  SDL_RenderPresent(state->renderer);
}

int main(int argc, char** argv) {
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    fmt::println("Error: SDL_Init failed: {}", SDL_GetError());
    return -1;
  }

  SDL_WindowFlags window_flags =
      (SDL_WindowFlags)(SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
  SDL_Window* window =
      SDL_CreateWindow("SAM32 Emulator Web", 1280, 720, window_flags);
  if (!window) {
    fmt::println("Error: SDL_CreateWindow failed: {}", SDL_GetError());
    return -1;
  }

  SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
  if (!renderer) {
    fmt::println("Error: SDL_CreateRenderer failed: {}", SDL_GetError());
    return -1;
  }

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  (void)io;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

  ImGui::StyleColorsDark();

  ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
  ImGui_ImplSDLRenderer3_Init(renderer);

  sam32::Emulator emu(1024 * 1024, true);
  sam32::gui::EmulatorGui gui(emu);

  AppState state;
  state.window = window;
  state.renderer = renderer;
  state.emu = &emu;
  state.gui = &gui;
  state.done = false;

#ifdef __EMSCRIPTEN__
  emscripten_set_main_loop_arg(main_loop_step, &state, 0, 1);
#else
  while (!state.done) {
    main_loop_step(&state);
  }
#endif
  ImGui_ImplSDLRenderer3_Shutdown();
  ImGui_ImplSDL3_Shutdown();
  ImGui::DestroyContext();

  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();

  return 0;
}
