#include <sam32/emulator/emulator.hpp>
#include <stdexcept>

namespace sam32 {
Instruction_t::Instruction_t(uint32_t instruction) : raw(instruction) {
  category = extract_bits(instruction, 30, 2);
  operation = extract_bits(instruction, 25, 5);
  is_rs2_imm = extract_bits(instruction, 24, 1);
  is_rs1_imm = extract_bits(instruction, 23, 1);
  
  rd = extract_bits(instruction, 18, 5);
  rs1 = extract_bits(instruction, 13, 5);
  
  if (is_rs1_imm) {
    imm = sign_extend(extract_bits(instruction, 5, 13), 13);
    rs2 = extract_bits(instruction, 0, 5);
    shift_type = 0;
    shift_amount = 0;
    F = false;
  } else if (is_rs2_imm) {
    imm = sign_extend(extract_bits(instruction, 0, 13), 13);
    rs2 = 0; // Not used
    shift_type = 0;
    shift_amount = 0;
    F = false;
  } else {
    imm = 0;
    rs2 = extract_bits(instruction, 8, 5);
    shift_type = extract_bits(instruction, 6, 2);
    shift_amount = extract_bits(instruction, 1, 5);
    F = extract_bits(instruction, 0, 1);
  }
  
  if (category == 1 && operation <= 10) { // J-Type PC-relative branch
    branch_offset = sign_extend(extract_bits(instruction, 0, 18), 18);
  } else {
    branch_offset = 0;
  }
}

Emulator::Emulator(size_t memory_size) : state(memory_size) {}

void Emulator::load_program(const std::vector<uint8_t>& program,
                            size_t start_address) {
  for (size_t i = 0; i < program.size(); ++i) {
    state.write_byte(start_address + i, program[i]);
  }
}

void Emulator::tick() {
  if (breakpoints.count(
          state
              .pc)) {  // get the instruction before so this one doesn't execute yet
    breakpoints[state.pc]();
  }

  uint32_t instruction = state.read_word(state.pc);
  execute_instruction(instruction);
  history.push_back(state);
  cycle_count++;
}

void Emulator::undo() {
  if (!history.empty()) {
    state = history.back();
    history.pop_back();
    cycle_count--;
  }
}

void Emulator::reset() {
  state = EmulatorState(state.memory.size());
  cycle_count = 0;
}

void Emulator::execute_instruction(uint32_t instruction) {
  Instruction_t decoded = Instruction_t(instruction);
  uint8_t operation = decoded.operation;
  uint8_t category = decoded.category;

  uint32_t output_bus = 0;
  uint32_t operand1;
  uint32_t operand2;

  if (decoded.is_rs1_imm) {
    operand1 = static_cast<uint32_t>(static_cast<int32_t>(decoded.imm));
    operand2 = state.get_reg(decoded.rs2);
  } else if (decoded.is_rs2_imm) {
    operand1 = state.get_reg(decoded.rs1);
    operand2 = static_cast<uint32_t>(static_cast<int32_t>(decoded.imm));
  } else {
    operand1 = state.get_reg(decoded.rs1);
    operand2 = state.get_reg(decoded.rs2);
  }

  // use the category bits to determine the instruction type
  switch (category) {
    case 0:  // ALU operations
    {
      if (!(decoded.is_rs1_imm || decoded.is_rs2_imm)) {
        operand2 = apply_barrel_shift(operand2, decoded.shift_type, decoded.shift_amount);
      }

      switch (operation) {
        case 0:  // ADD
          output_bus = operand1 + operand2;
          break;
        case 1:  // SUB
          output_bus = operand1 - operand2;
          break;
        case 2:  // MUL
          output_bus = static_cast<uint64_t>(operand1) * operand2 & 0xFFFFFFFF;
          break;
        case 3:  // DIV (Unsigned)
          if (operand2 != 0) {
            output_bus = operand1 / operand2;
          } else {
            output_bus = 0; // Or handle divide by zero
          }
          break;
        case 4:  // MOD (Unsigned)
          if (operand2 != 0) {
            output_bus = operand1 % operand2;
          } else {
            output_bus = 0;
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
        case 10:  // NOT
          output_bus = ~operand1;
          break;
        case 11:  // CLZ
          output_bus = count_leading_zeros(operand1);
          break;
        default:
          throw std::invalid_argument("Invalid ALU operation");
      }

      if (!decoded.F || decoded.is_rs1_imm || decoded.is_rs2_imm) {
        state.zero_flag = (output_bus == 0);
        state.negative_flag = (output_bus >> 31) & 1;

        if (operation == 0 || operation == 1) {
          if (operation == 1) {  // SUB
            state.carry_flag = (operand1 >= operand2);
            int32_t s1 = static_cast<int32_t>(operand1);
            int32_t s2 = static_cast<int32_t>(operand2);
            int32_t sr = static_cast<int32_t>(output_bus);
            state.overflow_flag = ((s1 >= 0 && s2 < 0 && sr < 0) ||
                                   (s1 < 0 && s2 >= 0 && sr >= 0));
          } else {  // ADD
            state.carry_flag = (operand1 > (0xFFFFFFFFu - operand2));
            int32_t s1 = static_cast<int32_t>(operand1);
            int32_t s2 = static_cast<int32_t>(operand2);
            int32_t sr = static_cast<int32_t>(output_bus);
            state.overflow_flag = ((s1 >= 0 && s2 >= 0 && sr < 0) ||
                                   (s1 < 0 && s2 < 0 && sr >= 0));
          }
        }
      }

      state.set_reg(decoded.rd, output_bus);
    } break;
    case 1:  // Control flow
    {
      bool should_jump = false;
      bool is_r_branch = (operation >= 16);
      uint8_t cond_op = is_r_branch ? (operation - 16) : operation;

      switch (cond_op) {
        case 0:  // B / BR
          should_jump = true;
          break;
        case 1:  // BEQ / BEQR
          should_jump = state.zero_flag;
          break;
        case 2:  // BNE / BNER
          should_jump = !state.zero_flag;
          break;
        case 3:  // BLT / BLTR
          should_jump = state.negative_flag != state.overflow_flag;
          break;
        case 4:  // BGE / BGER
          should_jump = state.negative_flag == state.overflow_flag;
          break;
        case 5:  // BLE / BLER
          should_jump = state.zero_flag || (state.negative_flag != state.overflow_flag);
          break;
        case 6:  // BGT / BGTR
          should_jump = !state.zero_flag && (state.negative_flag == state.overflow_flag);
          break;
        case 7:  // BCS / BCSR
          should_jump = state.carry_flag;
          break;
        case 8:  // BCC / BCCR
          should_jump = !state.carry_flag;
          break;
        case 9:  // BMI / BMIR
          should_jump = state.negative_flag;
          break;
        case 10:  // BPL / BPLR
          should_jump = !state.negative_flag;
          break;
        default:
          throw std::invalid_argument("Invalid Branch operation");
      }

      if (should_jump) {
        if (is_r_branch) {
          state.pc = state.get_reg(decoded.rs1);
        } else {
          state.set_reg(decoded.rd, state.pc + 4);
          state.pc += decoded.branch_offset * 4;
        }
        return;
      }
    } break;
    case 2:  // Memory operations
    {
      uint32_t addr = operand1 + operand2; // For memory, operand1/2 evaluates Rs1 + Imm correctly
      switch (operation) {
        case 16: // LW
          output_bus = state.read_word(addr);
          state.set_reg(decoded.rd, output_bus);
          break;
        case 0: // SW
          state.write_word(addr, state.get_reg(decoded.rd));
          break;
        default:
          throw std::invalid_argument("Invalid Memory operation");
      }
    } break;
    case 3: // System / Custom
    {
      if (operation == 0) {
        // HALT
        return; // Alternatively could throw, but returning early stops PC increment
      } else {
        throw std::invalid_argument("Invalid System operation");
      }
    } break;
    default:
      throw std::invalid_argument("Invalid category");
  }
  
  state.pc += 4;  // Move to the next instruction
}

uint32_t Emulator::count_leading_zeros(uint32_t value) {
  if (value == 0)
    return 32;
  return __builtin_clz(value);
}

void Emulator::add_breakpoint(size_t address, std::function<void()> callback) {
  breakpoints[address] = callback;
}

uint32_t EmulatorState::get_reg(size_t index) {
  if (index > 31) {
    throw std::out_of_range("Register index out of range");
  }

  if (index == 0) {
    return 0;  // r0 is always zero
  }

  return registers[index];
}

void EmulatorState::set_reg(size_t index, uint32_t value) {
  if (index > 31) {
    throw std::out_of_range("Register index out of range");
  }

  if (index == 0) {
    return;  // r0 is always zero
  }

  registers[index] = value;
}

uint8_t EmulatorState::read_byte(size_t address) {
  return memory.at(address);
}

uint16_t EmulatorState::read_halfword(size_t address) {
  return memory.at(address) | (memory.at(address + 1) << 8);
}

uint32_t EmulatorState::read_word(size_t address) {
  return memory.at(address) | (memory.at(address + 1) << 8) |
         (memory.at(address + 2) << 16) | (memory.at(address + 3) << 24);
}

void EmulatorState::write_byte(size_t address, uint8_t value) {
  memory.at(address) = value;
}

void EmulatorState::write_halfword(size_t address, uint16_t value) {
  memory.at(address) = value & 0xFF;
  memory.at(address + 1) = (value >> 8) & 0xFF;
}

void EmulatorState::write_word(size_t address, uint32_t value) {
  memory.at(address) = value & 0xFF;
  memory.at(address + 1) = (value >> 8) & 0xFF;
  memory.at(address + 2) = (value >> 16) & 0xFF;
  memory.at(address + 3) = (value >> 24) & 0xFF;
}

}  // namespace sam32