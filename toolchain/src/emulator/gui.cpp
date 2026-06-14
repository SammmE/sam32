#include <fmt/core.h>
#include <imgui.h>

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
  if (is_running) {
    safe_tick();
  }

  window_size = ImGui::GetIO().DisplaySize;
  ImGui::Begin("SAM32 Emulator");
  ImGui::Text("Welcome to the SAM32 Emulator!");
  ImGui::End();

  render_controls();
  render_editor();
}

void EmulatorGui::safe_tick() {
  try {
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

  if (ImGui::Button(ICON_CI_DEBUG_STEP_OVER)) {
    safe_tick();
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
        ".data\nmy_string: .string \"Hello, SAM32!\"\n\n.text\nglobal "
        "_start\n\n_start:\n  mov r0, #5 ; Load immediate\n  add.F r1, r0, #10 "
        "; Freeze flag modifier\n  halt");
    ImGui::SetNextWindowFocus();
    te_initialized = true;
  }

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

  // Compile
  if (ImGui::Button("Compile & Load")) {
    std::string code = editor.GetText();
    try {
      auto compiled = Assemble(editor.GetText());
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

}  // namespace sam32::gui
