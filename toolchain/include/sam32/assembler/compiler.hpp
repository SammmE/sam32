#pragma once
#include <cstdint>
#include <unordered_map>
#include <vector>
#include <sam32/assembler/lexer.hpp>

namespace sam32 {

constexpr uint32_t CAT_ALU = 0;     // 00
constexpr uint32_t CAT_BRANCH = 1;  // 01
constexpr uint32_t CAT_MEM = 2;     // 10
constexpr uint32_t PSEUDO = 0xFFFFFFFF;

struct Instruction {
public:
  Token mnemonic;
  Token Rd;
  Token Rs1;
  Token Rs2;
  Token imm;
  Token offset;
  uint8_t shift_amount;
  uint8_t shift_type;  // 0 = LSL, 1 = LSR, 2 = ASR
  bool freeze;

  size_t line_number;

  Instruction(std::vector<Token> tokens, size_t line_number);
  void resolve_labels(
      const std::unordered_map<std::string, size_t>& label_addresses,
      size_t current_pc);
  uint32_t encode() const;
};

std::vector<uint32_t> compile_instructions(
    const std::vector<Instruction>& instructions, bool verbose = false);
}  // namespace sam32