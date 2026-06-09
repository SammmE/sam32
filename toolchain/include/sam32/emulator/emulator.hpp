#pragma once

#include <cstdint>
#include <vector>

namespace sam32 {
uint32_t extract_bits(uint32_t value, size_t start, size_t length = 1);
uint32_t sign_extend(uint32_t value, size_t bits);
uint32_t apply_barrel_shift(uint32_t value, uint8_t shift_type,
                            uint8_t shift_amount);

struct Instruction_t {
  uint32_t raw;  // The raw 32-bit instruction

  // Decoded fields
  uint8_t opcode;        // 7 bits
  bool is_rs1_imm;       // 1 bit
  bool is_rs2_imm;       // 1 bit
  uint8_t rd;            // 5 bits
  uint8_t rs1;           // 5 bits
  uint8_t rs2;           // 5 bits
  uint16_t imm;          // 16 bits (immediate and offset)
  uint8_t shift_type;    // 2 bits
  uint8_t shift_amount;  // 5 bits
  bool F;                // 1 bit (Freeze status)

  Instruction_t(uint32_t instruction);
};

struct Emulator {
 public:
  uint32_t registers[32];  // r0-r31
  uint32_t pc;             // program counter
  bool zero_flag;
  bool negative_flag;
  bool carry_flag;
  bool overflow_flag;

  std::vector<uint8_t> memory;

  Emulator(size_t memory_size = 1024 * 1024);  // Default to 1MB of memory

  void load_program(const std::vector<uint32_t>& program,
                    size_t start_address = 0);
  void tick();

 private:
  void execute_instruction(uint32_t instruction);

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

  // ALU
  uint32_t count_leading_zeros(uint32_t value);
  bool eval_branch_condition(uint8_t condition_code);
};
}  // namespace sam32