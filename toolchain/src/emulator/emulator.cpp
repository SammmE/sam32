#include <sam32/emulator/emulator.hpp>
#include <stdexcept>

namespace sam32 {
Instruction_t::Instruction_t(uint32_t instruction) : raw(instruction) {
  category = extract_bits(instruction, 0, 2);
  operation = extract_bits(instruction, 2, 5);
  is_rs1_imm = extract_bits(instruction, 7, 1);
  is_rs2_imm = extract_bits(instruction, 8, 1);
  
  rd = extract_bits(instruction, 9, 5);

  if (category == 1 && is_rs1_imm && is_rs2_imm) {
    // J-Type PC-relative branch
    branch_offset = sign_extend(extract_bits(instruction, 14, 18), 18);
    rs1 = 0;
    rs2 = 0;
    imm = 0;
    shift_type = 0;
    shift_amount = 0;
    F = false;
  } else if (category == 1 && extract_bits(operation, 4, 1) == 1 && !is_rs1_imm && !is_rs2_imm) {
    // R-Branch (Register-Indirect)
    rs1 = extract_bits(instruction, 9, 5); // Target is in Rd field bits [13:9]
    rs2 = 0;
    imm = 0;
    branch_offset = 0;
    shift_type = 0;
    shift_amount = 0;
    F = false;
  } else {
    branch_offset = 0;

    if (is_rs1_imm) { // I-Type B
      imm = sign_extend(extract_bits(instruction, 14, 13), 13);
      rs1 = 0; // Not used
      rs2 = extract_bits(instruction, 27, 5);
      shift_type = 0;
      shift_amount = 0;
      F = false;
    } else if (is_rs2_imm) { // I-Type A
      imm = sign_extend(extract_bits(instruction, 19, 13), 13);
      rs1 = extract_bits(instruction, 14, 5);
      rs2 = 0; // Not used
      shift_type = 0;
      shift_amount = 0;
      F = false;
    } else { // R-Type
      rs1 = extract_bits(instruction, 14, 5);
      rs2 = extract_bits(instruction, 19, 5);
      shift_type = extract_bits(instruction, 24, 2);
      shift_amount = extract_bits(instruction, 26, 5);
      F = extract_bits(instruction, 31, 1);
      imm = 0;
    }
  }
}

Emulator::Emulator(size_t memory_size, bool allow_placeholders) : state(memory_size), allow_placeholders(allow_placeholders) {
  last_tick_time = std::chrono::high_resolution_clock::now();
}

void Emulator::load_program(const std::vector<uint8_t>& program,
                            size_t start_address) {
  for (size_t i = 0; i < program.size(); ++i) {
    state.write_byte(start_address + i, program[i]);
  }
}

void Emulator::tick() {
  if (capped_clock_speed) {
    auto now = std::chrono::high_resolution_clock::now();
    while (std::chrono::duration_cast<std::chrono::nanoseconds>(now - last_tick_time).count() < 10) {
      now = std::chrono::high_resolution_clock::now();
    }
    last_tick_time = now;
  } else {
    auto now = std::chrono::high_resolution_clock::now();
    while (std::chrono::duration_cast<std::chrono::microseconds>(now - last_tick_time).count() < 10) {
      now = std::chrono::high_resolution_clock::now();
    }
    last_tick_time = now;
  }

  if (breakpoints.count(state.pc)) {
    breakpoints[state.pc]();
  }

  state.mtime++;
  if (state.gie && state.mtimecmp < state.mtime) {
    trigger_trap(0x80000001);
  }

  uint32_t instruction = state.read_word(state.pc);
  state.heatmap[state.pc]++;
  execute_instruction(instruction);

  if (state.last_execution.writes_to_memory) {
    if (watchpoints.count(state.last_execution.memory_address_written)) {
      watchpoints[state.last_execution.memory_address_written]();
    }
  }

  if (history.size() > 100) {
    history.erase(history.begin());
  }
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

  state.last_execution.raw_instruction = instruction;
  state.last_execution.pc = state.pc;
  state.last_execution.writes_to_register = false;
  state.last_execution.writes_to_memory = false;
  state.last_execution.executed = true;

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
      state.last_execution.operand1 = operand1;
      state.last_execution.operand2 = operand2;

      if (!allow_placeholders && (operation == 3 || operation == 19 || operation == 4 || operation == 20)) {
        throw std::invalid_argument("Placeholder instruction not implemented. Enable --allow-placeholders to execute.");
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
        case 18: // MULH
          output_bus = (static_cast<uint64_t>(operand1) * operand2) >> 32;
          break;
        case 3:  // DIV
          if (operand2 != 0) {
            output_bus = static_cast<uint32_t>(static_cast<int32_t>(operand1) / static_cast<int32_t>(operand2));
          } else {
            output_bus = 0; // Or handle divide by zero
          }
          break;
        case 19: // DIVU
          if (operand2 != 0) {
            output_bus = operand1 / operand2;
          } else {
            output_bus = 0;
          }
          break;
        case 4:  // REM
          if (operand2 != 0) {
            output_bus = static_cast<uint32_t>(static_cast<int32_t>(operand1) % static_cast<int32_t>(operand2));
          } else {
            output_bus = 0;
          }
          break;
        case 20: // REMU
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
        case 8:  // NOT
          output_bus = ~operand1;
          break;
        case 9:  // CLZ
          output_bus = count_leading_zeros(operand1);
          break;
        default:
          trigger_trap(0);
          return;
      }

      uint32_t unshifted_output = output_bus;
      if (!(decoded.is_rs1_imm || decoded.is_rs2_imm)) {
        output_bus = apply_barrel_shift(output_bus, decoded.shift_type, decoded.shift_amount);
      }

      if (!decoded.F || decoded.is_rs1_imm || decoded.is_rs2_imm) {
        state.zero_flag = (output_bus == 0);
        state.negative_flag = (output_bus >> 31) & 1;

        if (operation == 0 || operation == 1) {
          if (operation == 1) {  // SUB
            state.carry_flag = (operand1 >= operand2);
            int32_t s1 = static_cast<int32_t>(operand1);
            int32_t s2 = static_cast<int32_t>(operand2);
            int32_t sr = static_cast<int32_t>(unshifted_output);
            state.overflow_flag = ((s1 >= 0 && s2 < 0 && sr < 0) ||
                                   (s1 < 0 && s2 >= 0 && sr >= 0));
          } else {  // ADD
            state.carry_flag = (operand1 > (0xFFFFFFFFu - operand2));
            int32_t s1 = static_cast<int32_t>(operand1);
            int32_t s2 = static_cast<int32_t>(operand2);
            int32_t sr = static_cast<int32_t>(unshifted_output);
            state.overflow_flag = ((s1 >= 0 && s2 >= 0 && sr < 0) ||
                                   (s1 < 0 && s2 < 0 && sr >= 0));
          }
        }
      }

      state.last_execution.alu_output = output_bus;
      state.last_execution.writes_to_register = true;
      state.last_execution.written_register = decoded.rd;

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
          trigger_trap(0);
          return;
      }

