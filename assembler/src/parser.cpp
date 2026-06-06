#include <algorithm>
#include <cctype>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

using ::uint8_t;

#include <fmt/base.h>
#include <indicators/block_progress_bar.hpp>
#include <indicators/cursor_control.hpp>

#include <sam32asm/compiler.hpp>
#include <sam32asm/lexer.hpp>
#include <sam32asm/parser.hpp>

namespace sam32asm {

namespace {
std::string lowercase(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  return value;
}

int32_t parse_register_value(const std::string& value) {
  if (value.size() < 2 || (value[0] != 'r' && value[0] != 'R')) {
    throw std::runtime_error("Invalid generated register token: " + value);
  }
  size_t parsed = 0;
  int32_t reg = std::stoi(value.substr(1), &parsed, 10);
  if (parsed != value.size() - 1 || reg < 0 || reg > 31) {
    throw std::runtime_error("Invalid generated register token: " + value);
  }
  return reg;
}

int32_t parse_immediate_value(const std::string& value) {
  if (value.empty() || value[0] != '#') {
    throw std::runtime_error("Invalid generated immediate token: " + value);
  }

  const std::string digits = value.substr(1);
  size_t parsed = 0;
  int64_t imm = 0;

  if (digits.rfind("0b", 0) == 0 || digits.rfind("-0b", 0) == 0) {
    const bool negative = digits[0] == '-';
    const size_t prefix_len = negative ? 3 : 2;
    if (digits.size() == prefix_len) {
      throw std::runtime_error("Invalid generated immediate token: " + value);
    }
    for (size_t i = prefix_len; i < digits.size(); i++) {
      if (digits[i] != '0' && digits[i] != '1') {
        throw std::runtime_error("Invalid generated immediate token: " + value);
      }
      imm = (imm << 1) + (digits[i] - '0');
    }
    if (negative) {
      imm = -imm;
    }
  } else {
    imm = std::stoll(digits, &parsed, 0);
    if (parsed != digits.size()) {
      throw std::runtime_error("Invalid generated immediate token: " + value);
    }
  }

  if (imm < INT32_MIN || imm > INT32_MAX) {
    throw std::runtime_error("Generated immediate token out of range: " +
                             value);
  }
  return static_cast<int32_t>(imm);
}

int32_t mnemonic_value(const std::string& value) {
  std::string mnem = lowercase(value);
  int num_mnemonics = sizeof(MNEMONICS) / sizeof(MNEMONICS[0]);
  for (int i = 0; i < num_mnemonics; i++) {
    if (mnem == MNEMONICS[i]) {
      return i;
    }
  }
  throw std::runtime_error("Invalid generated mnemonic token: " + value);
}

Token T(TOKEN_TYPE type, std::string value, int32_t int_value = 0) {
  return Token{type, value, int_value, 0};
}

Token M(std::string value) {
  return T(TOKEN_TYPE::MNEMONIC, value, mnemonic_value(value));
}

Token R(std::string value) {
  return T(TOKEN_TYPE::REGISTER, value, parse_register_value(value));
}

Token I(std::string value) {
  return T(TOKEN_TYPE::IMMEDIATE, value, parse_immediate_value(value));
}

Token S(std::string value) {
  return T(TOKEN_TYPE::SHIFT_OP, value);
}

Token C() {
  return T(TOKEN_TYPE::COMMA, ",");
}
}  // namespace

bool matches_model(const std::vector<Token>& line,
                   const std::vector<TOKEN_TYPE>& model) {
  if (line.empty() || model.empty())
    return false;

  size_t model_index = 1;
  size_t line_index =
      (line.size() > 1 && line[1].type == TOKEN_TYPE::FS) ? 2 : 1;

  while (model_index < model.size() && line_index < line.size()) {
    if (line[line_index].type != model[model_index]) {
      if (model[model_index] == TOKEN_TYPE::IMMEDIATE &&
          line[line_index].type == TOKEN_TYPE::LABEL_REF) {
        // Accept LABEL_REF anywhere an IMMEDIATE is expected for jump/branch resolution
      } else {
        return false;
      }
    }
    model_index++;
    line_index++;
  }

  // verify shift tokens (if present)
  if (model_index < model.size() &&
      model[model_index] == TOKEN_TYPE::SHIFT_OP) {
    if (line_index >= line.size() ||
        line[line_index].type != TOKEN_TYPE::SHIFT_OP) {
      return false;
    }
    model_index++;
    line_index++;
    if (model_index < model.size() &&
        model[model_index] == TOKEN_TYPE::SHIFT_AMT) {
      if (line_index >= line.size() ||
          line[line_index].type != TOKEN_TYPE::SHIFT_AMT) {
        return false;
      }
      model_index++;
      line_index++;
    } else {
      return false;
    }
  } else if (model_index < model.size() &&
             model[model_index] == TOKEN_TYPE::SHIFT_AMT) {
    return false;
  } else if (model_index < model.size() || line_index < line.size()) {
    return false;
  }

  return true;
}

Parser::Parser(const std::vector<std::vector<Token>>& tokens) {
  this->tokens = tokens;
};

void Parser::parse(bool verbose) {
  size_t current_address = 0;

  indicators::BlockProgressBar bar{
      indicators::option::BarWidth{80},
      indicators::option::Start{"["},
      indicators::option::End{"]"},
      indicators::option::ForegroundColor{indicators::Color::white},
      indicators::option::ShowPercentage{true},
      indicators::option::FontStyles{
          std::vector<indicators::FontStyle>{indicators::FontStyle::bold}}};

  if (verbose) {
    fmt::println("Parser run 1");
  }

  for (size_t i = 0; i < tokens.size(); i++) {
    std::vector<Token> line = tokens[i];
    if (line.empty())
      continue;

    if (line[0].type == TOKEN_TYPE::LABEL_DEF) {
      std::string label_name = line[0].value_str;
      if (label_addresses.find(label_name) != label_addresses.end()) {
        throw std::runtime_error("Duplicate label definition: " + label_name);
      }
      label_addresses[label_name] = current_address;
      continue;
    } else if (line[0].type != TOKEN_TYPE::MNEMONIC) {
      throw std::runtime_error("Expected mnemonic at line " +
                               std::to_string(i + 1));
    }

    size_t operand_start =
        (line.size() > 1 && line[1].type == TOKEN_TYPE::FS) ? 2 : 1;
    if (line.size() > operand_start &&
        line[operand_start].type != TOKEN_TYPE::COMMA) {
      line.insert(line.begin() + operand_start, C());
    }

    int instructions_added = 0;

    Mnemonic mnemonic_enum = static_cast<Mnemonic>(line[0].value_int);
    std::string mnemonic = line[0].value_str;
    std::transform(mnemonic.begin(), mnemonic.end(), mnemonic.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    switch (mnemonic_enum) {
      // CATEGORY 00: NATIVE ALU OPERATIONS
      case M_ADD:
      case M_SUB:
      case M_MUL:
      case M_DIV:
      case M_MOD:
      case M_AND:
      case M_OR:
      case M_XOR: {
        if (matches_model(line, R_SHIFT_MODEL)) {
          instructions.push_back(Instruction(line, i + 1));
          instructions_added++;
        } else if (matches_model(line, R_MODEL)) {
          instructions.push_back(Instruction(line, i + 1));
          instructions_added++;
        } else if (matches_model(line, IA_MODEL)) {
          instructions.push_back(Instruction(line, i + 1));
          instructions_added++;
        } else if (matches_model(line, IB_MODEL)) {
          instructions.push_back(Instruction(line, i + 1));
          instructions_added++;
        } else {
          throw std::runtime_error(
              "Syntax Error: Invalid operands for ALU operation at line " +
              std::to_string(i + 1));
        }
        break;
      }
      case M_NOT:
      case M_CLZ: {
        if (!matches_model(line, UNARY_MODEL)) {
          throw std::runtime_error(
              "Syntax Error: Invalid operands for unary operation at line " +
              std::to_string(i + 1));
        }
        instructions.push_back(Instruction(line, i + 1));
        instructions_added++;
        break;
      }

      // CATEGORY 01: NATIVE BRANCHING & JUMPS
      case M_B:
      case M_BEQ:
      case M_BNE:
      case M_BLT:
      case M_BGE:
      case M_BLE:
      case M_BGT:
      case M_BCS:
      case M_BCC:
      case M_BMI:
      case M_BPL: {
        if (!matches_model(line, J_TYPE_MODEL)) {
          throw std::runtime_error(
              "Syntax Error: Invalid operands for J-type branch at line " +
              std::to_string(i + 1));
        }
        instructions.push_back(Instruction(line, i + 1));
        instructions_added++;
        break;
      }
      case M_BR:
      case M_BEQR:
      case M_BNER:
      case M_BLTR:
      case M_BGER:
      case M_BLER:
      case M_BGTR:
      case M_BCSR:
      case M_BCCR:
      case M_BMIR:
      case M_BPLR: {
        if (!matches_model(line, RBR_MODEL)) {
          throw std::runtime_error(
              "Syntax Error: Invalid operands for R-branch at line " +
              std::to_string(i + 1));
        }
        instructions.push_back(Instruction(line, i + 1));
        instructions_added++;
        break;
      }

      // CATEGORY 10: NATIVE MEMORY OPERATIONS
      case M_LW:
      case M_SW: {
        if (!matches_model(line, MEM_MODEL)) {
          throw std::runtime_error(
              "Syntax Error: Invalid operands for memory operation at line " +
              std::to_string(i + 1));
        }
        instructions.push_back(Instruction(line, i + 1));
        instructions_added++;
        break;
      }

      // PSEUDO-INSTRUCTIONS & MACROS (EXPANDED)
      case M_NOP: {
        if (!matches_model(line, NOP_MODEL))
          throw std::runtime_error("Syntax Error at line " +
                                   std::to_string(i + 1));
        instructions.push_back(Instruction(
            {M("add"), C(), R("r0"), C(), R("r0"), C(), I("#0")}, i + 1));
        instructions_added += 1;
        break;
      }
      case M_MOV: {
        if (!matches_model(line, MOV_MODEL))
          throw std::runtime_error("Syntax Error at line " +
                                   std::to_string(i + 1));
        std::string rd = line[2].value_str;
        std::string rs = line[4].value_str;
        instructions.push_back(Instruction(
            {M("add"), C(), R(rd), C(), R(rs), C(), I("#0")}, i + 1));
        instructions_added += 1;
        break;
      }
      case M_CLR: {
        if (!matches_model(line, CLR_MODEL))
          throw std::runtime_error("Syntax Error at line " +
                                   std::to_string(i + 1));
        std::string rd = line[2].value_str;
        instructions.push_back(Instruction(
            {M("add"), C(), R(rd), C(), R("r0"), C(), I("#0")}, i + 1));
        instructions_added += 1;
        break;
      }
      case M_NEG: {
        if (!matches_model(line, NEG_MODEL))
          throw std::runtime_error("Syntax Error at line " +
                                   std::to_string(i + 1));
        std::string rd = line[2].value_str;
        std::string rs = line[4].value_str;
        instructions.push_back(Instruction(
            {M("sub"), C(), R(rd), C(), I("#0"), C(), R(rs)}, i + 1));
        instructions_added += 1;
        break;
      }
      case M_LI: {
        if (!matches_model(line, LI_MODEL))
          throw std::runtime_error("Syntax Error at line " +
                                   std::to_string(i + 1));
        std::string rd = line[2].value_str;
        uint32_t val = static_cast<uint32_t>(line[4].value_int);
        std::vector<uint32_t> chunks;
        do {
          chunks.push_back(val & 0xFFF);
          val >>= 12;
        } while (val != 0);

        int highest_chunk = static_cast<int>(chunks.size()) - 1;
        while (highest_chunk > 0 && chunks[highest_chunk] == 0) {
          highest_chunk--;
        }

        instructions.push_back(
            Instruction({M("add"), C(), R(rd), C(), R("r0"), C(),
                         I("#" + std::to_string(chunks[highest_chunk]))},
                        i + 1));
        instructions_added++;

        for (int chunk_index = highest_chunk - 1; chunk_index >= 0;
             chunk_index--) {
          instructions.push_back(Instruction(
              {M("add"), C(), R(rd), C(), R("r0"), C(), R(rd), C(), S("lsl"),
               I("#12")},
              i + 1));
          instructions_added++;

          if (chunks[chunk_index] != 0) {
            instructions.push_back(
                Instruction({M("add"), C(), R(rd), C(), R(rd), C(),
                             I("#" + std::to_string(chunks[chunk_index]))},
                            i + 1));
            instructions_added++;
          }
        }
        break;
      }
      case M_PUSH: {
        if (!matches_model(line, PUSH_MODEL))
          throw std::runtime_error("Syntax Error at line " +
                                   std::to_string(i + 1));
        std::string rs = line[2].value_str;
        instructions.push_back(Instruction(
            {M("sub"), C(), R("r31"), C(), R("r31"), C(), I("#4")}, i + 1));
        instructions.push_back(Instruction(
            {M("sw"), C(), R(rs), C(), R("r31"), C(), I("#0")}, i + 1));
        instructions_added += 2;
        break;
      }
      case M_POP: {
        if (!matches_model(line, POP_MODEL))
          throw std::runtime_error("Syntax Error at line " +
                                   std::to_string(i + 1));
        std::string rd = line[2].value_str;
        instructions.push_back(Instruction(
            {M("lw"), C(), R(rd), C(), R("r31"), C(), I("#0")}, i + 1));
        instructions.push_back(Instruction(
            {M("add"), C(), R("r31"), C(), R("r31"), C(), I("#4")}, i + 1));
        instructions_added += 2;
        break;
      }
      case M_PUSHM: {
        for (size_t j = 2; j < line.size(); j += 2) {
          if (line[j].type != TOKEN_TYPE::REGISTER) {
            throw std::runtime_error(
                "Syntax Error: expected register at line " +
                std::to_string(i + 1));
          }
        }
        instructions.push_back(Instruction(
            {M("sub"), C(), R("r31"), C(), R("r31"), C(), I("#4")}, i + 1));
        instructions_added++;
        for (size_t j = 2; j < line.size(); j += 2) {
          if (line[j].type == TOKEN_TYPE::REGISTER) {
            instructions.push_back(
                Instruction({M("sw"), C(), R(line[j].value_str), C(), R("r31"),
                             C(), I("#0")},
                            i + 1));
            instructions_added++;
          }
        }
        break;
      }
      case M_POPM: {
        for (size_t j = 2; j < line.size(); j += 2) {
          if (line[j].type != TOKEN_TYPE::REGISTER) {
            throw std::runtime_error(
                "Syntax Error: expected register at line " +
                std::to_string(i + 1));
          }
        }
        for (size_t j = 2; j < line.size(); j += 2) {
          if (line[j].type == TOKEN_TYPE::REGISTER) {
            instructions.push_back(
                Instruction({M("lw"), C(), R(line[j].value_str), C(), R("r31"),
                             C(), I("#0")},
                            i + 1));
            instructions_added++;
          }
        }
        instructions.push_back(Instruction(
            {M("add"), C(), R("r31"), C(), R("r31"), C(), I("#4")}, i + 1));
        instructions_added++;
        break;
      }
      case M_LSL:
      case M_LSR:
      case M_ASR: {
        if (!matches_model(line, SHIFT_MODEL))
          throw std::runtime_error("Syntax Error at line " +
                                   std::to_string(i + 1));
        std::string rd = line[2].value_str;
        std::string rs = line[4].value_str;
        std::string amt = line[6].value_str;
        instructions.push_back(
            Instruction({M("add"), C(), R(rd), C(), R("r0"), C(), R(rs), C(),
                         S(mnemonic), I(amt)},
                        i + 1));
        instructions_added += 1;
        break;
      }
      case M_INC: {
        if (!matches_model(line, INC_MODEL))
          throw std::runtime_error("Syntax Error at line " +
                                   std::to_string(i + 1));
        std::string rd = line[2].value_str;
        instructions.push_back(Instruction(
            {M("add"), C(), R(rd), C(), R(rd), C(), I("#1")}, i + 1));
        instructions_added += 1;
        break;
      }
      case M_DEC: {
        if (!matches_model(line, INC_MODEL))
          throw std::runtime_error("Syntax Error at line " +
                                   std::to_string(i + 1));
        std::string rd = line[2].value_str;
        instructions.push_back(Instruction(
            {M("sub"), C(), R(rd), C(), R(rd), C(), I("#1")}, i + 1));
        instructions_added += 1;
        break;
      }
      case M_SWAP: {
        if (!matches_model(line, SWAP_MODEL))
          throw std::runtime_error("Syntax Error at line " +
                                   std::to_string(i + 1));
        std::string r1 = line[2].value_str;
        std::string r2 = line[4].value_str;
        instructions.push_back(
            Instruction({M("xor"), C(), R(r1), C(), R(r1), C(), R(r2)}, i + 1));
        instructions.push_back(
            Instruction({M("xor"), C(), R(r2), C(), R(r1), C(), R(r2)}, i + 1));
        instructions.push_back(
            Instruction({M("xor"), C(), R(r1), C(), R(r1), C(), R(r2)}, i + 1));
        instructions_added += 3;
        break;
      }
      case M_CMP: {
        if (!matches_model(line, CMP_MODEL))
          throw std::runtime_error("Syntax Error at line " +
                                   std::to_string(i + 1));
        std::string rs1 = line[2].value_str;
        std::string rs2 = line[4].value_str;
        instructions.push_back(Instruction(
            {M("sub"), C(), R("r0"), C(), R(rs1), C(), R(rs2)}, i + 1));
        instructions_added += 1;
        break;
      }
      case M_CMN: {
        if (!matches_model(line, CMP_MODEL))
          throw std::runtime_error("Syntax Error at line " +
                                   std::to_string(i + 1));
        std::string rs1 = line[2].value_str;
        std::string rs2 = line[4].value_str;
        instructions.push_back(Instruction(
            {M("add"), C(), R("r0"), C(), R(rs1), C(), R(rs2)}, i + 1));
        instructions_added += 1;
        break;
      }
      case M_TST: {
        if (!matches_model(line, CMP_MODEL))
          throw std::runtime_error("Syntax Error at line " +
                                   std::to_string(i + 1));
        std::string rs1 = line[2].value_str;
        std::string rs2 = line[4].value_str;
        instructions.push_back(Instruction(
            {M("and"), C(), R("r0"), C(), R(rs1), C(), R(rs2)}, i + 1));
        instructions_added += 1;
        break;
      }
      case M_TEQ: {
        if (!matches_model(line, CMP_MODEL))
          throw std::runtime_error("Syntax Error at line " +
                                   std::to_string(i + 1));
        std::string rs1 = line[2].value_str;
        std::string rs2 = line[4].value_str;
        instructions.push_back(Instruction(
            {M("xor"), C(), R("r0"), C(), R(rs1), C(), R(rs2)}, i + 1));
        instructions_added += 1;
        break;
      }
      case M_JMP: {
        if (!matches_model(line, J_MODEL))
          throw std::runtime_error("Syntax Error at line " +
                                   std::to_string(i + 1));
        instructions.push_back(
            Instruction({M("b"), C(), R("r0"), C(), line[2]}, i + 1));
        instructions_added += 1;
        break;
      }
      case M_CALL: {
        if (!matches_model(line, J_MODEL))
          throw std::runtime_error("Syntax Error at line " +
                                   std::to_string(i + 1));
        instructions.push_back(
            Instruction({M("b"), C(), R("r30"), C(), line[2]}, i + 1));
        instructions_added += 1;
        break;
      }
      case M_JMPR: {
        if (!matches_model(line, J_TYPE_MODEL))
          throw std::runtime_error("Syntax Error at line " +
                                   std::to_string(i + 1));
        instructions.push_back(
            Instruction({M("b"), C(), line[2], C(), line[4]}, i + 1));
        instructions_added += 1;
        break;
      }
      case M_RET: {
        if (!matches_model(line, NOP_MODEL))
          throw std::runtime_error("Syntax Error at line " +
                                   std::to_string(i + 1));
        instructions.push_back(Instruction({M("br"), C(), R("r30")}, i + 1));
        instructions_added += 1;
        break;
      }
      case M_BEQZ: {
        if (!matches_model(line, BEQZ_MODEL))
          throw std::runtime_error("Syntax Error at line " +
                                   std::to_string(i + 1));
        instructions.push_back(Instruction(
            {M("sub"), C(), R("r0"), C(), line[2], C(), R("r0")}, i + 1));
        instructions.push_back(
            Instruction({M("beq"), C(), R("r0"), C(), line[4]}, i + 1));
        instructions_added += 2;
        break;
      }
      case M_BNEZ: {
        if (!matches_model(line, BEQZ_MODEL))
          throw std::runtime_error("Syntax Error at line " +
                                   std::to_string(i + 1));
        instructions.push_back(Instruction(
            {M("sub"), C(), R("r0"), C(), line[2], C(), R("r0")}, i + 1));
        instructions.push_back(
            Instruction({M("bne"), C(), R("r0"), C(), line[4]}, i + 1));
        instructions_added += 2;
        break;
      }
      case M_ABS: {
        if (!matches_model(line, ABS_MODEL))
          throw std::runtime_error("Syntax Error at line " +
                                   std::to_string(i + 1));
        std::string rd = line[2].value_str;
        std::string rs = line[4].value_str;
        instructions.push_back(Instruction(
            {M("add"), C(), R(rd), C(), R(rs), C(), I("#0")}, i + 1));
        instructions.push_back(Instruction(
            {M("sub"), C(), R("r0"), C(), R(rd), C(), R("r0")}, i + 1));
        instructions.push_back(
            Instruction({M("bge"), C(), R("r0"), C(), I("#2")}, i + 1));
        instructions.push_back(Instruction(
            {M("sub"), C(), R(rd), C(), I("#0"), C(), R(rd)}, i + 1));
        instructions_added += 4;
        break;
      }
      case M_SEQ:
      case M_SNE:
      case M_SLT:
      case M_SGE: {
        if (!matches_model(line, SEQ_MODEL))
          throw std::runtime_error("Syntax Error at line " +
                                   std::to_string(i + 1));
        std::string rd = line[2].value_str;
        std::string rs1 = line[4].value_str;
        std::string rs2 = line[6].value_str;

        std::string branch_op = "bne";
        if (mnemonic_enum == M_SNE)
          branch_op = "beq";
        else if (mnemonic_enum == M_SLT)
          branch_op = "bge";
        else if (mnemonic_enum == M_SGE)
          branch_op = "blt";

        instructions.push_back(Instruction(
            {M("sub"), C(), R("r0"), C(), R(rs1), C(), R(rs2)}, i + 1));
        instructions.push_back(Instruction(
            {M("add"), C(), R(rd), C(), R("r0"), C(), I("#1")}, i + 1));
        instructions.push_back(
            Instruction({M(branch_op), C(), R("r0"), C(), I("#2")}, i + 1));
        instructions.push_back(Instruction(
            {M("add"), C(), R(rd), C(), R("r0"), C(), I("#0")}, i + 1));
        instructions_added += 4;
        break;
      }
      default:
        throw std::runtime_error("Unknown mnemonic: " + mnemonic);
    }

    current_address += 4 * instructions_added;  // each instruction is 4 bytes

    if (verbose) {
      bar.set_progress((i + 1) * 100 / tokens.size());
    }
  }

  indicators::BlockProgressBar bar2{
      indicators::option::BarWidth{80},
      indicators::option::Start{"["},
      indicators::option::End{"]"},
      indicators::option::ForegroundColor{indicators::Color::white},
      indicators::option::ShowPercentage{true},
      indicators::option::FontStyles{
          std::vector<indicators::FontStyle>{indicators::FontStyle::bold}}};

  if (verbose) {
    fmt::println("Parser run 2: resolving labels");
  }

  for (size_t i = 0; i < instructions.size(); i++) {
    instructions[i].resolve_labels(label_addresses, i * 4);
    if (verbose) {
      bar2.set_progress((i + 1) * 100 / instructions.size());
    }
  }
};
}  // namespace sam32asm
