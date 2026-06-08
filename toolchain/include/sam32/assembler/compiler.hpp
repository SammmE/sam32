#pragma once
#include <cstdint>
#include <sam32/assembler/lexer.hpp>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace sam32 {

constexpr uint32_t CAT_ALU = 0;     // 00
constexpr uint32_t CAT_BRANCH = 1;  // 01
constexpr uint32_t CAT_MEM = 2;     // 10
constexpr uint32_t PSEUDO = 0xFFFFFFFF;

struct Data {
  size_t addr_offset;
  uint32_t value;

  Data(size_t addr_offset, uint32_t value)
      : addr_offset(addr_offset), value(value) {}
};

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
  size_t addr_offset;

  size_t line_number;

  Instruction(std::vector<Token> tokens, size_t line_number,
              size_t addr_offset);
  void resolve_labels(
      const std::unordered_map<std::string, size_t>& label_addresses_exact,
      size_t current_pc, const std::vector<size_t>& org_start_addresses);
  Data encode() const;
};

struct Org {
  size_t start_address;
  std::vector<std::variant<Instruction, Data>> data;
};

std::vector<uint32_t> compile_instructions(const std::vector<Org>& orgs,
                                           bool verbose = false);
}  // namespace sam32