#include <fmt/core.h>
#include <imgui.h>
#include <fstream>

#include <sam32/emulator/IconsCodicons.h>
#include <sam32/assembler/parser.hpp>
#include <sam32/emulator/TextEditor.hpp>
#include <sam32/emulator/gui.hpp>
#include "sam32/emulator/ImGuiFileDialog.hpp"
#include "sam32/utils/utils.hpp"

TextEditor::LanguageDefinition GetSam32LanguageDefinition() {
  TextEditor::LanguageDefinition langDef;

  langDef.mName = "SAM32";
  langDef.mCaseSensitive = false;
  langDef.mAutoIndentation = true;

  // Define Keywords
  for (const auto& mnem : sam32::mnemonic_names) {
    std::string upper_mnem = mnem;
    std::transform(upper_mnem.begin(), upper_mnem.end(), upper_mnem.begin(),
                   ::toupper);
    langDef.mKeywords.insert(upper_mnem);
  }

  // Define Registers as Known Identifiers
  for (int i = 0; i < 16; ++i) {
    langDef.mIdentifiers.insert(
        {std::string("R") + std::to_string(i), TextEditor::Identifier{}});
  }
  langDef.mIdentifiers.insert({"SP", TextEditor::Identifier{}});
  langDef.mIdentifiers.insert({"LR", TextEditor::Identifier{}});
  langDef.mIdentifiers.insert({"PC", TextEditor::Identifier{}});

  // Single Line Comments
  langDef.mSingleLineComment = ";";

  // Disable Default Preprocessor Logic
  langDef.mPreprocChar = '~';

  // Disable Multi-line comments by setting to a non-empty, unused sequence.
  langDef.mCommentStart = "/*";
  langDef.mCommentEnd = "*/";

  // Custom Tokenizer
  // This lambda replaces the default tokenizer. It mimics your sam32::Lexer logic.
  langDef.mTokenize = [](const char* in_begin, const char* in_end,
                         const char*& out_begin, const char*& out_end,
                         TextEditor::PaletteIndex& paletteIndex) -> bool {
    paletteIndex = TextEditor::PaletteIndex::Default;
    const char* p = in_begin;

    // Skip whitespace
    while (p < in_end && isspace((unsigned char)*p))
      p++;

    if (p == in_end) {
      out_begin = in_end;
      out_end = in_end;
      return true;
    }

    out_begin = p;

    // Strings ("...")
    if (*p == '"') {
      p++;
      while (p < in_end) {
        if (*p == '"') {
          p++;
          break;
        }
        if (*p == '\\' && p + 1 < in_end) {
          p += 2;  // skip escaped char
        } else {
          p++;
        }
      }
      out_end = p;
      paletteIndex = TextEditor::PaletteIndex::String;
      return true;
    }

    // Directive (.xxx)
    if (*p == '.') {
      p++;
      while (p < in_end && !isspace((unsigned char)*p) && *p != ',' &&
             *p != ':' && *p != ';') {
        p++;
      }
      out_end = p;
      paletteIndex = TextEditor::PaletteIndex::Preprocessor;
      return true;
    }

    // Immediate (#xxx)
    if (*p == '#') {
      p++;
      if (p < in_end && *p == '-')
        p++;  // Handle negative
      while (p < in_end && !isspace((unsigned char)*p) && *p != ',' &&
             *p != ':' && *p != ';') {
        p++;
      }
      out_end = p;
      paletteIndex = TextEditor::PaletteIndex::Number;
      return true;
    }

    // Number (Hex, Binary, Decimal, Negative)
    if (isdigit((unsigned char)*p) ||
        (*p == '-' && p + 1 < in_end && isdigit((unsigned char)p[1]))) {
      if (*p == '-')
        p++;
      if (p + 1 < in_end && *p == '0' && (p[1] == 'x' || p[1] == 'b')) {
        p += 2;
        while (p < in_end &&
               (isxdigit((unsigned char)*p) || *p == '0' || *p == '1'))
          p++;
      } else {
        while (p < in_end && isdigit((unsigned char)*p))
          p++;
      }
      out_end = p;
      paletteIndex = TextEditor::PaletteIndex::Number;
      return true;
    }

    // Identifier (and Mnemonic Modifiers like ADD.F)
    if (isalpha((unsigned char)*p) || *p == '_') {
      while (p < in_end && (isalnum((unsigned char)*p) || *p == '_'))
        p++;

      // Check for .suffix (e.g., ADD.F)
      if (p < in_end && *p == '.') {
        const char* tmp = p + 1;
        while (tmp < in_end && isalpha((unsigned char)*tmp))
          tmp++;
        if (tmp > p + 1 && (tmp >= in_end || isspace((unsigned char)*tmp) ||
                            *tmp == ',' || *tmp == ':' || *tmp == ';')) {
          p = tmp;
        }
      }
      out_end = p;
      paletteIndex = TextEditor::PaletteIndex::Identifier;
      return true;
    }

    // Punctuation
    if (*p == ',' || *p == ':') {
      p++;
      out_end = p;
      paletteIndex = TextEditor::PaletteIndex::Punctuation;
      return true;
    }

    // Fallback (CRITICAL: Must advance p by 1 to prevent infinite loops!)
    p++;
    out_end = p;
    paletteIndex = TextEditor::PaletteIndex::Default;
    return true;
  };

  return langDef;
}

