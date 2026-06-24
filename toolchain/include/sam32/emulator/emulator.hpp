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
  uint8_t category;      // 2 bits [1:0]
  uint8_t operation;     // 5 bits [6:2]
  bool is_rs1_imm;       // 1 bit  [7]
  bool is_rs2_imm;       // 1 bit  [8]
  uint8_t rd;            // 5 bits [13:9]
  uint8_t rs1;           // 5 bits [18:14] or [13:9] in R-Branch
  uint8_t rs2;           // 5 bits [23:19] or [31:27]
  
  uint16_t imm;          // 13 bits (I-Type)
  uint32_t branch_offset;// 18 bits (J-Type)
  
  uint8_t shift_type;    // 2 bits [25:24]
  uint8_t shift_amount;  // 5 bits [30:26]
  bool F;                // 1 bit  [31] (Freeze status)

  Instruction_t(uint32_t instruction);
};

struct InstructionExecutionDetails {
  uint32_t raw_instruction = 0;
  uint32_t pc = 0;
  uint32_t operand1 = 0;
  uint32_t operand2 = 0;
  uint32_t alu_output = 0;
  bool writes_to_register = false;
  uint8_t written_register = 0;
  bool writes_to_memory = false;
  uint32_t memory_address_written = 0;
  uint32_t memory_value_written = 0;
  bool executed = false;
};

struct EmulatorState {
  uint32_t registers[32];  // r0-r31
  uint32_t pc;             // program counter
  bool zero_flag;
  bool negative_flag;
  bool carry_flag;
  bool overflow_flag;

  uint32_t epc = 0;
  uint32_t cause = 0;
  uint32_t tvec = 0;
  bool gie = false;
  uint32_t mtime = 0;
  uint32_t mtimecmp = 0;

  std::vector<uint8_t> memory;
  InstructionExecutionDetails last_execution;
  std::vector<uint32_t> call_stack;
  std::unordered_map<uint32_t, uint32_t> heatmap;

  EmulatorState(size_t memory_size) : memory(memory_size, 0) {
    for (int i = 0; i < 32; ++i) registers[i] = 0;
    pc = 0;
    zero_flag = negative_flag = carry_flag = overflow_flag = false;
  }

  // registers
  uint32_t get_reg(size_t index);
  void set_reg(size_t index, uint32_t value);
  uint32_t read_csr(uint8_t address);
  void write_csr(uint8_t address, uint32_t value);

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
  std::unordered_map<size_t, std::function<void()>> watchpoints;
  size_t cycle_count = 0;
  bool allow_placeholders;

  Emulator(size_t memory_size = 1024 * 1024, bool allow_placeholders = false);  // Default to 1MB of memory

  void load_program(const std::vector<uint8_t>& program,
                    size_t start_address = 0);
  void tick();
  void undo();
  void reset();
  void trigger_trap(uint32_t trap_cause);

  void add_breakpoint(size_t address, std::function<void()> callback);

 private:
  void execute_instruction(uint32_t instruction);

  // ALU
  uint32_t count_leading_zeros(uint32_t value);
  bool eval_branch_condition(uint8_t condition_code);
};
}  // namespace sam32