      if (should_jump) {
        if (is_r_branch) {
          if (decoded.rs1 == 30 && !state.call_stack.empty()) {
            state.call_stack.pop_back(); // RET instruction
          }
          state.pc = state.get_reg(decoded.rs1);
        } else {
          if (decoded.rd == 30) {
            state.call_stack.push_back(state.pc + 4); // CALL instruction
          }
          state.last_execution.writes_to_register = true;
          state.last_execution.written_register = decoded.rd;
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
        case 16: // LB
          output_bus = static_cast<uint32_t>(static_cast<int32_t>(static_cast<int8_t>(state.read_byte(addr))));
          state.last_execution.writes_to_register = true;
          state.last_execution.written_register = decoded.rd;
          state.set_reg(decoded.rd, output_bus);
          break;
        case 24: // LBU
          output_bus = state.read_byte(addr);
          state.last_execution.writes_to_register = true;
          state.last_execution.written_register = decoded.rd;
          state.set_reg(decoded.rd, output_bus);
          break;
        case 17: // LH
          output_bus = static_cast<uint32_t>(static_cast<int32_t>(static_cast<int16_t>(state.read_halfword(addr))));
          state.last_execution.writes_to_register = true;
          state.last_execution.written_register = decoded.rd;
          state.set_reg(decoded.rd, output_bus);
          break;
        case 25: // LHU
          output_bus = state.read_halfword(addr);
          state.last_execution.writes_to_register = true;
          state.last_execution.written_register = decoded.rd;
          state.set_reg(decoded.rd, output_bus);
          break;
        case 18: // LW
          output_bus = state.read_word(addr);
          state.last_execution.writes_to_register = true;
          state.last_execution.written_register = decoded.rd;
          state.set_reg(decoded.rd, output_bus);
          break;
        case 0: // SB
          state.last_execution.writes_to_memory = true;
          state.last_execution.memory_address_written = addr;
          state.last_execution.memory_value_written = state.get_reg(decoded.rd) & 0xFF;
          state.write_byte(addr, state.get_reg(decoded.rd) & 0xFF);
          break;
        case 1: // SH
          state.last_execution.writes_to_memory = true;
          state.last_execution.memory_address_written = addr;
          state.last_execution.memory_value_written = state.get_reg(decoded.rd) & 0xFFFF;
          state.write_halfword(addr, state.get_reg(decoded.rd) & 0xFFFF);
          break;
        case 2: // SW
          state.last_execution.writes_to_memory = true;
          state.last_execution.memory_address_written = addr;
          state.last_execution.memory_value_written = state.get_reg(decoded.rd);
          state.write_word(addr, state.get_reg(decoded.rd));
          break;
        default:
          trigger_trap(0);
          return;
      }
    } break;
    case 3: // System / Custom
    {
      switch (operation) {
        case 0: // CSRR
          state.set_reg(decoded.rs1, state.read_csr(decoded.rd));
          break;
        case 1: // CSRW
          state.write_csr(decoded.rd, operand1);
          break;
        case 3: // RETI
          state.pc = state.epc;
          state.gie = true;
          return;
        case 8: // ECALL
          trigger_trap(0x00000008);
          return;
        default:
          trigger_trap(0);
          return;
      }
    } break;
    default:
      trigger_trap(0);
      return;
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
  if ((address & 0x80000000) == 0) {
    if (address < umem_limit && (address + umem_base) < memory.size()) {
      return memory.at(address + umem_base);
    }
    return 0;
  }
  return memory.at(address); // Peripheral or direct access
}