namespace sam32::gui {

EmulatorGui::EmulatorGui(Emulator& emu) : emulator(emu) {
  window_size = ImGui::GetIO().DisplaySize;
  fmt::println("Window size: {}x{}", window_size.x, window_size.y);
}

void EmulatorGui::render() {
  emulator.breakpoints.clear();
  for (int line : editor.GetBreakpoints()) {
    for (const auto& row : lst) {
      size_t row_line =
          std::holds_alternative<sam32::Data>(row.parsed)
              ? std::get<sam32::Data>(row.parsed).line_number
              : std::get<sam32::ParsedInstruction>(row.parsed).line_number;
      if (row_line == (size_t)line) {
        emulator.add_breakpoint(row.address, [this]() { is_running = false; });
        break;  // Only map the first address for this line
      }
    }
  }

  if (is_running) {
    safe_tick();
  }

  window_size = ImGui::GetIO().DisplaySize;

  render_controls();
  render_editor();
  render_lst();
  render_registers();
  render_flags();
  render_instruction_details();
  render_watchlist();
  render_hex_dump();
  render_stack_visualizer();
  render_call_stack();
}

void EmulatorGui::safe_tick() {
  try {
    if (emulator.state.pc == step_over_breakpoint ||
        emulator.state.pc == step_out_breakpoint) {
      is_running = false;
      step_over_breakpoint = 0xFFFFFFFF;
      step_out_breakpoint = 0xFFFFFFFF;
      return;
    }
    emulator.tick();
  } catch (const std::exception& e) {
    fmt::println("Error during tick: {}", e.what());
    is_running = false;
  }
}

void EmulatorGui::render_controls() {
  ImGui::SetNextWindowPos({window_size.x * 0.5f, 0.0f}, ImGuiCond_Always,
                          ImVec2(0.5f, 0.0f));

  ImGui::Begin("Controls", nullptr,
               ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                   ImGuiWindowFlags_NoCollapse |
                   ImGuiWindowFlags_NoSavedSettings |
                   ImGuiWindowFlags_NoTitleBar);

  ImGui::BeginDisabled(emulator.history.empty());
  if (ImGui::Button(ICON_CI_DEBUG_STEP_BACK)) {
    emulator.undo();
  }
  ImGui::SameLine();
  ImGui::EndDisabled();

  if (is_running) {
    if (ImGui::Button(ICON_CI_DEBUG_PAUSE)) {
      is_running = false;
    }
  } else {
    if (ImGui::Button(ICON_CI_DEBUG_START)) {
      is_running = true;
    }
  }
  ImGui::SameLine();

  if (ImGui::Button(ICON_CI_DEBUG_STEP_INTO)) {
    safe_tick();
  }
  if (ImGui::IsItemHovered())
    ImGui::SetTooltip("Step Into");
  ImGui::SameLine();

  if (ImGui::Button(ICON_CI_DEBUG_STEP_OVER)) {
    // If current instruction is CALL, set step_over_breakpoint
    if (emulator.state.pc + 3 < emulator.state.memory.size()) {
      sam32::Instruction_t instr(emulator.state.read_word(emulator.state.pc));
      // CALL is M_B (0) with Rd = 30 and not R-branch (operation < 16)
      if (instr.category == 1 && instr.operation == 0 && instr.rd == 30) {
        step_over_breakpoint = emulator.state.pc + 4;
        is_running = true;
      } else {
        safe_tick();  // regular step
      }
    } else {
      safe_tick();
    }
  }
  if (ImGui::IsItemHovered())
    ImGui::SetTooltip("Step Over");
  ImGui::SameLine();

  if (ImGui::Button(ICON_CI_DEBUG_STEP_OUT)) {
    step_out_breakpoint = emulator.state.get_reg(30);
    is_running = true;
  }
  if (ImGui::IsItemHovered())
    ImGui::SetTooltip("Step Out");

  ImGui::Separator();
  if (ImGui::Button(ICON_CI_SAVE " Save State")) {
    std::ofstream out("state.sav", std::ios::binary);
    out.write(reinterpret_cast<const char*>(&emulator.state.registers),
              sizeof(emulator.state.registers));
    out.write(reinterpret_cast<const char*>(&emulator.state.pc),
              sizeof(emulator.state.pc));
    uint32_t mem_size = emulator.state.memory.size();
    out.write(reinterpret_cast<const char*>(&mem_size), sizeof(mem_size));
    out.write(reinterpret_cast<const char*>(emulator.state.memory.data()),
              mem_size);
  }
  ImGui::SameLine();
  if (ImGui::Button(ICON_CI_FOLDER_OPENED " Load State")) {
    std::ifstream in("state.sav", std::ios::binary);
    if (in.is_open()) {
      in.read(reinterpret_cast<char*>(&emulator.state.registers),
              sizeof(emulator.state.registers));
      in.read(reinterpret_cast<char*>(&emulator.state.pc),
              sizeof(emulator.state.pc));
      uint32_t mem_size = 0;
      in.read(reinterpret_cast<char*>(&mem_size), sizeof(mem_size));
      if (mem_size == emulator.state.memory.size()) {
        in.read(reinterpret_cast<char*>(emulator.state.memory.data()),
                mem_size);
      }
      emulator.history.clear();  // invalidate history
    }
  }

  ImGui::End();
}

void EmulatorGui::render_editor() {
  if (!te_initialized) {
    editor.SetLanguageDefinition(GetSam32LanguageDefinition());

    auto palette = TextEditor::GetDarkPalette();
    palette[(int)TextEditor::PaletteIndex::Default] =
        IM_COL32(192, 202, 245, 255);
    palette[(int)TextEditor::PaletteIndex::Keyword] =
        IM_COL32(187, 154, 247, 255);
    palette[(int)TextEditor::PaletteIndex::Number] =
        IM_COL32(255, 158, 100, 255);
    palette[(int)TextEditor::PaletteIndex::String] =
        IM_COL32(158, 206, 106, 255);
    palette[(int)TextEditor::PaletteIndex::CharLiteral] =
        IM_COL32(158, 206, 106, 255);
    palette[(int)TextEditor::PaletteIndex::Punctuation] =
        IM_COL32(192, 202, 245, 255);
    palette[(int)TextEditor::PaletteIndex::Preprocessor] =
        IM_COL32(125, 207, 255, 255);
    palette[(int)TextEditor::PaletteIndex::Identifier] =
        IM_COL32(122, 162, 247, 255);
    palette[(int)TextEditor::PaletteIndex::KnownIdentifier] =
        IM_COL32(122, 162, 247, 255);
    palette[(int)TextEditor::PaletteIndex::PreprocIdentifier] =
        IM_COL32(125, 207, 255, 255);
    palette[(int)TextEditor::PaletteIndex::Comment] =
        IM_COL32(86, 95, 137, 255);
    palette[(int)TextEditor::PaletteIndex::MultiLineComment] =
        IM_COL32(86, 95, 137, 255);
    palette[(int)TextEditor::PaletteIndex::Background] =
        IM_COL32(26, 27, 38, 255);
    palette[(int)TextEditor::PaletteIndex::Cursor] =
        IM_COL32(192, 202, 245, 255);
    palette[(int)TextEditor::PaletteIndex::Selection] =
        IM_COL32(86, 95, 137, 128);
    palette[(int)TextEditor::PaletteIndex::ErrorMarker] =
        IM_COL32(247, 118, 142, 160);
    palette[(int)TextEditor::PaletteIndex::Breakpoint] =
        IM_COL32(247, 118, 142, 255);
    palette[(int)TextEditor::PaletteIndex::LineNumber] =
        IM_COL32(86, 95, 137, 255);
    palette[(int)TextEditor::PaletteIndex::CurrentLineFill] =
        IM_COL32(36, 40, 59, 255);
    palette[(int)TextEditor::PaletteIndex::CurrentLineFillInactive] =
        IM_COL32(26, 27, 38, 255);
    palette[(int)TextEditor::PaletteIndex::CurrentLineEdge] =
        IM_COL32(122, 162, 247, 160);

    editor.SetPalette(palette);

    editor.SetText(
        "; SAM32 ISA Demo\nLI R10, 0x2000\nMOV R1, #5\nMOV R2, #0\nloop:\nADD "
        "R3, R2, #42\nSW R3, R10, #0\nADD R10, R10, #4\nADD R2, R2, #1\nSUB "
        "R1, R1, #1\nBNE R0, loop\nend:\nB R0, end");
    ImGui::SetNextWindowFocus();
    te_initialized = true;
  }

  ImGui::SetNextWindowPos(ImVec2(17, 54), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize(ImVec2(388, 929), ImGuiCond_FirstUseEver);
  ImGui::Begin("SAM32 Code Editor");

  // File loading dialog
  if (ImGui::Button("Load from File")) {
    IGFD::FileDialogConfig config;
    config.path = ".";
    ImGuiFileDialog::Instance()->OpenDialog("ChooseFileDlgKey", "Choose File",
                                            ".asm", config);
  }
  if (ImGuiFileDialog::Instance()->Display("ChooseFileDlgKey")) {
    if (ImGuiFileDialog::Instance()->IsOk()) {
      std::string filePath = ImGuiFileDialog::Instance()->GetFilePathName();
      std::string fileContent = read_file(filePath);
      if (fileContent != "") {
        editor.SetText(fileContent);
      } else {
        fmt::println("Failed to read file: {}", filePath);
      }
    }
    ImGuiFileDialog::Instance()->Close();
  }
  ImGui::SameLine();

  // Compile
  if (ImGui::Button("Compile & Load")) {
    std::string code = editor.GetText();
    try {
      auto compiled = Assemble(editor.GetText(), emulator.allow_placeholders);
      machine_code = compiled.first;
      lst = compiled.second;

      emulator.load_program(machine_code);

      fmt::println("Code compiled and loaded successfully!");
    } catch (const std::exception& e) {
      fmt::println("Compilation error: {}", e.what());
    }
  }

  editor.Render("Sam32Editor", ImGui::GetContentRegionAvail());
  ImGui::End();
}

static std::string format_operand(const sam32::Operand& op) {
  if (op.type == sam32::OperandType::REGISTER)
    return "R" + std::to_string(op.reg_id);
  if (op.type == sam32::OperandType::IMMEDIATE) {
    if (op.is_offset)
      return std::to_string(static_cast<int32_t>(op.value));
    return "#" + std::to_string(static_cast<int32_t>(op.value));
  }
  if (op.type == sam32::OperandType::LABEL)
    return op.label;
  return "";
}

static std::string format_parsed(
    const std::variant<sam32::ParsedInstruction, sam32::Data>& parsed) {
  if (std::holds_alternative<sam32::Data>(parsed)) {
    const auto& data = std::get<sam32::Data>(parsed);
    if (data.value.type == sam32::OperandType::LABEL) {
      return ".string \"" + data.value.label + "\"";
    }
    if (data.size == 1)
      return ".byte " + format_operand(data.value);
    if (data.size == 2)
      return ".half " + format_operand(data.value);
    return ".word " + format_operand(data.value);
  } else {
    const auto& pi = std::get<sam32::ParsedInstruction>(parsed);
    std::string res = sam32::mnemonic_names[static_cast<int>(pi.mnemonic)];
    if (pi.freeze_flag)
      res += ".F";
    res += " ";
    for (size_t i = 0; i < pi.operands.size(); ++i) {
      if (i > 0)
        res += ", ";
      res += format_operand(pi.operands[i]);
    }
    if (pi.has_shift) {
      res += ", ";
      if (pi.shift_type == sam32::SHIFT_LSL)
        res += "LSL";
      else if (pi.shift_type == sam32::SHIFT_LSR)
        res += "LSR";
      else if (pi.shift_type == sam32::SHIFT_ASR)
        res += "ASR";
      else if (pi.shift_type == sam32::SHIFT_ROR)
        res += "ROR";
      res += " #" + std::to_string(pi.shift_amt);
    }
    return res;
  }
}

void EmulatorGui::render_lst() {
  ImGui::SetNextWindowPos(ImVec2(445, 81), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize(ImVec2(634, 484), ImGuiCond_FirstUseEver);
  ImGui::Begin("Instruction List (LST)");
  ImGui::Text("Cycle Count: %zu", emulator.cycle_count);
  if (ImGui::BeginTable("LST Table", 5,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                            ImGuiTableFlags_ScrollY)) {
    ImGui::TableSetupColumn("Address", ImGuiTableColumnFlags_WidthFixed, 80.0f);
    ImGui::TableSetupColumn("Machine Code", ImGuiTableColumnFlags_WidthFixed,
                            100.0f);
    ImGui::TableSetupColumn("Line #", ImGuiTableColumnFlags_WidthFixed, 60.0f);
    ImGui::TableSetupColumn("Heat", ImGuiTableColumnFlags_WidthFixed, 50.0f);
    ImGui::TableSetupColumn("Assembly");
    ImGui::TableHeadersRow();

    for (const auto& row : lst) {
      ImGui::TableNextRow();

      bool is_current_pc = (row.address == emulator.state.pc);
      if (is_current_pc) {
        ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,
                               IM_COL32(122, 162, 247, 100));
      }

      ImGui::TableSetColumnIndex(0);
      if (emulator.breakpoints.count(row.address)) {
        ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f),
                           " " ICON_CI_DEBUG_BREAKPOINT " 0x%08X",
                           (unsigned int)row.address);
      } else {
        ImGui::Text("   0x%08X", (unsigned int)row.address);
      }

      ImGui::TableSetColumnIndex(1);
      ImGui::Text("0x%08X", (unsigned int)row.instruction);

      ImGui::TableSetColumnIndex(2);
      size_t line =
          std::holds_alternative<sam32::Data>(row.parsed)
              ? std::get<sam32::Data>(row.parsed).line_number
              : std::get<sam32::ParsedInstruction>(row.parsed).line_number;
      ImGui::Text("%zu", line);

      ImGui::TableSetColumnIndex(3);
      if (emulator.state.heatmap.count(row.address) &&
          emulator.state.heatmap.at(row.address) > 0) {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.2f, 1.0f), "%u",
                           emulator.state.heatmap.at(row.address));
      } else {
        ImGui::Text(" ");
      }

