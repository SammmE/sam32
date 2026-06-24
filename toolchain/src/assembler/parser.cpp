#include <algorithm>
#include <iterator>
#include <stdexcept>
#include <string>

#include <sam32/assembler/lexer.hpp>
#include <sam32/assembler/parser.hpp>

namespace sam32 {

Parser::Parser(const std::vector<Token>& tokens, bool allow_placeholders) : tokens(tokens), pos(0), allow_placeholders(allow_placeholders) {
  text_segment = Segment{{}, 0x00000000, 0};
  data_segment = Segment{{}, 0x00000000, 0};
  current_segment = &text_segment;
}

Token Parser::current() const {
  if (pos < tokens.size())
    return tokens[pos];
  return Token(TOKEN_EOF, "", 0, 0);
}

Token Parser::peek() const {
  if (pos + 1 < tokens.size())
    return tokens[pos + 1];
  return Token(TOKEN_EOF, "", 0, 0);
}

Token Parser::consume() {
  Token t = current();
  pos++;
  return t;
}

void Parser::skip_newlines() {
  while (current().type == TOKEN_NEWLINE)
    pos++;
}

static int32_t parse_int(const std::string& s) {
  bool negative = (!s.empty() && s[0] == '-');
  const std::string body = negative ? s.substr(1) : s;

  int32_t result;
  if (body.size() > 2 && body[0] == '0' && (body[1] == 'b' || body[1] == 'B')) {
    // std::stol base 0 does not handle 0b, so parse binary manually
    result = static_cast<int32_t>(std::stoul(body.substr(2), nullptr, 2));
  } else {
    result = static_cast<int32_t>(std::stol(body, nullptr, 0));
  }

  return negative ? -result : result;
}

static int find_mnemonic(const std::string& name) {
  std::string lower = name;
  std::transform(lower.begin(), lower.end(), lower.begin(),
                 [](unsigned char c) { return std::tolower(c); });

  for (size_t i = 0; i < std::size(mnemonic_names); ++i) {
    if (mnemonic_names[i] == lower)
      return static_cast<int>(i);
  }
  return -1;
}

static int find_directive(const std::string& name) {
  std::string lower = name;
  std::transform(lower.begin(), lower.end(), lower.begin(),
                 [](unsigned char c) { return std::tolower(c); });

  for (size_t i = 0; i < std::size(directive_names); ++i) {
    if (directive_names[i] == lower)
      return static_cast<int>(i);
  }
  return -1;
}

static bool try_parse_register(const Token& tok, Operand& op) {
  if (tok.type != TOKEN_IDENTIFIER)
    return false;

  std::string s = tok.value;
  std::transform(s.begin(), s.end(), s.begin(),
                 [](unsigned char c) { return std::tolower(c); });

  if (s == "sp") {
    op = Operand::Register(31);
    return true;
  }
  if (s == "lr") {
    op = Operand::Register(30);
    return true;
  }
  if (s == "zr") {
    op = Operand::Register(0);
    return true;
  }

  if ((s[0] == 'r') && s.size() > 1) {
    const std::string num_part = s.substr(1);
    if (std::all_of(num_part.begin(), num_part.end(),
                    [](unsigned char c) { return std::isdigit(c); })) {
      int reg = std::stoi(num_part);
      if (reg >= 0 && reg <= 31) {
        op = Operand::Register(reg);
        return true;
      }
    }
  }
  return false;
}

static bool is_shift_keyword(const std::string& s) {
  std::string low = s;
  std::transform(low.begin(), low.end(), low.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  return low == "lsl" || low == "lsr" || low == "asr" || low == "ror";
}

static ShiftType shift_keyword_to_type(const std::string& s) {
  std::string low = s;
  std::transform(low.begin(), low.end(), low.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  if (low == "lsl")
    return SHIFT_LSL;
  if (low == "lsr")
    return SHIFT_LSR;
  if (low == "asr")
    return SHIFT_ASR;
  return SHIFT_ROR;
}

void Parser::parse_directive(Directive directive, const Token& dir_tok) {
  switch (directive) {
    case D_DATA:
      current_segment = &data_segment;
      break;

    case D_TEXT:
      current_segment = &text_segment;
      break;

    case D_ORG: {
      Token num = current();
      if (num.type != TOKEN_NUMBER) {
        throw std::runtime_error("Expected number after .org at line " +
                                 std::to_string(dir_tok.line));
      }
      current_segment->addr_base = static_cast<size_t>(parse_int(num.value));
      consume();
    } break;

    case D_EQU: {
      Token name_tok = current();
      if (name_tok.type != TOKEN_IDENTIFIER) {
        throw std::runtime_error("Expected identifier after .equ at line " +
                                 std::to_string(dir_tok.line));
      }
      std::string const_name = name_tok.value;
      consume();

      if (current().type == TOKEN_COMMA)
        consume();

      Token val_tok = current();
      if (val_tok.type != TOKEN_NUMBER && val_tok.type != TOKEN_IMMEDIATE) {
        throw std::runtime_error(
            "Expected numeric value after .equ name at line " +
            std::to_string(dir_tok.line));
      }
      symbol_table[const_name] = {
          nullptr, static_cast<size_t>(parse_int(val_tok.value))};
      consume();
    } break;

    case D_BYTE:
    case D_HALF:
    case D_WORD: {
      uint8_t size = (directive == D_BYTE) ? 1 : (directive == D_HALF) ? 2 : 4;

      while (current().type != TOKEN_NEWLINE && current().type != TOKEN_EOF) {
        if (current().type == TOKEN_COMMA) {
          consume();
          continue;
        }

        Token num = current();
        if (num.type == TOKEN_IDENTIFIER) {
          // allow label references in .data directives, but resolve them as
          // absolute addresses in the second pass
          current_segment->instructions.push_back(
              Data(num.value, num.line, current_segment->cursor));
          current_segment->cursor += size;
          consume();
          continue;
        }

        if (num.type != TOKEN_NUMBER && num.type != TOKEN_IMMEDIATE) {
          throw std::runtime_error("Expected numeric value in " +
                                   dir_tok.value + " list at line " +
                                   std::to_string(dir_tok.line));
        }
        uint32_t value = static_cast<uint32_t>(parse_int(num.value));
        current_segment->instructions.push_back(
            Data{value, size, num.line, current_segment->cursor});
        current_segment->cursor += size;
        consume();
      }
    } break;

    case D_STRING: {
      Token str = current();
      if (str.type != TOKEN_STRING) {
        throw std::runtime_error(
            "Expected string literal after .string at line " +
            std::to_string(dir_tok.line));
      }
      for (char c : str.value) {
        current_segment->instructions.push_back(Data{
            static_cast<uint8_t>(c), 1, str.line, current_segment->cursor});
        current_segment->cursor += 1;
      }
      // Null-terminate
      current_segment->instructions.push_back(
          Data{0, 1, str.line, current_segment->cursor});
      current_segment->cursor += 1;
      consume();
    } break;

    case D_SPACE: {
      Token num = current();
      if (num.type != TOKEN_NUMBER) {
        throw std::runtime_error("Expected number after .space at line " +
                                 std::to_string(dir_tok.line));
      }
      size_t count = static_cast<size_t>(parse_int(num.value));
      for (size_t i = 0; i < count; ++i) {
        current_segment->instructions.push_back(
            Data{0, 1, dir_tok.line, current_segment->cursor});
        current_segment->cursor += 1;
      }
      consume();
    } break;

    case D_INCLUDE: {
      Token filename_tok = current();
      if (filename_tok.type != TOKEN_STRING) {
        throw std::runtime_error(
            "Expected string literal (filename) after .include at line " +
            std::to_string(dir_tok.line));
      }
      std::string filename = filename_tok.value;
      consume();

      Lexer included_lexer(filename);
      included_lexer.tokenize();

      tokens.insert(tokens.begin() + static_cast<long>(pos),
                    included_lexer.tokens.begin(), included_lexer.tokens.end());
    } break;
  }
}

std::vector<Operand> Parser::parse_operands(bool& has_shift,
                                            ShiftType& shift_type,
                                            uint8_t& shift_amt) {
  std::vector<Operand> operands;
  has_shift = false;
  shift_type = SHIFT_LSL;
  shift_amt = 0;

  while (current().type != TOKEN_NEWLINE && current().type != TOKEN_EOF) {
    Token tok = current();

    if (tok.type == TOKEN_COMMA) {
      consume();
      continue;
    }

    Operand reg_op{OperandType::REGISTER, 0, 0, ""};
    if (try_parse_register(tok, reg_op)) {
      consume();
      operands.push_back(reg_op);
      continue;
    }

    if (tok.type == TOKEN_IMMEDIATE) {
      operands.push_back(Operand::Immediate(parse_int(tok.value)));
      consume();
      continue;
    }

    if (tok.type == TOKEN_NUMBER) {
      operands.push_back(Operand::Immediate(parse_int(tok.value)));
      consume();
      continue;
    }

    if (tok.type == TOKEN_IDENTIFIER && is_shift_keyword(tok.value)) {
      shift_type = shift_keyword_to_type(tok.value);
      consume();

      Token amt_tok = current();
      if (amt_tok.type != TOKEN_IMMEDIATE && amt_tok.type != TOKEN_NUMBER) {
        throw std::runtime_error("Expected shift amount after " + tok.value +
                                 " at line " + std::to_string(tok.line));
      }
      int32_t amt = parse_int(amt_tok.value);
      if (amt < 0 || amt > 31) {
        throw std::runtime_error("Shift amount out of range [0,31] at line " +
                                 std::to_string(amt_tok.line));
      }
      shift_amt = static_cast<uint8_t>(amt);
      has_shift = true;
      consume();
      continue;
    }

    if (tok.type == TOKEN_IDENTIFIER) {
      operands.push_back(Operand::Label(tok.value));
      consume();
      continue;
    }

    throw std::runtime_error(
        "Unexpected token '" + tok.value + "' in operand list at line " +
        std::to_string(tok.line) + ", column " + std::to_string(tok.column));
  }

  return operands;
}

// First pass: build instruction stream and populate symbol table.
// Forward references are stored as labels to be resolved in the second pass.
void Parser::parse() {
  while (current().type != TOKEN_EOF) {
    if (current().type == TOKEN_NEWLINE) {
      consume();
      continue;
    }

    Token tok = current();

    if (tok.type == TOKEN_DIRECTIVE) {
      int dir_idx = find_directive(tok.value);
      if (dir_idx < 0) {
        throw std::runtime_error("Unknown directive '" + tok.value +
                                 "' at line " + std::to_string(tok.line));
      }
      consume();
      parse_directive(static_cast<Directive>(dir_idx), tok);

      if (current().type == TOKEN_NEWLINE)
        consume();
      continue;
    }

    if (tok.type == TOKEN_IDENTIFIER) {
      if (peek().type == TOKEN_COLON) {
        symbol_table[tok.value] = {&current_segment->addr_base,
                                   current_segment->cursor};
        consume();
        consume();
        continue;
      }

      // Handle EQU constant: LABEL EQU VALUE
      {
        std::string lower_val = tok.value;
        std::transform(lower_val.begin(), lower_val.end(), lower_val.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        if (lower_val == "equ") {
          throw std::runtime_error(
              "Unexpected 'EQU' without preceding label at line " +
              std::to_string(tok.line));
        }
      }

      if (peek().type == TOKEN_IDENTIFIER) {
        std::string next_lower = peek().value;
        std::transform(next_lower.begin(), next_lower.end(), next_lower.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        if (next_lower == "equ") {
          std::string const_name = tok.value;
          consume();
          consume();

          Token val_tok = current();
          if (val_tok.type != TOKEN_NUMBER && val_tok.type != TOKEN_IMMEDIATE) {
            throw std::runtime_error(
                "Expected numeric value after EQU at line " +
                std::to_string(val_tok.line));
          }
          symbol_table[const_name] = {
              nullptr, static_cast<size_t>(parse_int(val_tok.value))};
          consume();
          if (current().type == TOKEN_NEWLINE)
            consume();
          continue;
        }
      }

      // Handle mnemonic instructions (optionally with .F suffix)
      std::string raw_mnem = tok.value;
      bool freeze_flag = false;

      // Case 1: lexer merged ".F" into the identifier (e.g. "SUB.F")
      auto dot_pos = raw_mnem.find('.');
      if (dot_pos != std::string::npos) {
        std::string suffix = raw_mnem.substr(dot_pos + 1);
        std::transform(suffix.begin(), suffix.end(), suffix.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        if (suffix == "f") {
          freeze_flag = true;
          raw_mnem = raw_mnem.substr(0, dot_pos);
        }
      }

      int mnem_idx = find_mnemonic(raw_mnem);
      if (mnem_idx < 0) {
        throw std::runtime_error("Unknown mnemonic '" + tok.value +
                                 "' at line " + std::to_string(tok.line));
      }

      Mnemonic mnemonic = static_cast<Mnemonic>(mnem_idx);
      size_t instr_line = tok.line;
      consume();

      // Case 2: lexer emitted ".F" as a separate TOKEN_DIRECTIVE
      if (!freeze_flag && current().type == TOKEN_DIRECTIVE) {
        std::string dir_val = current().value;
        std::transform(dir_val.begin(), dir_val.end(), dir_val.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        if (dir_val == ".f") {
          freeze_flag = true;
          consume();
        }
      }

      bool has_shift = false;
      ShiftType stype = SHIFT_LSL;
      uint8_t samt = 0;
      std::vector<Operand> operands = parse_operands(has_shift, stype, samt);

      ParsedInstruction instr(mnemonic, operands, freeze_flag, has_shift, stype,
                              samt);

      instr.line_number = instr_line;
      verify_instruction(instr);
      flush_instruction(instr);

      if (current().type == TOKEN_NEWLINE)
        consume();
      continue;
    }

    throw std::runtime_error("Unexpected token '" + tok.value + "' at line " +
                             std::to_string(tok.line) + ", column " +
                             std::to_string(tok.column));
  }

  // get the last .text segment offset for the .data segment's start address
  data_segment.addr_base = text_segment.addr_base + text_segment.cursor + 4;

  // PASS 2
  // resolve label references
  for (auto& instr_or_data : text_segment.instructions) {
    if (std::holds_alternative<ParsedInstruction>(instr_or_data)) {
      ParsedInstruction& instr = std::get<ParsedInstruction>(instr_or_data);

      // Determine if this instruction is a PC-relative J-Type branch
      bool is_pc_relative_branch =
          (instr.mnemonic >= M_B && instr.mnemonic <= M_BPL);

      for (Operand& op : instr.operands) {
        if (op.type == OperandType::LABEL) {
          auto it = symbol_table.find(op.label);
          if (it == symbol_table.end()) {
            throw std::runtime_error("Undefined label '" + op.label +
                                     "' at line " +
                                     std::to_string(instr.line_number));
          }

          size_t target_base = it->second.first ? *(it->second.first) : 0;
          size_t target_offset = it->second.second;
          size_t target_addr = target_base + target_offset;

          op.type = OperandType::IMMEDIATE;

          if (op.is_offset && is_pc_relative_branch) {
            // Calculate relative offset in INSTRUCTIONS
            size_t current_pc = text_segment.addr_base + instr.addr_offset;
            int32_t relative_bytes = static_cast<int32_t>(target_addr) -
                                     static_cast<int32_t>(current_pc);

            if (relative_bytes % 4 != 0) {
              throw std::runtime_error(
                  "Branch target is not word-aligned at line " +
                  std::to_string(instr.line_number));
            }

            int32_t relative_instructions = relative_bytes / 4;
            op.value = static_cast<uint32_t>(relative_instructions);
          } else {
            if (op.chunk_idx == 1) {
              target_addr &= 0xFFF;
            } else if (op.chunk_idx == 2) {
              target_addr = (target_addr >> 12) & 0xFFF;
            } else if (op.chunk_idx == 3) {
              target_addr = (target_addr >> 24) & 0xFF;
            }
            op.value = static_cast<uint32_t>(target_addr);
          }
        }

        // Range Checking for all IMMEDIATE operands
        if (op.type == OperandType::IMMEDIATE) {
          int32_t signed_val = static_cast<int32_t>(op.value);

          if (op.is_offset && is_pc_relative_branch) {
            // 18-bit signed branch offset (instructions)
            if (signed_val < -131072 || signed_val > 131071) {
              throw std::runtime_error(
                  "Branch offset out of range (+-131,072 instructions) at "
                  "line " +
                  std::to_string(instr.line_number));
            }
          } else if (op.is_offset) {
            // 13-bit signed memory offset (bytes)
            if (signed_val < -4096 || signed_val > 4095) {
              throw std::runtime_error(
                  "Memory offset out of range (+-4,096 bytes) at line " +
                  std::to_string(instr.line_number));
            }
          } else {
            // 13-bit signed ALU immediate
            if (signed_val < -4096 || signed_val > 4095) {
              throw std::runtime_error(
                  "Immediate value out of range (+-4,096) at line " +
                  std::to_string(instr.line_number));
            }
          }
        }

        if (op.type == OperandType::REGISTER && op.reg_id > 31) {
          throw std::runtime_error("Register number out of range at line " +
                                   std::to_string(instr.line_number));
        }
      }
    } else {
      Data& data = std::get<Data>(instr_or_data);
      if (data.value.type == OperandType::LABEL) {
        auto it = symbol_table.find(data.value.label);
        if (it == symbol_table.end()) {
          throw std::runtime_error("Undefined label '" + data.value.label +
                                   "' at line " +
                                   std::to_string(data.line_number));
        }
        size_t base = it->second.first ? *(it->second.first) : 0;
        size_t offset = it->second.second;
        data.value.type = OperandType::IMMEDIATE;
        data.value.value = static_cast<uint32_t>(base + offset);
      }
    }
  }
}

void Parser::flush_instruction(ParsedInstruction& instr) {
  std::vector<ParsedInstruction> expanded_instrs;
  bool is_pseudo = false;

  switch (instr.mnemonic) {
    case M_NOP:
      expanded_instrs.push_back(ParsedInstruction(
          M_ADD,
          {Operand::Register(0), Operand::Register(0), Operand::Immediate(0)},
          false, false, SHIFT_LSL, 0));
      is_pseudo = true;
      break;

    case M_MOV:
      if (instr.operands.at(1).type == OperandType::REGISTER) {
        expanded_instrs.push_back(ParsedInstruction(
            M_ADD,
            {instr.operands.at(0), instr.operands.at(1), Operand::Immediate(0)},
            false, false, SHIFT_LSL, 0));
      } else {
        expanded_instrs.push_back(ParsedInstruction(
            M_ADD,
            {instr.operands.at(0), Operand::Register(0), instr.operands.at(1)},
            false, false, SHIFT_LSL, 0));
      }
      is_pseudo = true;
      break;

    case M_CLR:
      expanded_instrs.push_back(ParsedInstruction(
          M_ADD,
          {instr.operands.at(0), Operand::Register(0), Operand::Immediate(0)},
          false, false, SHIFT_LSL, 0));
      is_pseudo = true;
      break;

    case M_NEG:
      expanded_instrs.push_back(ParsedInstruction(
          M_SUB,
          {instr.operands.at(0), Operand::Immediate(0), instr.operands.at(1)},
          false, false, SHIFT_LSL, 0));
      is_pseudo = true;
      break;

    case M_LA:
    case M_LI: {
      uint32_t val32 = 0;
      bool is_label = false;
      std::string label_name = "";

      const Operand& rd = instr.operands.at(0);
      const Operand& src = instr.operands.at(1);

      if (src.type == OperandType::IMMEDIATE) {
        val32 = src.value;
      } else if (src.type == OperandType::LABEL) {
        is_label = true;
        label_name = src.label;
        auto it = symbol_table.find(label_name);
        if (it != symbol_table.end()) {
          size_t base = it->second.first ? *(it->second.first) : 0;
          if (it->second.first == &data_segment.addr_base && base == 0) {
            base = text_segment.addr_base + text_segment.cursor + 4;
          }
          val32 = static_cast<uint32_t>(base + it->second.second);
        } else {
          val32 = 0xFFFFFFFF;
        }
      }

      uint32_t c0 = val32 & 0xFFF;
      uint32_t c1 = (val32 >> 12) & 0xFFF;
      uint32_t c2 = (val32 >> 24) & 0xFF;

      auto make_op = [&](uint32_t val, uint8_t chunk_idx) {
        if (is_label) {
          return Operand::Label(label_name, false, chunk_idx);
        }
        return Operand::Immediate(val);
      };

      if (c2 != 0 || is_label) {
        expanded_instrs.push_back(ParsedInstruction(M_ADD, {rd, Operand::Register(0), make_op(c2, 3)}, false, false, SHIFT_LSL, 0));
        expanded_instrs.push_back(ParsedInstruction(M_ADD, {rd, Operand::Register(0), rd}, false, true, SHIFT_LSL, 12));
        expanded_instrs.push_back(ParsedInstruction(M_ADD, {rd, rd, make_op(c1, 2)}, false, false, SHIFT_LSL, 0));
        expanded_instrs.push_back(ParsedInstruction(M_ADD, {rd, Operand::Register(0), rd}, false, true, SHIFT_LSL, 12));
        expanded_instrs.push_back(ParsedInstruction(M_ADD, {rd, rd, make_op(c0, 1)}, false, false, SHIFT_LSL, 0));
      } else if (c1 != 0) {
        expanded_instrs.push_back(ParsedInstruction(M_ADD, {rd, Operand::Register(0), make_op(c1, 2)}, false, false, SHIFT_LSL, 0));
        expanded_instrs.push_back(ParsedInstruction(M_ADD, {rd, Operand::Register(0), rd}, false, true, SHIFT_LSL, 12));
        expanded_instrs.push_back(ParsedInstruction(M_ADD, {rd, rd, make_op(c0, 1)}, false, false, SHIFT_LSL, 0));
      } else {
        expanded_instrs.push_back(ParsedInstruction(M_ADD, {rd, Operand::Register(0), make_op(c0, 1)}, false, false, SHIFT_LSL, 0));
      }
      is_pseudo = true;
      break;
    }

    case M_LUI: {
      const Operand& rd = instr.operands.at(0);
      uint32_t val32 = instr.operands.at(1).value & 0x1FFF;
      expanded_instrs.push_back(ParsedInstruction(M_ADD, {rd, Operand::Register(0), Operand::Immediate(val32)}, false, true, SHIFT_LSL, 19));
      is_pseudo = true;
      break;
    }

    case M_EIC:
      expanded_instrs.push_back(ParsedInstruction(
          M_CSRW,
          {Operand::Immediate(0x04), Operand::Immediate(1)},
          false, false, SHIFT_LSL, 0));
      is_pseudo = true;
      break;

    case M_DIC:
      expanded_instrs.push_back(ParsedInstruction(
          M_CSRW,
          {Operand::Immediate(0x04), Operand::Immediate(0)},
          false, false, SHIFT_LSL, 0));
      is_pseudo = true;
      break;

    case M_PUSH:
      expanded_instrs.push_back(ParsedInstruction(
          M_SUB,
          {Operand::Register(31), Operand::Register(31), Operand::Immediate(4)},
          false, false, SHIFT_LSL, 0));
      expanded_instrs.push_back(
          ParsedInstruction(M_SW,
                            {instr.operands.at(0), Operand::Register(31),
                             Operand::Immediate(0, true)},
                            false, false, SHIFT_LSL, 0));
      is_pseudo = true;
      break;

    case M_POP:
      expanded_instrs.push_back(
          ParsedInstruction(M_LW,
                            {instr.operands.at(0), Operand::Register(31),
                             Operand::Immediate(0, true)},
                            false, false, SHIFT_LSL, 0));
      expanded_instrs.push_back(ParsedInstruction(
          M_ADD,
          {Operand::Register(31), Operand::Register(31), Operand::Immediate(4)},
          false, false, SHIFT_LSL, 0));
      is_pseudo = true;
      break;

    case M_LSL:
      expanded_instrs.push_back(ParsedInstruction(
          M_ADD,
          {instr.operands.at(0), Operand::Register(0), instr.operands.at(1)},
          false, true, SHIFT_LSL,
          static_cast<uint8_t>(instr.operands.at(2).value)));
      is_pseudo = true;
      break;

    case M_LSR:
      expanded_instrs.push_back(ParsedInstruction(
          M_ADD,
          {instr.operands.at(0), Operand::Register(0), instr.operands.at(1)},
          false, true, SHIFT_LSR,
          static_cast<uint8_t>(instr.operands.at(2).value)));
      is_pseudo = true;
      break;

    case M_ASR:
      expanded_instrs.push_back(ParsedInstruction(
          M_ADD,
          {instr.operands.at(0), Operand::Register(0), instr.operands.at(1)},
          false, true, SHIFT_ASR,
          static_cast<uint8_t>(instr.operands.at(2).value)));
      is_pseudo = true;
      break;

    case M_ROR:
      expanded_instrs.push_back(ParsedInstruction(
          M_ADD,
          {instr.operands.at(0), Operand::Register(0), instr.operands.at(1)},
          false, true, SHIFT_ROR,
          static_cast<uint8_t>(instr.operands.at(2).value)));
      is_pseudo = true;
      break;

    case M_INC:
      expanded_instrs.push_back(ParsedInstruction(
          M_ADD,
          {instr.operands.at(0), instr.operands.at(0), Operand::Immediate(1)},
          false, false, SHIFT_LSL, 0));
      is_pseudo = true;
      break;

    case M_DEC:
      expanded_instrs.push_back(ParsedInstruction(
          M_SUB,
          {instr.operands.at(0), instr.operands.at(0), Operand::Immediate(1)},
          false, false, SHIFT_LSL, 0));
      is_pseudo = true;
      break;

    case M_CMP:
      expanded_instrs.push_back(ParsedInstruction(
          M_SUB,
          {Operand::Register(0), instr.operands.at(0), instr.operands.at(1)},
          false, false, SHIFT_LSL, 0));
      is_pseudo = true;
      break;

    case M_CMN:
      expanded_instrs.push_back(ParsedInstruction(
          M_ADD,
          {Operand::Register(0), instr.operands.at(0), instr.operands.at(1)},
          false, false, SHIFT_LSL, 0));
      is_pseudo = true;
      break;

    case M_TST:
      expanded_instrs.push_back(ParsedInstruction(
          M_AND,
          {Operand::Register(0), instr.operands.at(0), instr.operands.at(1)},
          false, false, SHIFT_LSL, 0));
      is_pseudo = true;
      break;

    case M_TEQ:
      expanded_instrs.push_back(ParsedInstruction(
          M_XOR,
          {Operand::Register(0), instr.operands.at(0), instr.operands.at(1)},
          false, false, SHIFT_LSL, 0));
      is_pseudo = true;
      break;

    case M_JMP:
      expanded_instrs.push_back(
          ParsedInstruction(M_B, {Operand::Register(0), instr.operands.at(0)},
                            false, false, SHIFT_LSL, 0));
      is_pseudo = true;
      break;

    case M_CALL:
      expanded_instrs.push_back(
          ParsedInstruction(M_B, {Operand::Register(30), instr.operands.at(0)},
                            false, false, SHIFT_LSL, 0));
      is_pseudo = true;
      break;

    case M_RET:
      expanded_instrs.push_back(ParsedInstruction(M_BR, {Operand::Register(30)},
                                                  false, false, SHIFT_LSL, 0));
      is_pseudo = true;
      break;

    case M_BEQZ:
      expanded_instrs.push_back(ParsedInstruction(
          M_SUB,
          {Operand::Register(0), instr.operands.at(0), Operand::Register(0)},
          false, false, SHIFT_LSL, 0));
      expanded_instrs.push_back(
          ParsedInstruction(M_BEQ, {Operand::Register(0), instr.operands.at(1)},
                            false, false, SHIFT_LSL, 0));
      is_pseudo = true;
      break;

    case M_BNEZ:
      expanded_instrs.push_back(ParsedInstruction(
          M_SUB,
          {Operand::Register(0), instr.operands.at(0), Operand::Register(0)},
          false, false, SHIFT_LSL, 0));
      expanded_instrs.push_back(
          ParsedInstruction(M_BNE, {Operand::Register(0), instr.operands.at(1)},
                            false, false, SHIFT_LSL, 0));
      is_pseudo = true;
      break;

    case M_ABS:
      expanded_instrs.push_back(ParsedInstruction(
          M_ADD,
          {instr.operands.at(0), instr.operands.at(1), Operand::Immediate(0)},
          false, false, SHIFT_LSL, 0));
      expanded_instrs.push_back(ParsedInstruction(
          M_SUB,
          {Operand::Register(0), instr.operands.at(0), Operand::Register(0)},
          false, false, SHIFT_LSL, 0));
      expanded_instrs.push_back(ParsedInstruction(
          M_BGE, {Operand::Register(0), Operand::Immediate(2, true)}, false,
          false, SHIFT_LSL, 0));
      expanded_instrs.push_back(ParsedInstruction(
          M_SUB,
          {instr.operands.at(0), Operand::Immediate(0), instr.operands.at(0)},
          false, false, SHIFT_LSL, 0));
      is_pseudo = true;
      break;

    case M_SEQ:
      expanded_instrs.push_back(ParsedInstruction(
          M_SUB,
          {Operand::Register(0), instr.operands.at(1), instr.operands.at(2)},
          false, false, SHIFT_LSL, 0));
      expanded_instrs.push_back(ParsedInstruction(
          M_ADD,
          {instr.operands.at(0), Operand::Register(0), Operand::Immediate(1)},
          false, false, SHIFT_LSL, 0));
      expanded_instrs.push_back(ParsedInstruction(
          M_BNE, {Operand::Register(0), Operand::Immediate(2, true)}, false,
          false, SHIFT_LSL, 0));
      expanded_instrs.push_back(ParsedInstruction(
          M_ADD,
          {instr.operands.at(0), Operand::Register(0), Operand::Immediate(0)},
          false, false, SHIFT_LSL, 0));
      is_pseudo = true;
      break;
    case M_PUSHM:
    case M_POPM:
    case M_SWAP:
      throw std::runtime_error("Macro '" + mnemonic_names[instr.mnemonic] +
                               "' is not yet implemented.");

    default:
      break;  // Not a pseudo-op, handle normally
  }

  if (is_pseudo) {
    for (auto& pi : expanded_instrs) {
      pi.line_number = instr.line_number;
      verify_instruction(pi);
      pi.addr_offset = current_segment->cursor;
      current_segment->instructions.push_back(pi);
      current_segment->cursor += 4;
    }
  } else {
    instr.addr_offset = current_segment->cursor;
    current_segment->instructions.push_back(std::move(instr));
    current_segment->cursor += 4;
  }
}

void Parser::verify_instruction(ParsedInstruction& instr) {
  auto it = std::find_if(SIGNATURES.begin(), SIGNATURES.end(),
                         [&instr](const InstructionSignature& sig) {
                           return sig.mnemonic == instr.mnemonic;
                         });

  if (it == SIGNATURES.end()) {
    throw std::runtime_error(
        "Internal error: no signature for instruction " +
        mnemonic_names[static_cast<size_t>(instr.mnemonic)]);
  }

  if (!allow_placeholders) {
    if (instr.mnemonic == M_DIV || instr.mnemonic == M_DIVU || 
        instr.mnemonic == M_REM || instr.mnemonic == M_REMU) {
      throw std::runtime_error(
          "Instruction '" + mnemonic_names[static_cast<size_t>(instr.mnemonic)] + 
          "' is a placeholder and not implemented in the CPU. Enable --allow-placeholders to compile it.");
    }
  }

  if (instr.freeze_flag &&
      std::any_of(instr.operands.begin(), instr.operands.end(),
                  [](const Operand& op) {
                    return op.type == OperandType::IMMEDIATE;
                  })) {
    throw std::runtime_error(
        "Freeze flag (.F) cannot be set for I-Type (Immediate) instructions at "
        "line " +
        std::to_string(instr.line_number));
  }

  const InstructionSignature& sig = *it;

  for (const auto& valid_pattern : sig.operand_patterns) {
    if (valid_pattern.size() == instr.operands.size()) {
      bool pattern_matches = true;
      for (size_t i = 0; i < valid_pattern.size(); ++i) {
        OperandType expected = valid_pattern[i];
        OperandType actual = instr.operands[i].type;

        bool expected_is_numeric = (expected == OperandType::IMMEDIATE ||
                                    expected == OperandType::LABEL);
        bool actual_is_numeric =
            (actual == OperandType::IMMEDIATE || actual == OperandType::LABEL);

        if (expected_is_numeric && actual_is_numeric) {
          continue;
        }

        if (expected != actual) {
          pattern_matches = false;
          break;
        }
      }

      if (pattern_matches) {
        if (sig.is_offset) {
          for (size_t i = 0; i < instr.operands.size(); ++i) {
            if (instr.operands[i].type == OperandType::IMMEDIATE ||
                instr.operands[i].type == OperandType::LABEL) {
              instr.operands[i].is_offset = true;
            }
          }
        }
        return;
      }
    }
  }

  throw std::runtime_error("Invalid operand types for instruction " +
                           mnemonic_names[static_cast<size_t>(instr.mnemonic)] +
                           " at line " + std::to_string(instr.line_number));
}

}  // namespace sam32