uint16_t EmulatorState::read_halfword(size_t address) {
  if ((address & 0x80000000) == 0) {
    if (address < umem_limit && (address + umem_base + 1) < memory.size()) {
      return memory.at(address + umem_base) | (memory.at(address + umem_base + 1) << 8);
    }
    return 0;
  }
  return memory.at(address) | (memory.at(address + 1) << 8);
}

uint32_t EmulatorState::read_word(size_t address) {
  if ((address & 0x80000000) == 0) {
    if (address < umem_limit && (address + umem_base + 3) < memory.size()) {
      return memory.at(address + umem_base) | (memory.at(address + umem_base + 1) << 8) |
             (memory.at(address + umem_base + 2) << 16) | (memory.at(address + umem_base + 3) << 24);
    }
    return 0;
  }
  return memory.at(address) | (memory.at(address + 1) << 8) |
         (memory.at(address + 2) << 16) | (memory.at(address + 3) << 24);
}

void EmulatorState::write_byte(size_t address, uint8_t value) {
  if ((address & 0x80000000) == 0) {
    if (address < umem_limit && (address + umem_base) < memory.size()) {
      memory.at(address + umem_base) = value;
    }
    return;
  }
  memory.at(address) = value;
}

void EmulatorState::write_halfword(size_t address, uint16_t value) {
  if ((address & 0x80000000) == 0) {
    if (address < umem_limit && (address + umem_base + 1) < memory.size()) {
      memory.at(address + umem_base) = value & 0xFF;
      memory.at(address + umem_base + 1) = (value >> 8) & 0xFF;
    }
    return;
  }
  memory.at(address) = value & 0xFF;
  memory.at(address + 1) = (value >> 8) & 0xFF;
}

void EmulatorState::write_word(size_t address, uint32_t value) {
  if ((address & 0x80000000) == 0) {
    if (address < umem_limit && (address + umem_base + 3) < memory.size()) {
      memory.at(address + umem_base) = value & 0xFF;
      memory.at(address + umem_base + 1) = (value >> 8) & 0xFF;
      memory.at(address + umem_base + 2) = (value >> 16) & 0xFF;
      memory.at(address + umem_base + 3) = (value >> 24) & 0xFF;
    }
    return;
  }
  memory.at(address) = value & 0xFF;
  memory.at(address + 1) = (value >> 8) & 0xFF;
  memory.at(address + 2) = (value >> 16) & 0xFF;
  memory.at(address + 3) = (value >> 24) & 0xFF;
}

uint32_t EmulatorState::read_csr(uint8_t address) {
  switch (address) {
    case 0x00: { // STATUS
      uint32_t status = 0;
      if (zero_flag) status |= (1 << 0);
      if (negative_flag) status |= (1 << 1);
      if (carry_flag) status |= (1 << 2);
      if (overflow_flag) status |= (1 << 3);
      return status;
    }
    case 0x01: return epc;
    case 0x02: return cause;
    case 0x03: return tvec;
    case 0x04: return gie ? 1 : 0;
    case 0x05: return mtime;
    case 0x06: return mtimecmp;
    case 0x07: return 0; // BOOT
    case 0x08: return umem_base;
    case 0x09: return umem_limit;
  }
  return 0;
}

void EmulatorState::write_csr(uint8_t address, uint32_t value) {
  switch (address) {
    case 0x00: { // STATUS
      zero_flag = (value & (1 << 0)) != 0;
      negative_flag = (value & (1 << 1)) != 0;
      carry_flag = (value & (1 << 2)) != 0;
      overflow_flag = (value & (1 << 3)) != 0;
      break;
    }
    case 0x01: epc = value; break;
    case 0x02: cause = value; break;
    case 0x03: tvec = value; break;
    case 0x04: gie = (value & 1) != 0; break;
    case 0x05: mtime = value; break;
    case 0x06: mtimecmp = value; break;
    case 0x08: umem_base = value; break;
    case 0x09: umem_limit = value; break;
  }
}

void Emulator::trigger_trap(uint32_t trap_cause) {
  state.epc = state.pc;
  state.cause = trap_cause;
  state.gie = false;
  state.pc = state.tvec;
}

}  // namespace sam32