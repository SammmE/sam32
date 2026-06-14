#include <fmt/base.h>
#include <fmt/core.h>
#include <CLI/CLI.hpp>
#include <filesystem>
#include <fstream>
#include <string>

#include <SDL3/SDL.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>

#include <sam32/assembler/lexer.hpp>
#include <sam32/assembler/parser.hpp>
#include <sam32/utils/utils.hpp>
#include "sam32/assembler/compiler.hpp"
#include "sam32/emulator/IconsCodicons.h"
#include "sam32/emulator/emulator.hpp"
#include "sam32/emulator/gui.hpp"

#define FONT_SIZE 20.0f

void run_gui(const std::string& bin_path) {
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    fmt::println("Error: SDL_Init failed: {}", SDL_GetError());
    return;
  }

  SDL_WindowFlags window_flags =
      (SDL_WindowFlags)(SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
  SDL_Window* window =
      SDL_CreateWindow("SAM32 Emulator Native", 1280, 720, window_flags);
  if (!window) {
    fmt::println("Error: SDL_CreateWindow failed: {}", SDL_GetError());
    return;
  }

  SDL_StartTextInput(window);

  SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
  if (!renderer) {
    fmt::println("Error: SDL_CreateRenderer failed: {}", SDL_GetError());
    return;
  }

  float dpi_scale = 1.0f;
  int win_w, win_h, win_pw, win_ph;
  SDL_GetWindowSize(window, &win_w, &win_h);
  SDL_GetWindowSizeInPixels(window, &win_pw, &win_ph);
  if (win_w > 0) {
    dpi_scale = (float)win_pw / (float)win_w;
  }

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  (void)io;

  ImFontConfig base_font_config;
  io.Fonts->AddFontFromFileTTF("assets/RobotoMono-Regular.ttf",
                               FONT_SIZE * dpi_scale, &base_font_config);
  // io.Fonts->AddFontDefault(&base_font_config);

  ImFontConfig icons_config;
  icons_config.MergeMode = true;
  icons_config.PixelSnapH = true;
  icons_config.GlyphOffset.y = 2.5f * dpi_scale;
  static const ImWchar icon_ranges[] = {ICON_MIN_CI, ICON_MAX_CI, 0};
  io.Fonts->AddFontFromFileTTF("assets/codicon.ttf", FONT_SIZE * dpi_scale, &icons_config, icon_ranges);

  ImGui::StyleColorsDark();

  ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
  ImGui_ImplSDLRenderer3_Init(renderer);

  sam32::Emulator emu;
  if (!bin_path.empty() && std::filesystem::exists(bin_path)) {
    std::ifstream input_file(bin_path, std::ios::binary);
    input_file.seekg(0, std::ios::end);
    size_t size = input_file.tellg();
    input_file.seekg(0, std::ios::beg);

    std::vector<uint8_t> program(size);
    input_file.read(reinterpret_cast<char*>(program.data()), size);
    emu.load_program(program);
  }

  sam32::gui::EmulatorGui gui(emu);

  bool done = false;
  while (!done) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      ImGui_ImplSDL3_ProcessEvent(&event);
      if (event.type == SDL_EVENT_QUIT)
        done = true;
      if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
          event.window.windowID == SDL_GetWindowID(window))
        done = true;
    }

    ImGui_ImplSDL3_NewFrame();
    ImGui_ImplSDLRenderer3_NewFrame();
    ImGui::NewFrame();

    gui.render();

    ImGui::Render();
    SDL_SetRenderDrawColor(renderer, 45, 45, 45, 255);
    SDL_RenderClear(renderer);
    ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
    SDL_RenderPresent(renderer);
  }

  ImGui_ImplSDLRenderer3_Shutdown();
  ImGui_ImplSDL3_Shutdown();
  ImGui::DestroyContext();

  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
}

int main(int argc, char** argv) {
  CLI::App app;

  auto compile = app.add_subcommand("compile", "Compile file");
  auto emulator = app.add_subcommand("emulator", "Run Emulator GUI");

  std::string file_path;
  std::string output_path;
  std::string bin_path;
  bool verbose = false;

  compile
      ->add_option("file", file_path,
                   "Assembly file to compile into machine code")
      ->required();
  compile->add_option("-o,--output", output_path,
                      "Output file for the compiled machine code");
  compile->add_flag("-v,--verbose", verbose,
                    "Verbose output during compilation");

  emulator->add_option("-b,--bin", bin_path,
                       "Binary file to load into emulator");

  app.require_subcommand(1);

  compile->callback([&]() {
    if (!std::filesystem::exists(file_path)) {
      fmt::println("{} does not exist", file_path);
      return;
    }

    std::vector<sam32::Token> tokens =
        sam32::LexString(sam32::read_file(file_path));

    if (verbose)
      fmt::println("Lexed {} tokens", tokens.size());

    sam32::Parser parser(tokens);
    parser.parse();

    if (verbose) {
      fmt::println("Parsed {} text instructions, {} data items",
                   parser.text_segment.instructions.size(),
                   parser.data_segment.instructions.size());
    }

    auto [machine_code, lst_rows] =
        sam32::encode(parser.text_segment, parser.data_segment);

    if (output_path.empty()) {
      output_path = file_path.substr(0, file_path.find_last_of('.')) + ".bin";
    }

    sam32::write_file(output_path, machine_code);
    fmt::println("Compiled machine code written to {}", output_path);
  });

  emulator->callback([&]() { run_gui(bin_path); });

  CLI11_PARSE(app, argc, argv);
  return 0;
}