      ImGui::TableSetColumnIndex(4);
      ImGui::Text("%s", format_parsed(row.parsed).c_str());
    }
    ImGui::EndTable();
  }
  ImGui::End();
}

void EmulatorGui::render_registers() {
  ImGui::SetNextWindowPos(ImVec2(1459, 6), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize(ImVec2(457, 288), ImGuiCond_FirstUseEver);
  ImGui::Begin("Registers");

  if (emulator.state.last_execution.executed &&
      emulator.state.pc != last_pc_flashed) {
    if (emulator.state.last_execution.writes_to_register &&
        emulator.state.last_execution.written_register == 0) {
      r0_flash_timer = ImGui::GetTime();
    }
    last_pc_flashed = emulator.state.pc;
  }

  // Radix selection
  ImGui::RadioButton("Hex", &display_radix, 16);
  ImGui::SameLine();
  ImGui::RadioButton("Signed", &display_radix, 10);
  ImGui::SameLine();
  ImGui::RadioButton("Unsigned", &display_radix, -10);
  ImGui::SameLine();
  ImGui::RadioButton("Binary", &display_radix, 2);

  if (ImGui::GetTime() - r0_flash_timer < 0.2f) {
    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f),
                       ">>> R0 BIT-BUCKET ROUTED! <<<");
  } else {
    ImGui::Text(" ");
  }

  if (ImGui::BeginTable("RegTable", 4, ImGuiTableFlags_Borders)) {
    for (int i = 0; i < 32; ++i) {
      ImGui::TableNextColumn();

      bool changed = false;
      if (!emulator.history.empty()) {
        if (emulator.history.back().registers[i] !=
            emulator.state.registers[i]) {
          changed = true;
        }
      }

      uint32_t val = emulator.state.registers[i];
      std::string val_str;
      if (display_radix == 16)
        val_str = fmt::format("0x%08X", val);
      else if (display_radix == 10)
        val_str = std::to_string(static_cast<int32_t>(val));
      else if (display_radix == -10)
        val_str = std::to_string(val);
      else {
        val_str = "0b";
        for (int b = 31; b >= 0; --b)
          val_str += ((val >> b) & 1) ? "1" : "0";
      }

      if (i == 0) {
        ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg,
                               IM_COL32(100, 100, 100, 255));
      } else if (i == 30) {
        ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg,
                               IM_COL32(150, 100, 100, 255));
      } else if (i == 31) {
        ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg,
                               IM_COL32(100, 150, 100, 255));
      }

      if (changed) {
        ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "R%d: %s", i,
                           val_str.c_str());
      } else {
        ImGui::Text("R%d: %s", i, val_str.c_str());
      }
    }
    ImGui::EndTable();
  }
  ImGui::End();
}

