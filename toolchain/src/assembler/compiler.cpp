#include <array>
#include <cstdint>
#include <stdexcept>
#include <utility>
#include <vector>

#include <sam32/assembler/compiler.hpp>
#include <sam32/utils/utils.hpp>

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
    machine_code |= (1 << 8) | (1 << 7);
    machine_code |= (get_reg(0) << 9);
    machine_code |= ((get_imm(1) & 0x3FFFF) << 14);

  } else if (instr.mnemonic >= M_BR && instr.mnemonic <= M_BPLR) {
    machine_code |= (get_reg(0) << 9);

    // Category 10: Memory
  } else if (instr.mnemonic >= M_SB && instr.mnemonic <= M_LW) {
    machine_code |= (1 << 8);  // Imm Rs2 Toggle
    machine_code |= (get_reg(0) << 9);
    machine_code |= (get_reg(1) << 14);
    machine_code |= ((get_imm(2) & 0x1FFF) << 19);

    // Category 11: System & Custom Operations
  } else if (instr.mnemonic >= M_CSRR && instr.mnemonic <= M_ECALL) {
    if (instr.mnemonic == M_CSRR) {
      machine_code |= ((get_imm(0) & 0x1F) << 9);
      machine_code |= (get_reg(1) << 14);
    } else if (instr.mnemonic == M_CSRW) {
      machine_code |= ((get_imm(0) & 0x1F) << 9);
      if (get_type(1) == OperandType::IMMEDIATE) {
        machine_code |= (1 << 7); // is_rs1_imm
        machine_code |= ((get_imm(1) & 0x1FFF) << 14);
      } else {
        machine_code |= (get_reg(1) << 14);
      }
    } else if (instr.mnemonic == M_RETI || instr.mnemonic == M_ECALL) {
      // Nothing to set, all Reg/Shift fields are 0
    }

    // Category 00: ALU
  } else {
    bool is_r_type = false;

    if (instr.operands.size() == 3) {
      machine_code |= (get_reg(0) << 9);

      if (get_type(1) == OperandType::REGISTER &&
          get_type(2) == OperandType::REGISTER) {
        is_r_type = true;
        machine_code |= (get_reg(1) << 14);
        machine_code |= (get_reg(2) << 19);
        if (instr.has_shift) {
          machine_code |= (static_cast<uint32_t>(instr.shift_type) << 24);
          machine_code |= (static_cast<uint32_t>(instr.shift_amt & 0x1F) << 26);
        }
      } else if (get_type(1) == OperandType::REGISTER &&
                 get_type(2) == OperandType::IMMEDIATE) {
        machine_code |= (1 << 8);
        machine_code |= (get_reg(1) << 14);
        machine_code |= ((get_imm(2) & 0x1FFF) << 19);
      } else if (get_type(1) == OperandType::IMMEDIATE &&
                 get_type(2) == OperandType::REGISTER) {
        machine_code |= (1 << 7);
        machine_code |= ((get_imm(1) & 0x1FFF) << 14);
        machine_code |= (get_reg(2) << 27);
      }
    } else if (instr.operands.size() == 2) {
      machine_code |= (get_reg(0) << 9);

      if (get_type(1) == OperandType::REGISTER) {
        is_r_type = true;
        machine_code |= (get_reg(1) << 14);
        if (instr.has_shift) {
          machine_code |= (static_cast<uint32_t>(instr.shift_type) << 24);
          machine_code |= (static_cast<uint32_t>(instr.shift_amt & 0x1F) << 26);
        }
      } else if (get_type(1) == OperandType::IMMEDIATE) {
        machine_code |= (1 << 8);
        machine_code |= ((get_imm(1) & 0x1FFF) << 19);
      }
    }

    if (instr.freeze_flag) {
      machine_code |= (1U << 31);
    }
  }

  return machine_code;
}

void add_data(std::vector<uint8_t>& output, const Data& data) {
  // insert data bytes into output vector, ensuring proper alignment at the address specified by data.addr_offset

  if (output.size() < data.addr_offset) {
    output.resize(data.addr_offset, 0);  // pad with zeros if needed
  }

  std::array<uint8_t, 4> bytes = to_bytes(data.value.value, data.size);

  switch (data.size) {
    case 1:
      output.push_back(bytes[0]);
      break;
    case 2:
      output.push_back(bytes[0]);
      output.push_back(bytes[1]);
      break;
    case 4:
      output.push_back(bytes[0]);
      output.push_back(bytes[1]);
      output.push_back(bytes[2]);
      output.push_back(bytes[3]);
      break;
    default:
      throw std::runtime_error("Invalid data size at line " +
                               std::to_string(data.line_number));
  }
}

std::pair<std::vector<uint8_t>, std::vector<LstRow>> encode(
    const Segment& text_segment, const Segment& data_segment) {
  std::vector<uint8_t> output;
  std::vector<LstRow> lst_rows;

  for (const auto& var : text_segment.instructions) {
    if (std::holds_alternative<ParsedInstruction>(var)) {
      const auto& instr = std::get<ParsedInstruction>(var);
      uint32_t mc = encode_instruction(instr);
      add_data(output, Data(mc, 4, instr.line_number,
                            instr.addr_offset + text_segment.addr_base));
      lst_rows.emplace_back(instr.addr_offset + text_segment.addr_base, mc,
                            instr);
    } else {
      const auto& data = std::get<Data>(var);
      add_data(output, data);
      lst_rows.emplace_back(data.addr_offset + data_segment.addr_base,
                            data.value.value, ParsedInstruction(M_NOP, {}));
    }
  }

  for (const auto& var : data_segment.instructions) {
    if (std::holds_alternative<ParsedInstruction>(var)) {
      const auto& instr = std::get<ParsedInstruction>(var);
      uint32_t mc = encode_instruction(instr);
      add_data(output, Data(mc, 4, instr.line_number,
                            instr.addr_offset + text_segment.addr_base));
      lst_rows.emplace_back(instr.addr_offset + text_segment.addr_base, mc,
                            instr);
    } else {
      const auto& data = std::get<Data>(var);
      add_data(output, data);
      lst_rows.emplace_back(data.addr_offset + data_segment.addr_base,
                            data.value.value, ParsedInstruction(M_NOP, {}));
    }
  }
  return std::make_pair(output, lst_rows);
}
}  // namespace sam32
