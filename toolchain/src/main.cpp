#include <fmt/base.h>
#include <fmt/core.h>
#include <CLI/CLI.hpp>
#include <filesystem>
#include <string>

#include <sam32/assembler/lexer.hpp>
#include <sam32/assembler/parser.hpp>
#include "sam32/assembler/compiler.hpp"

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

    sam32::Lexer lexer(file_path);
    lexer.tokenize();

    if (verbose)
      fmt::println("Lexed {} tokens", lexer.tokens.size());

    sam32::Parser parser(lexer.tokens);
    parser.parse();

    if (verbose) {
      fmt::println("Parsed {} text instructions, {} data items",
                   parser.text_segment.instructions.size(),
                   parser.data_segment.instructions.size());
    }

    std::vector<uint8_t> machine_code =
        sam32::encode(parser.text_segment, parser.data_segment);

    if (output_path.empty()) {
      output_path = file_path.substr(0, file_path.find_last_of('.')) + ".bin";
    }

    std::ofstream output_file(output_path, std::ios::binary);
    for (uint8_t byte : machine_code) {
      output_file.put(byte);
    }
    output_file.close();
    fmt::println("Compiled machine code written to {}", output_path);
  });

  CLI11_PARSE(app, argc, argv);
  return 0;
}