void EmulatorGui::render_flags() {
  ImGui::SetNextWindowPos(ImVec2(1109, 298), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize(ImVec2(162, 134), ImGuiCond_FirstUseEver);
  ImGui::Begin("Status Flags");

  bool is_frozen = false;
  if (emulator.state.pc < emulator.state.memory.size() - 3) {
    uint32_t next_raw = emulator.state.read_word(emulator.state.pc);
    sam32::Instruction_t next_instr(next_raw);
    is_frozen = next_instr.F;
  }

  if (is_frozen) {
    ImGui::TextColored(ImVec4(0.5f, 0.5f, 1.0f, 1.0f),
                       "[FROZEN - FLAGS LOCKED]");
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(100, 100, 100, 255));
  }

  ImGui::Text("Z (Zero):      %s", emulator.state.zero_flag ? "O" : ".");
  ImGui::Text("N (Negative):  %s", emulator.state.negative_flag ? "O" : ".");
  ImGui::Text("C (Carry):     %s", emulator.state.carry_flag ? "O" : ".");
  ImGui::Text("V (Overflow):  %s", emulator.state.overflow_flag ? "O" : ".");

  if (is_frozen) {
    ImGui::PopStyleColor();
  }
  ImGui::End();
}

void EmulatorGui::render_instruction_details() {
  ImGui::SetNextWindowPos(ImVec2(1099, 9), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize(ImVec2(357, 271), ImGuiCond_FirstUseEver);
  ImGui::Begin("Instruction Details");
  if (emulator.state.last_execution.executed) {
    ImGui::Text("Raw Instruction: 0x%08X",
                emulator.state.last_execution.raw_instruction);
    ImGui::Separator();
    ImGui::Text("Operand 1: 0x%08X", emulator.state.last_execution.operand1);
    ImGui::Text("Operand 2: 0x%08X", emulator.state.last_execution.operand2);
    ImGui::Text("ALU Output: 0x%08X", emulator.state.last_execution.alu_output);
    ImGui::Separator();
    if (emulator.state.last_execution.writes_to_register) {
      ImGui::Text("Modified Reg: R%d",
                  emulator.state.last_execution.written_register);
    }
    if (emulator.state.last_execution.writes_to_memory) {
      ImGui::Text("Written Memory: [0x%08X] = 0x%08X",
                  emulator.state.last_execution.memory_address_written,
                  emulator.state.last_execution.memory_value_written);
    }
  } else {
    ImGui::Text("No instructions executed yet.");
  }
  ImGui::End();
}

void EmulatorGui::render_watchlist() {
  ImGui::SetNextWindowPos(ImVec2(1331, 634), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize(ImVec2(577, 361), ImGuiCond_FirstUseEver);
  ImGui::Begin("Watchlist");

  static char input_buf[32] = "";
  ImGui::InputText("Address/Reg", input_buf, sizeof(input_buf));
  ImGui::SameLine();
  if (ImGui::Button("Add")) {
    std::string s = input_buf;
    if (!s.empty()) {
      if (s[0] == 'r' || s[0] == 'R') {
        WatchItem item;
        item.is_register = true;
        item.address_or_index = std::stoi(s.substr(1));
        watchlist.push_back(item);
      } else {
        WatchItem item;
        item.is_register = false;
        item.address_or_index = std::stoul(s, nullptr, 0);
        watchlist.push_back(item);
      }
    }
  }

  if (ImGui::BeginTable("WatchTable", 3, ImGuiTableFlags_Borders)) {
    ImGui::TableSetupColumn("Type");
    ImGui::TableSetupColumn("Target");
    ImGui::TableSetupColumn("Value");
    ImGui::TableHeadersRow();

    for (const auto& item : watchlist) {
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::Text("%s", item.is_register ? "Register" : "Memory");

      ImGui::TableSetColumnIndex(1);
      if (item.is_register)
        ImGui::Text("R%d", item.address_or_index);
      else
        ImGui::Text("0x%08X", item.address_or_index);

      ImGui::TableSetColumnIndex(2);
      if (item.is_register) {
        if (item.address_or_index < 32) {
          uint32_t val = emulator.state.get_reg(item.address_or_index);
          ImGui::Text("0x%08X", val);
        } else {
          ImGui::Text("Invalid Reg");
        }
      } else {
        if (item.address_or_index + 3 < emulator.state.memory.size()) {
          uint32_t val = emulator.state.read_word(item.address_or_index);
          ImGui::Text("0x%08X", val);
        } else {
          ImGui::Text("Out of Bounds");
        }
      }
    }
    ImGui::EndTable();
  }
  ImGui::End();
}

void EmulatorGui::render_hex_dump() {
  ImGui::SetNextWindowPos(ImVec2(417, 596), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize(ImVec2(873, 379), ImGuiCond_FirstUseEver);
  ImGui::Begin("Hex Dump");
  const int bytes_per_line = 16;
  size_t total_lines =
      (emulator.state.memory.size() + bytes_per_line - 1) / bytes_per_line;

  ImGuiListClipper clipper;
  clipper.Begin(total_lines);
  while (clipper.Step()) {
    for (int line = clipper.DisplayStart; line < clipper.DisplayEnd; line++) {
      size_t addr = line * bytes_per_line;
      ImGui::Text("%08X: ", (unsigned int)addr);

      for (int i = 0; i < bytes_per_line; i++) {
        ImGui::SameLine();
        if (addr + i < emulator.state.memory.size()) {
          ImGui::Text("%02X", emulator.state.memory[addr + i]);
          if (ImGui::IsItemHovered() &&
              ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
            uint32_t right_clicked_address = addr + i;
            emulator.watchpoints[right_clicked_address] = [this]() {
              is_running = false;
            };
          }
        } else {
          ImGui::Text("  ");
        }
      }
      ImGui::SameLine();
      ImGui::Text(" | ");
      for (int i = 0; i < bytes_per_line; i++) {
        ImGui::SameLine();
        if (addr + i < emulator.state.memory.size()) {
          char c = emulator.state.memory[addr + i];
          char buf[2] = {(char)((c >= 32 && c < 127) ? c : '.'), '\0'};
          ImGui::Text("%s", buf);
        }
      }
      ImGui::NewLine();
    }
  }
  ImGui::End();
}

void EmulatorGui::render_stack_visualizer() {
  ImGui::SetNextWindowPos(ImVec2(1293, 298), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize(ImVec2(299, 335), ImGuiCond_FirstUseEver);
  ImGui::Begin("Stack Visualizer");
  uint32_t sp = emulator.state.get_reg(31);
  ImGui::Text("Stack Pointer (R31): 0x%08X", sp);
  ImGui::Separator();
  if (ImGui::BeginTable("StackTable", 2, ImGuiTableFlags_Borders)) {
    ImGui::TableSetupColumn("Address");
    ImGui::TableSetupColumn("Value");
    ImGui::TableHeadersRow();

    // Show SP to SP+40 (10 words)
    for (int i = 0; i < 10; i++) {
      ImGui::TableNextRow();
      uint32_t addr = sp + (i * 4);
      ImGui::TableSetColumnIndex(0);
      if (addr == sp) {
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.2f, 1.0f), "-> 0x%08X", addr);
      } else {
        ImGui::Text("   0x%08X", addr);
      }
      ImGui::TableSetColumnIndex(1);
      if (addr + 3 < emulator.state.memory.size()) {
        ImGui::Text("0x%08X", emulator.state.read_word(addr));
      } else {
        ImGui::Text("Out of bounds");
      }
    }
    ImGui::EndTable();
  }
  ImGui::End();
}

void EmulatorGui::render_call_stack() {
  ImGui::SetNextWindowPos(ImVec2(1586, 303), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize(ImVec2(321, 264), ImGuiCond_FirstUseEver);
  ImGui::Begin("Call Stack");
  if (ImGui::BeginTable("CallStackTable", 2, ImGuiTableFlags_Borders)) {
    ImGui::TableSetupColumn("Depth");
    ImGui::TableSetupColumn("Return Address");
    ImGui::TableHeadersRow();

    for (size_t i = 0; i < emulator.state.call_stack.size(); i++) {
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::Text("%zu", i);
      ImGui::TableSetColumnIndex(1);
      ImGui::Text("0x%08X", emulator.state.call_stack[i]);
    }
    ImGui::EndTable();
  }
  ImGui::End();
}
}  // namespace sam32::gui
