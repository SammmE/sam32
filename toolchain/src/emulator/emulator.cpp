#include <sam32/emulator/emulator.hpp>
#include <stdexcept>

namespace sam32 {
uint32_t extract_bits(uint32_t value, size_t start, size_t length) {
  return (value >> start) & ((1u << length) - 1);
}

uint32_t sign_extend(uint32_t value, size_t bits) {
  uint32_t mask = 1u << (bits - 1);
  return (value ^ mask) - mask;
}

uint32_t apply_barrel_shift(uint32_t value, uint8_t shift_type,
                            uint8_t shift_amount) {
  if (shift_amount == 0)
    return value;  // No shift

  switch (shift_type) {
    case 0:  // Logical left
      return value << shift_amount;
    case 1:  // Logical right
      return value >> shift_amount;
    case 2:  // Arithmetic right
      return static_cast<int32_t>(value) >> shift_amount;
    case 3:  // Rotate right
      if (shift_amount >= 32)
        shift_amount %= 32;

      return (value >> shift_amount) | (value << (32 - shift_amount));
    default:
      throw std::invalid_argument("Invalid shift type");
  }
}

Instruction_t::Instruction_t(uint32_t instruction) : raw(instruction) {
  opcode = extract_bits(instruction, 0, 7);
  is_rs1_imm = extract_bits(instruction, 7, 1);
  is_rs2_imm = extract_bits(instruction, 8, 1);
  rd = extract_bits(instruction, 9, 5);
  rs1 = extract_bits(instruction, 14, 5);
  rs2 = is_rs1_imm ? extract_bits(instruction, 27, 5)
                   : extract_bits(instruction, 19, 5);
  shift_type = extract_bits(instruction, 24, 2);
  shift_amount = extract_bits(instruction, 26, 5);
  F = false;

  if (is_rs1_imm && is_rs2_imm) {
    imm = sign_extend(extract_bits(instruction, 14, 18), 18);
  } else if (is_rs1_imm) {
    imm = sign_extend(extract_bits(instruction, 14, 13), 13);
  } else if (is_rs2_imm) {
    imm = sign_extend(extract_bits(instruction, 19, 13), 13);
  } else {
    imm = 0;
    F = extract_bits(instruction, 31, 1);
  }
}

Emulator::Emulator(size_t memory_size)
    : pc(0),
      zero_flag(false),
      negative_flag(false),
      carry_flag(false),
      overflow_flag(false) {
  memory.resize(memory_size, 0);
}

void Emulator::load_program(const std::vector<uint32_t>& program,
                            size_t start_address) {
  for (size_t i = 0; i < program.size(); ++i) {
    write_word(start_address + i * 4, program[i]);
  }
}

void Emulator::tick() {
  uint32_t instruction = read_word(pc);
  execute_instruction(instruction);
}

void Emulator::execute_instruction(uint32_t instruction) {
  Instruction_t decoded = Instruction_t(instruction);
  bool modify = extract_bits(decoded.opcode, 6, 1);
  uint8_t operation = extract_bits(decoded.opcode, 2, 4);
  uint8_t category = extract_bits(decoded.opcode, 0, 2);

  uint32_t output_bus = 0;
  uint32_t operand1;
  uint32_t operand2;

  if (decoded.is_rs1_imm) {
    operand1 = decoded.imm;
    operand2 = get_reg(decoded.rs2);
  } else if (decoded.is_rs2_imm) {
    operand1 = get_reg(decoded.rs1);
    operand2 = decoded.imm;
  } else {
    operand1 = get_reg(decoded.rs1);
    operand2 = get_reg(decoded.rs2);
  }

  // use the category bits to determine the instruction type
  switch (category) {
    case 0:  // ALU operations
    {
      switch (operation) {  // 3rd bit is a modifier bit
        case 0:             // ADD
          output_bus = operand1 + operand2;

          break;
        case 1:  // SUB
          output_bus = operand1 - operand2;
          break;
        case 2:          // MUL
          if (modify) {  // higher 32 bits
            output_bus = static_cast<uint64_t>(operand1) * operand2 >> 32;
          } else {  // lower 32 bits
            output_bus =
                static_cast<uint64_t>(operand1) * operand2 & 0xFFFFFFFF;
          }
          break;
        case 3:  // DIV
          if (modify) {
            output_bus = operand1 / operand2;  // divide unsigned
          } else {                             // divide signed
            output_bus = static_cast<uint32_t>(static_cast<int32_t>(operand1) /
                                               static_cast<int32_t>(operand2));
          }
          break;
        case 4:  // REM
          if (modify) {
            output_bus = operand1 % operand2;  // remainder signed
          } else {                             // remainder unsigned
            output_bus = static_cast<uint32_t>(operand1) %
                         static_cast<uint32_t>(operand2);
          }
          break;
        case 5:  // AND
          output_bus = operand1 & operand2;
          break;
        case 6:  // OR
          output_bus = operand1 | operand2;
          break;
        case 7:  // XOR
          output_bus = operand1 ^ operand2;
          break;
        case 8:  // NOT
          output_bus = ~operand1;
          break;
        case 9:  //CLZ
          output_bus = count_leading_zeros(operand1);
          break;
        default:
          throw std::invalid_argument("Invalid ALU operation");
      }

      if (!(decoded.is_rs1_imm || decoded.is_rs2_imm)) {
        output_bus = apply_barrel_shift(output_bus, decoded.shift_type,
                                        decoded.shift_amount);
      }

      if (!decoded.F) {
        if (!decoded.F) {
          zero_flag = (output_bus == 0);
          negative_flag = (output_bus >> 31) & 1;

          // C and V are ONLY calculated for ADD (0) and SUB (1)
          if (operation == 0 || operation == 1) {
            if (operation == 1) {  // SUB
              carry_flag = (operand1 >= operand2);
              int32_t s1 = static_cast<int32_t>(operand1);
              int32_t s2 = static_cast<int32_t>(operand2);
              int32_t sr = static_cast<int32_t>(output_bus);
              overflow_flag = ((s1 >= 0 && s2 < 0 && sr < 0) ||
                               (s1 < 0 && s2 >= 0 && sr >= 0));
            } else {  // ADD
              carry_flag = (operand1 > (0xFFFFFFFFu - operand2));
              int32_t s1 = static_cast<int32_t>(operand1);
              int32_t s2 = static_cast<int32_t>(operand2);
              int32_t sr = static_cast<int32_t>(output_bus);
              overflow_flag = ((s1 >= 0 && s2 >= 0 && sr < 0) ||
                               (s1 < 0 && s2 < 0 && sr >= 0));
            }
          } else {
            carry_flag = false;
            overflow_flag = false;
          }
        }
      }

      set_reg(decoded.rd, output_bus);
    } break;
    case 1:  // Control flow
    {
      bool should_jump = false;
      switch (operation) {
        case 0:
          should_jump = true;  // Unconditional jump
          break;
        case 1:  // Jump if zero
          should_jump = zero_flag;
          break;
        case 2:  // Jump if not zero
          should_jump = !zero_flag;
          break;
        case 3:  // Jump if less than
          should_jump = negative_flag != overflow_flag;
          break;
        case 4:  // jump if greater than or equal
          should_jump = negative_flag == overflow_flag;
          break;
        case 5:  // jump if less than or equal
          should_jump = negative_flag != overflow_flag || zero_flag;
          break;
        case 6:  // Jump if greater than
          should_jump = negative_flag == overflow_flag && !zero_flag;
          break;
        case 7:  // Jump if carry
          should_jump = carry_flag;
          break;
        case 8:  // jump if not carry
          should_jump = !carry_flag;
          break;
        case 9:  // jump if negative
          should_jump = negative_flag;
          break;
        case 10:  // jump if not negative
          should_jump = !negative_flag;
          break;
      }
      if (should_jump) {

        if (modify) {
          pc = get_reg(decoded.rd);  // Jump to target in register
        } else {
          set_reg(decoded.rd, pc + 4);  // Store return address in rd
          pc += decoded.imm * 4;        // Jump to offset in immediate
        }

        return;
      }
    } break;
    case 2:  // Memory operations
    {
      bool is_unsigned =
          extract_bits(decoded.opcode, 5);  // 5th bit is unsigned modifier
      switch (extract_bits(decoded.opcode, 2, 2)) {
        case 0:          // Load/Store byte
          if (modify) {  // Load
            if (is_unsigned) {
              output_bus = read_byte(operand1 + decoded.imm);
            } else {
              output_bus = sign_extend(read_byte(operand1 + decoded.imm), 8);
            }
          } else {  // Store
            write_byte(operand1 + decoded.imm, get_reg(decoded.rd) & 0xFF);
          }
          break;
        case 1:          // Load/Store halfword
          if (modify) {  // Load
            if (is_unsigned) {
              output_bus = read_halfword(operand1 + decoded.imm);
            } else {
              output_bus =
                  sign_extend(read_halfword(operand1 + decoded.imm), 16);
            }
          } else {  // Store
            write_halfword(operand1 + decoded.imm,
                           get_reg(decoded.rd) & 0xFFFF);
          }
          break;
        case 2:          // Load/Store word
          if (modify) {  // Load
            if (is_unsigned) {
              output_bus = read_word(operand1 + decoded.imm);
            } else {
              output_bus = read_word(operand1 + decoded.imm);
            }
          } else {  // Store
            write_word(operand1 + decoded.imm, get_reg(decoded.rd));
          }
          break;
        default:
          throw std::invalid_argument("Invalid memory operation");
      }
      if (modify) {
        set_reg(decoded.rd, output_bus);
      }
    } break;
    default:
      throw std::invalid_argument("Invalid opcode");
  }
  pc += 4;  // Move to the next instructionI
}

uint32_t Emulator::get_reg(size_t index) {
  if (index > 31) {
    throw std::out_of_range("Register index out of range");
  }

  if (index == 0) {
    return 0;  // r0 is always zero
  }

  return registers[index];
}

void Emulator::set_reg(size_t index, uint32_t value) {
  if (index > 31) {
    throw std::out_of_range("Register index out of range");
  }

  if (index == 0) {
    return;  // r0 is always zero
  }

  registers[index] = value;
}

uint8_t Emulator::read_byte(size_t address) {
  return memory[address];
}

uint16_t Emulator::read_halfword(size_t address) {
  return memory[address] | (memory[address + 1] << 8);
}

uint32_t Emulator::read_word(size_t address) {
  return memory[address] | (memory[address + 1] << 8) |
         (memory[address + 2] << 16) | (memory[address + 3] << 24);
}

void Emulator::write_byte(size_t address, uint8_t value) {
  memory[address] = value;
}

void Emulator::write_halfword(size_t address, uint16_t value) {
  memory[address] = value & 0xFF;
  memory[address + 1] = (value >> 8) & 0xFF;
}

void Emulator::write_word(size_t address, uint32_t value) {
  memory[address] = value & 0xFF;
  memory[address + 1] = (value >> 8) & 0xFF;
  memory[address + 2] = (value >> 16) & 0xFF;
  memory[address + 3] = (value >> 24) & 0xFF;
}

uint32_t Emulator::count_leading_zeros(uint32_t value) {
  if (value == 0)
    return 32;
  return __builtin_clz(value);
}

}  // namespace sam32