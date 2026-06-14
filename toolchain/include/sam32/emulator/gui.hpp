#pragma once

#include <imgui.h>
#include <vector>
#include "sam32/assembler/compiler.hpp"
#include "sam32/emulator/TextEditor.hpp"
#include "sam32/emulator/emulator.hpp"

namespace sam32::gui {

class EmulatorGui {
 public:
  EmulatorGui(Emulator& emu);
  ~EmulatorGui() = default;

  void render();

 private:
  Emulator& emulator;
  ImVec2 window_size;
  bool is_running = false;

  std::vector<uint8_t> machine_code;
  std::vector<LstRow> lst;

  TextEditor editor;
  bool te_initialized = false;

  void safe_tick();

  void render_controls();
  void render_editor();
};

}  // namespace sam32::gui