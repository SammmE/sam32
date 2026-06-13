#include <algorithm>
#include <iterator>
#include <stdexcept>
#include <string>

#include <sam32/assembler/lexer.hpp>
#include <sam32/assembler/parser.hpp>

namespace sam32 {

Parser::Parser(const std::vector<Token>& tokens) : tokens(tokens), pos(0) {
  text_segment = Segment{{}, 0x00000000, 0};
  data_segment = Segment{{}, 0x10000000, 0};
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
        current_segment->instructions.push_back(
            Data{static_cast<uint32_t>(static_cast<uint8_t>(c)), 1, str.line,
                 current_segment->cursor});
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
      current_segment->cursor += static_cast<size_t>(parse_int(num.value));
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
void Parser::parse_1() {
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

      ParsedInstruction instr;
      instr.mnemonic = mnemonic;
      instr.operands = std::move(operands);
      instr.freeze_flag = freeze_flag;
      instr.has_shift = has_shift;
      instr.shift_type = stype;
      instr.shift_amt = samt;
      instr.addr_offset = current_segment->cursor;
      instr.line_number = instr_line;

      current_segment->instructions.push_back(std::move(instr));
      current_segment->cursor += 4;

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

  // PASS 2 - resolve labels, check operand types
}

}  // namespace sam32