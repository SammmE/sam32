#include <fmt/base.h>
#include <fmt/core.h>
#include <CLI/CLI.hpp>
#include <filesystem>
#include <string>

#include <indicators/block_progress_bar.hpp>
#include <indicators/cursor_control.hpp>

#include <sam32/assembler/lexer.hpp>
#include <sam32/assembler/parser.hpp>

int main(int argc, char** argv) {
  CLI::App app;

  auto compile = app.add_subcommand("compile", "Compile file");

  std::string file_path;
  std::string output_path;
  bool verbose = false;
  compile
      ->add_option("file", file_path,
                   "Assembly file to compile into machine code")
      ->required();
  compile->add_option("-o,--output", output_path,
                      "Output file for the compiled machine code");
  compile->add_flag("-v,--verbose", verbose,
                    "Verbose output during compilation");

  compile->callback([&]() {
    if (!std::filesystem::exists(file_path)) {
      fmt::println("{} does not exist", file_path);
      return;
    }

    if (verbose) {
      fmt::println("Starting compilation of {}", file_path);
      indicators::show_console_cursor(false);
    }

    sam32::Lexer lexer(file_path);
    lexer.tokenize();

    if (verbose) fmt::println("Lexed {} tokens", lexer.tokens.size());

    sam32::Parser parser(lexer.tokens);
    parser.parse();

    if (verbose) {
      fmt::println("Parsed {} text instructions, {} data items",
                   parser.text_segment.instructions.size(),
                   parser.data_segment.instructions.size());
    }

    // TODO: compiler (second pass + binary emission) not yet implemented.
    std::vector<uint32_t> machine_code;

    if (output_path.empty()) {
      output_path = file_path + ".bin";
    }

    indicators::BlockProgressBar bar{
        indicators::option::BarWidth{80},
        indicators::option::Start{"["},
        indicators::option::End{"]"},
        indicators::option::ForegroundColor{indicators::Color::white},
        indicators::option::ShowPercentage{true},
        indicators::option::FontStyles{
            std::vector<indicators::FontStyle>{indicators::FontStyle::bold}}};

    std::ofstream output_file(output_path, std::ios::binary);
    for (uint32_t instr : machine_code) {
      output_file.write(reinterpret_cast<const char*>(&instr), sizeof(instr));

      if (verbose) {
        bar.set_progress((output_file.tellp() * 100) /
                         (machine_code.size() * sizeof(uint32_t)));
      }
    }
    output_file.close();
    fmt::println("Compiled machine code written to {}", output_path);
  });

  CLI11_PARSE(app, argc, argv);
  return 0;
}
