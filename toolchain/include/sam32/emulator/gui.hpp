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

  struct WatchItem {
    bool is_register;
    uint32_t address_or_index;
  };

  int display_radix = 16;
  std::vector<WatchItem> watchlist;
  float r0_flash_timer = 0.0f;
  uint32_t last_pc_flashed = 0xFFFFFFFF; // track PC so we only flash once per execute

  uint32_t step_over_breakpoint = 0xFFFFFFFF;
  uint32_t step_out_breakpoint = 0xFFFFFFFF;

  void render_controls();
  void render_editor();
  void render_registers();
  void render_flags();
  void render_instruction_details();
  void render_watchlist();
  void render_lst();
  void render_hex_dump();
  void render_stack_visualizer();
  void render_call_stack();
};

}  // namespace sam32::gui