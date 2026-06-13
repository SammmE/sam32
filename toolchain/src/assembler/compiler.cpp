#include <algorithm>
#include <iostream>
#include <sam32/assembler/compiler.hpp>
#include <stdexcept>
#include <vector>

std::array<uint8_t, 4> to_bytes(uint32_t word, uint8_t size = 4) {
  std::array<uint8_t, 4> bytes = {0, 0, 0, 0};
  for (uint8_t i = 0; i < size; ++i) {
    bytes[i] = (word >> (8 * i)) & 0xFF;
  }
  return bytes;
}

namespace sam32 {
uint32_t encode_instruction(const ParsedInstruction& instr) {
  auto get_reg = [&](size_t idx) -> uint32_t {
    if (idx >= instr.operands.size()) {
      throw std::runtime_error(
          "Encoder Error: Expected register at operand " + std::to_string(idx) +
          " for '" + mnemonic_names[static_cast<size_t>(instr.mnemonic)] +
          "' at line " + std::to_string(instr.line_number));
    }
    if (instr.operands[idx].type != OperandType::REGISTER) {
      throw std::runtime_error(
          "Encoder Error: Expected REGISTER at operand " + std::to_string(idx) +
          " for '" + mnemonic_names[static_cast<size_t>(instr.mnemonic)] +
          "' at line " + std::to_string(instr.line_number));
    }
    return static_cast<uint32_t>(instr.operands[idx].reg_id) & 0x1F;
  };

  auto get_imm = [&](size_t idx) -> uint32_t {
    if (idx >= instr.operands.size()) {
      throw std::runtime_error(
          "Encoder Error: Expected immediate at operand " +
          std::to_string(idx) + " for '" +
          mnemonic_names[static_cast<size_t>(instr.mnemonic)] + "' at line " +
          std::to_string(instr.line_number));
    }
    if (instr.operands[idx].type != OperandType::IMMEDIATE) {
      throw std::runtime_error(
          "Encoder Error: Expected IMMEDIATE at operand " +
          std::to_string(idx) + " for '" +
          mnemonic_names[static_cast<size_t>(instr.mnemonic)] + "' at line " +
          std::to_string(instr.line_number));
    }
    return instr.operands[idx].value;
  };

  auto get_type = [&](size_t idx) -> OperandType {
    if (idx >= instr.operands.size())
      throw std::runtime_error(
          "Encoder Error: Expected operand at index " + std::to_string(idx) +
          " for '" + mnemonic_names[static_cast<size_t>(instr.mnemonic)] +
          "' at line " + std::to_string(instr.line_number));
    return instr.operands[idx].type;
  };

  uint32_t machine_code = opcode_map.at(instr.mnemonic);

  // Category 01: Branches
  if (instr.mnemonic >= M_B && instr.mnemonic <= M_BPL) {
    machine_code |= (get_reg(0) << 18);
    machine_code |= (get_imm(1) & 0x3FFFF);

  } else if (instr.mnemonic >= M_BR && instr.mnemonic <= M_BPLR) {
    machine_code |= (get_reg(0) << 18);

    // Category 10: Memory
  } else if (instr.mnemonic >= M_SB && instr.mnemonic <= M_LW) {
    machine_code |= (1 << 24);  // Imm Rs2 Toggle
    machine_code |= (get_reg(0) << 18);
    machine_code |= (get_reg(1) << 13);
    machine_code |= (get_imm(2) & 0x1FFF);

    // Category 00: ALU
  } else {
    bool is_r_type = false;

    if (instr.operands.size() == 3) {
      machine_code |= (get_reg(0) << 18);

      if (get_type(1) == OperandType::REGISTER &&
          get_type(2) == OperandType::REGISTER) {
        is_r_type = true;
        machine_code |= (get_reg(1) << 13);
        machine_code |= (get_reg(2) << 8);
        if (instr.has_shift) {
          machine_code |= (static_cast<uint32_t>(instr.shift_type) << 6);
          machine_code |= (static_cast<uint32_t>(instr.shift_amt & 0x1F) << 1);
        }
      } else if (get_type(1) == OperandType::REGISTER &&
                get_type(2) == OperandType::IMMEDIATE) {
        machine_code |= (1 << 24);
        machine_code |= (get_reg(1) << 13);
        machine_code |= (get_imm(2) & 0x1FFF);
      } else if (get_type(1) == OperandType::IMMEDIATE &&
                get_type(2) == OperandType::REGISTER) {
        machine_code |= (1 << 23);
        machine_code |= ((get_imm(1) & 0x1FFF) << 5);
        machine_code |= (get_reg(2));
      }
    } else if (instr.operands.size() == 2) {
      machine_code |= (get_reg(0) << 18);
      
      if (get_type(1) == OperandType::REGISTER) {
        is_r_type = true;
        machine_code |= (get_reg(1) << 13);
        if (instr.has_shift) {
          machine_code |= (static_cast<uint32_t>(instr.shift_type) << 6);
          machine_code |= (static_cast<uint32_t>(instr.shift_amt & 0x1F) << 1);
        }
      } else if (get_type(1) == OperandType::IMMEDIATE) {
        machine_code |= (1 << 24);
        machine_code |= (get_imm(1) & 0x1FFF);
      }
    }

    if (instr.freeze_flag) {
      machine_code |= 1;
    }
  }

  return machine_code;
}

std::vector<uint8_t> encode(const Segment& text_segment,
                            const Segment& data_segment) {
  std::vector<uint8_t> output;

  for (const auto& var : text_segment.instructions) {
    if (std::holds_alternative<ParsedInstruction>(var)) {
      const auto& instr = std::get<ParsedInstruction>(var);
      uint32_t mc = encode_instruction(instr);
      auto bytes = to_bytes(mc);
      output.insert(output.end(), bytes.begin(), bytes.end());
    } else {
      const auto& data = std::get<Data>(var);
      auto bytes = to_bytes(data.value.value, data.size);
      output.insert(output.end(), bytes.begin(), bytes.begin() + data.size);
    }
  }

  for (const auto& var : data_segment.instructions) {
    if (std::holds_alternative<Data>(var)) {
      const auto& data = std::get<Data>(var);
      auto bytes = to_bytes(data.value.value, data.size);
      output.insert(output.end(), bytes.begin(), bytes.begin() + data.size);
    }
  }
  return output;
}
}  // namespace sam32
