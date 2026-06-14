#pragma once

#include <cstdint>
#include <functional>
#include <unordered_map>
#include <vector>
#include "sam32/assembler/parser.hpp"

namespace sam32 {
uint32_t extract_bits(uint32_t value, size_t start, size_t length = 1);
uint32_t sign_extend(uint32_t value, size_t bits);
uint32_t apply_barrel_shift(uint32_t value, uint8_t shift_type,
                            uint8_t shift_amount);

struct Instruction_t {
  uint32_t raw;  // The raw 32-bit instruction

  // Decoded fields
  uint8_t category;      // 2 bits [31:30]
  uint8_t operation;     // 5 bits [29:25]
  bool is_rs2_imm;       // 1 bit  [24]
  bool is_rs1_imm;       // 1 bit  [23]
  uint8_t rd;            // 5 bits [22:18]
  uint8_t rs1;           // 5 bits [17:13]
  uint8_t rs2;           // 5 bits [12:8] or [4:0]
  
  uint16_t imm;          // 13 bits (I-Type)
  uint32_t branch_offset;// 18 bits (J-Type)
  
  uint8_t shift_type;    // 2 bits [7:6]
  uint8_t shift_amount;  // 5 bits [5:1]
  bool F;                // 1 bit  [0] (Freeze status)

  Instruction_t(uint32_t instruction);
};

struct EmulatorState {
  uint32_t registers[32];  // r0-r31
  uint32_t pc;             // program counter
  bool zero_flag;
  bool negative_flag;
  bool carry_flag;
  bool overflow_flag;

  std::vector<uint8_t> memory;

  EmulatorState(size_t memory_size) : memory(memory_size, 0) {}

  // registers
  uint32_t get_reg(size_t index);
  void set_reg(size_t index, uint32_t value);

  // memory
  uint8_t read_byte(size_t address);
  uint16_t read_halfword(size_t address);
  uint32_t read_word(size_t address);

  void write_byte(size_t address, uint8_t value);
  void write_halfword(size_t address, uint16_t value);
  void write_word(size_t address, uint32_t value);
};

struct Emulator {
 public:
  EmulatorState state;
  std::vector<EmulatorState> history;
  std::unordered_map<size_t, std::function<void()>> breakpoints;
  size_t cycle_count = 0;

  Emulator(size_t memory_size = 1024 * 1024);  // Default to 1MB of memory

  void load_program(const std::vector<uint8_t>& program,
                    size_t start_address = 0);
  void tick();
  void undo();
  void reset();

  void add_breakpoint(size_t address, std::function<void()> callback);

 private:
  void execute_instruction(uint32_t instruction);

  // ALU
  uint32_t count_leading_zeros(uint32_t value);
  bool eval_branch_condition(uint8_t condition_code);
};
}  // namespace sam32