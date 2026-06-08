#include <fmt/base.h>
#include <cstdint>
#include <limits>
#include <stdexcept>

using ::uint8_t;

#include <indicators/block_progress_bar.hpp>
#include <indicators/cursor_control.hpp>

#include <sam32/assembler/compiler.hpp>

namespace sam32 {
Instruction::Instruction(std::vector<Token> tokens, size_t line_number,
                         size_t addr_offset) {
  Rd.type = static_cast<TOKEN_TYPE>(-1);
  Rs1.type = static_cast<TOKEN_TYPE>(-1);
  Rs2.type = static_cast<TOKEN_TYPE>(-1);
  imm.type = static_cast<TOKEN_TYPE>(-1);
  shift_amount = 0;
  shift_type = 0;
  freeze = false;
  this->line_number = line_number;
  this->addr_offset = addr_offset;
  bool expecting_shift_amount = false;

  for (size_t i = 0; i < tokens.size(); i++) {
    const Token& token = tokens[i];
    if (token.type == TOKEN_TYPE::MNEMONIC) {
      mnemonic = token;
    } else if (token.type == TOKEN_TYPE::FS) {
      freeze = true;
    } else if (token.type == TOKEN_TYPE::REGISTER) {
      if (Rd.type != TOKEN_TYPE::REGISTER) {
        Rd = token;
      } else if (Rs1.type != TOKEN_TYPE::REGISTER &&
                 imm.type != TOKEN_TYPE::IMMEDIATE &&
                 imm.type != TOKEN_TYPE::LABEL_REF) {
        Rs1 = token;
      } else if (imm.type == TOKEN_TYPE::IMMEDIATE ||
                 imm.type == TOKEN_TYPE::LABEL_REF) {
        Rs2 = token;
      } else {
        Rs2 = token;
      }
    } else if (token.type == TOKEN_TYPE::IMMEDIATE ||
               token.type == TOKEN_TYPE::LABEL_REF) {
      if (expecting_shift_amount) {
        if (token.type != TOKEN_TYPE::IMMEDIATE || token.value_int < 0 ||
            token.value_int > 31) {
          throw std::runtime_error("Invalid shift amount at line " +
                                   std::to_string(line_number));
        }
        shift_amount = static_cast<uint8_t>(token.value_int);
        expecting_shift_amount = false;
      } else {
        imm = token;
      }
    } else if (token.type == TOKEN_TYPE::SHIFT_OP) {
      if (token.value_str == "LSR" || token.value_str == "lsr")
        shift_type = 1;
      else if (token.value_str == "ASR" || token.value_str == "asr")
        shift_type = 2;
      else
        shift_type = 0;
      expecting_shift_amount = true;
    } else if (token.type == TOKEN_TYPE::SHIFT_AMT) {
      shift_amount = token.value_int;
      expecting_shift_amount = false;
    }
  }
};

void Instruction::resolve_labels(
    const std::unordered_map<std::string, size_t>& label_addresses_exact,
    size_t current_pc, const std::vector<size_t>& org_start_addresses) {
  if (imm.type == TOKEN_TYPE::LABEL_REF) {
    auto it = label_addresses_exact.find(imm.value_str);
    if (it == label_addresses_exact.end()) {
      throw std::runtime_error("Undefined label: " + imm.value_str +
                               " at line " + std::to_string(line_number));
    }

    size_t label_address = it->second;

    bool is_branch =
        (mnemonic.value_int >= M_B && mnemonic.value_int <= M_BPLR);

    if (is_branch) {
      int64_t offset_bytes = static_cast<int64_t>(label_address) -
                             static_cast<int64_t>(current_pc);
      if (offset_bytes % 4 != 0) {
        throw std::runtime_error("Label address is not instruction-aligned: " +
                                 imm.value_str);
      }
      int32_t offset_instructions = static_cast<int32_t>(offset_bytes / 4);
      imm.value_int = offset_instructions;

      // Jumps are 18 bits (signed)
      if (offset_instructions > 131071 || offset_instructions < -131072) {
        throw std::runtime_error(
            "Label address out of range for jump: " + imm.value_str +
            " at line " + std::to_string(line_number));
      }
    } else {
      // Non-branches (I-Type) use absolute addresses, not PC-relative
      imm.value_int = static_cast<int32_t>(label_address);

      // 13-bit signed immediate range is -4096 to 4095
      if (label_address > 4095) {
        throw std::runtime_error(
            "Label address out of range for 13-bit immediate: " +
            imm.value_str + " at line " + std::to_string(line_number));
      }
    }
  }
}

Data Instruction::encode() const {
  uint32_t cat = 0, op = 0;
  Mnemonic m = static_cast<Mnemonic>(mnemonic.value_int);

  switch (m) {
    case M_ADD:
      cat = 0;
      op = 0;
      break;
    case M_SUB:
      cat = 0;
      op = 1;
      break;
    case M_MUL:
      cat = 0;
      op = 2;
      break;
    case M_DIV:
      cat = 0;
      op = 3;
      break;
    case M_MOD:
      cat = 0;
      op = 4;
      break;
    case M_AND:
      cat = 0;
      op = 5;
      break;
    case M_OR:
      cat = 0;
      op = 6;
      break;
    case M_XOR:
      cat = 0;
      op = 7;
      break;
    case M_NOT:
      cat = 0;
      op = 10;
      break;
    case M_CLZ:
      cat = 0;
      op = 11;
      break;

    case M_B:
      cat = 1;
      op = 0;
      break;
    case M_BEQ:
      cat = 1;
      op = 1;
      break;
    case M_BNE:
      cat = 1;
      op = 2;
      break;
    case M_BLT:
      cat = 1;
      op = 3;
      break;
    case M_BGE:
      cat = 1;
      op = 4;
      break;
    case M_BLE:
      cat = 1;
      op = 5;
      break;
    case M_BGT:
      cat = 1;
      op = 6;
      break;
    case M_BCS:
      cat = 1;
      op = 7;
      break;
    case M_BCC:
      cat = 1;
      op = 8;
      break;
    case M_BMI:
      cat = 1;
      op = 9;
      break;
    case M_BPL:
      cat = 1;
      op = 10;
      break;

    case M_BR:
      cat = 1;
      op = 16;
      break;
    case M_BEQR:
      cat = 1;
      op = 17;
      break;
    case M_BNER:
      cat = 1;
      op = 18;
      break;
    case M_BLTR:
      cat = 1;
      op = 19;
      break;
    case M_BGER:
      cat = 1;
      op = 20;
      break;
    case M_BLER:
      cat = 1;
      op = 21;
      break;
    case M_BGTR:
      cat = 1;
      op = 22;
      break;
    case M_BCSR:
      cat = 1;
      op = 23;
      break;
    case M_BCCR:
      cat = 1;
      op = 24;
      break;
    case M_BMIR:
      cat = 1;
      op = 25;
      break;
    case M_BPLR:
      cat = 1;
      op = 26;
      break;

    case M_SW:
      cat = 2;
      op = 0;
      break;
    case M_LW:
      cat = 2;
      op = 16;
      break;
    default:
      throw std::runtime_error(
          "Unexpected mnemonic found during bit-encoding!");
  }

  uint32_t opcode_block = (cat << 7) | (op << 2);
  uint32_t encoded = 0;

  if (cat == 0 || cat == 2) {
    if (imm.type != static_cast<TOKEN_TYPE>(-1)) {
      if (Rs1.type != static_cast<TOKEN_TYPE>(-1)) {
        // I-Type A: Rd, Rs1, Imm
        opcode_block |= 0b10;  // ImmRs2 = 1, ImmRs1 = 0
        encoded = (opcode_block << 23) | ((Rd.value_int & 0x1F) << 18) |
                  ((Rs1.value_int & 0x1F) << 13) | (imm.value_int & 0x1FFF);
      } else {
        // I-Type B: Rd, Imm, Rs2
        opcode_block |= 0b01;  // ImmRs2 = 0, ImmRs1 = 1
        encoded = (opcode_block << 23) | ((Rd.value_int & 0x1F) << 18) |
                  ((imm.value_int & 0x1FFF) << 5) | (Rs2.value_int & 0x1F);
      }
    } else {
      // R-Type / unaries
      opcode_block |= 0b00;
      uint32_t rd_bits =
          (Rd.type != static_cast<TOKEN_TYPE>(-1)) ? Rd.value_int & 0x1F : 0;
      uint32_t rs1_bits =
          (Rs1.type != static_cast<TOKEN_TYPE>(-1)) ? Rs1.value_int & 0x1F : 0;
      uint32_t rs2_bits =
          (Rs2.type != static_cast<TOKEN_TYPE>(-1)) ? Rs2.value_int & 0x1F : 0;
      uint32_t ext_bits = ((shift_type & 0x3) << 6) |
                          ((shift_amount & 0x1F) << 1) | (freeze ? 1 : 0);
      encoded = (opcode_block << 23) | (rd_bits << 18) | (rs1_bits << 13) |
                (rs2_bits << 8) | ext_bits;
    }
  } else if (cat == 1) {
    if (imm.type != static_cast<TOKEN_TYPE>(-1)) {
      // J-Type
      opcode_block |= 0b00;
      encoded = (opcode_block << 23) | ((Rd.value_int & 0x1F) << 18) |
                (imm.value_int & 0x3FFFF);
    } else {
      // R-Branch
      opcode_block |= 0b00;
      encoded = (opcode_block << 23) | ((Rd.value_int & 0x1F) << 18);
    }
  }

  return Data(addr_offset, encoded);
}

std::vector<uint32_t> compile_instructions(const std::vector<Org>& orgs,
                                           bool verbose) {

  indicators::BlockProgressBar bar{
      indicators::option::BarWidth{80},
      indicators::option::Start{"["},
      indicators::option::End{"]"},
      indicators::option::ForegroundColor{indicators::Color::white},
      indicators::option::ShowPercentage{true},
      indicators::option::FontStyles{
          std::vector<indicators::FontStyle>{indicators::FontStyle::bold}}};

  if (verbose) {
    fmt::println("Compiling to machine code...");
  }

  std::vector<uint32_t> machine_code;

  // TODO: populate machine_code with instructions form all orgs, filling in zeros for gaps
  for (const auto& org : orgs) {
    for (const auto& item : org.data) {
      Data next(0, 0);
      if (std::holds_alternative<Instruction>(item)) {
        const Instruction& instr = std::get<Instruction>(item);
        next =
            Data(org.start_address + instr.addr_offset, instr.encode().value);
      } else {
        const Data& data = std::get<Data>(item);
        next = Data(org.start_address + data.addr_offset, data.value);
      }

      while (machine_code.size() * 4 < next.addr_offset) {
        machine_code.push_back(0);
      }
      machine_code.push_back(next.value);
    }
  }

  if (verbose) {
    fmt::println("\nCompilation complete.");
    indicators::show_console_cursor(true);
  }
  return machine_code;
};

}  // namespace sam32
