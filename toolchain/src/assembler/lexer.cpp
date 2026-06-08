#include <algorithm>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

using ::uint8_t;

#include <fmt/base.h>
#include <indicators/block_progress_bar.hpp>
#include <indicators/cursor_control.hpp>

#include <sam32/assembler/lexer.hpp>

namespace sam32 {
namespace {
int32_t parse_integer_literal(const std::string& text) {
  if (text.empty()) {
    throw std::invalid_argument("empty integer literal");
  }

  int64_t value = 0;
  size_t parsed = 0;

  if (text.rfind("0b", 0) == 0 || text.rfind("-0b", 0) == 0) {
    const bool negative = text[0] == '-';
    const size_t prefix_len = negative ? 3 : 2;
    if (text.size() == prefix_len) {
      throw std::invalid_argument("binary literal has no digits");
    }
    for (size_t i = prefix_len; i < text.size(); i++) {
      if (text[i] != '0' && text[i] != '1') {
        throw std::invalid_argument("invalid binary digit");
      }
      value = (value << 1) + (text[i] - '0');
      if (value > static_cast<int64_t>(INT32_MAX) + 1) {
        throw std::out_of_range("binary literal out of range");
      }
    }
    if (negative) {
      value = -value;
    }
  } else {
    value = std::stoll(text, &parsed, 0);
    if (parsed != text.size()) {
      throw std::invalid_argument("trailing characters in integer literal");
    }
  }

  if (value < INT32_MIN || value > 0xFFFFFFFFLL) {
    throw std::out_of_range("integer literal out of range");
  }
  return static_cast<int32_t>(value);
}
}  // namespace

void Lexer::add_line(std::string line) {
  std::vector<Token> line_tokens;
  size_t line_number = tokens.size() + 1;

  size_t cursor = 0;  // column
  while (cursor < line.size()) {
    // if current char is a ';', then it's a comment
    if (line[cursor] == ';') {
      break;
    } else if (line[cursor] == ' ' || line[cursor] == '\t') {
      cursor++;
      continue;
    } else if (line[cursor] == ',') {
      line_tokens.push_back({TOKEN_TYPE::COMMA, ",", 0, cursor});
      cursor++;
      continue;
    }

    // must be a letter, #, or a digit
    // parse until we hit a space, tab, comma, or ';'
    size_t end = cursor;
    bool in_string = false;
    while (end < line.size()) {
      char c = line[end];
      if (c == '"') {
        in_string = !in_string;
      } else if (!in_string &&
                 (c == ' ' || c == '\t' || c == ',' || c == ';')) {
        break;
      }
      end++;
    }

    std::string chunk = line.substr(cursor, end - cursor);

    if (chunk[0] == '.') {
      // if the chunk starts with a '.', it's a directive
      std::string directive_str = chunk.substr(1);
      size_t num_directives = std::size(DIRECTIVES);
      bool found = false;

      for (size_t i = 0; i < num_directives; i++) {
        if (directive_str == DIRECTIVES[i]) {
          line_tokens.push_back({TOKEN_TYPE::DIRECTIVE, directive_str,
                                 static_cast<int32_t>(i), cursor});
          found = true;
          break;
        }
      }

      if (!found) {
        throw std::runtime_error("Invalid directive: " + directive_str +
                                 " at line " + std::to_string(line_number) +
                                 ", column " + std::to_string(cursor));
      }
    } else if (std::isdigit(chunk[0]) || (chunk[0] == '-' && chunk.size() > 1 &&
                                          std::isdigit(chunk[1]))) {
      // if the chunk starts with a digit, it's a number (not immediate! those start w/ #) (e.g. 0x..., 0b..., or decimal)
      int32_t numeric_value;
      try {
        numeric_value = parse_integer_literal(chunk);
      } catch (const std::invalid_argument& e) {
        throw std::runtime_error("Invalid immediate value: " + chunk +
                                 " at line " + std::to_string(line_number) +
                                 ", column " + std::to_string(cursor));
      } catch (const std::out_of_range& e) {
        throw std::runtime_error("Immediate value out of range: " + chunk +
                                 " at line " + std::to_string(line_number) +
                                 ", column " + std::to_string(cursor));
      }
      line_tokens.push_back(
          {TOKEN_TYPE::IMMEDIATE, chunk, numeric_value, cursor});

    } else if (chunk[0] == '"') {
      // if the chunk starts with a '"', it's a string literal. Parse until the closing '"', allowing for escaped quotes (\") and escaped backslashes (\\)
      std::string str_value;
      bool escape = false;
      size_t j = 1;
      for (; j < chunk.size(); j++) {
        char c = chunk[j];
        if (escape) {
          if (c == '"' || c == '\\') {
            str_value += c;
          } else {
            throw std::runtime_error(
                "Invalid escape sequence in string literal: \\" +
                std::string(1, c) + " at line " + std::to_string(line_number) +
                ", column " + std::to_string(cursor + j));
          }
          escape = false;
        } else {
          if (c == '\\') {
            escape = true;
          } else if (c == '"') {
            break;
          } else {
            str_value += c;
          }
        }
      }
      if (j == chunk.size() || chunk[j] != '"') {
        throw std::runtime_error("Unterminated string literal at line " +
                                 std::to_string(line_number) + ", column " +
                                 std::to_string(cursor));
      }
      line_tokens.push_back({TOKEN_TYPE::STRING, str_value, 0, cursor});
    } else if (chunk[0] == '\'') {
      // if the chunk starts with a ''', it's a character literal. It must be exactly 3 characters long (e.g. 'a', '\n', '\\', '\'')
      if (chunk.size() == 3 && chunk[2] == '\'') {
        line_tokens.push_back(
            {TOKEN_TYPE::CHAR, std::string(1, chunk[1]), 0, cursor});
      } else if (chunk.size() == 4 && chunk[1] == '\\' && chunk[3] == '\'') {
        char escaped_char;
        switch (chunk[2]) {
          case 'n':
            escaped_char = '\n';
            break;
          case 't':
            escaped_char = '\t';
            break;
          case '\\':
            escaped_char = '\\';
            break;
          case '\'':
            escaped_char = '\'';
            break;
          default:
            throw std::runtime_error(
                "Invalid escape sequence in character literal: \\" +
                std::string(1, chunk[2]) + " at line " +
                std::to_string(line_number) + ", column " +
                std::to_string(cursor));
        }
        line_tokens.push_back(
            {TOKEN_TYPE::CHAR, std::string(1, escaped_char), 0, cursor});
      } else {
        throw std::runtime_error("Invalid character literal at line " +
                                 std::to_string(line_number) + ", column " +
                                 std::to_string(cursor));
      }
    } else if (chunk[0] == '#') {
      // if the chunk starts with a '#', it's an immediate value
      // strip the '#' and parse the rest as an int. If the remaining string contains non-digit characters, it's an error (unless it's a hex value starting with 0x)
      // if the immediate is >8192, it's an error
      std::string immediate_str = chunk.substr(1);
      int32_t imm_val;
      try {
        imm_val = parse_integer_literal(immediate_str);
      } catch (const std::invalid_argument& e) {
        throw std::runtime_error("Invalid immediate value: " + immediate_str +
                                 " at line " + std::to_string(line_number) +
                                 ", column " + std::to_string(cursor));
      } catch (const std::out_of_range& e) {
        throw std::runtime_error(
            "Immediate value out of range, use a register instead: " +
            immediate_str + " at line " + std::to_string(line_number) +
            ", column " + std::to_string(cursor));
      }

      if (imm_val > 131071) {
        throw std::runtime_error(
            "Immediate/Offset value too large, use a register shift instead: " +
            std::to_string(imm_val) + " at line " +
            std::to_string(line_number) + ", column " + std::to_string(cursor));
      } else if (imm_val < -131072) {
        throw std::runtime_error(
            "Immediate value too small, use a register shift instead: " +
            std::to_string(imm_val) + " at line " +
            std::to_string(line_number) + ", column " + std::to_string(cursor));
      }
      // the token type is SHIFT_AMT if the previous token is a shift operator, otherwise it's an IMMEDIATE
      if (!line_tokens.empty() &&
          line_tokens.back().type == TOKEN_TYPE::SHIFT_OP) {
        if (imm_val > 31) {
          throw std::runtime_error(
              "Shift amount too large, must be between 0 and 31: " +
              std::to_string(imm_val) + " at line " +
              std::to_string(line_number) + ", column " +
              std::to_string(cursor));
        } else if (imm_val < 0) {
          throw std::runtime_error(
              "Shift amount cannot be negative, must be between 0 and 31: " +
              std::to_string(imm_val) + " at line " +
              std::to_string(line_number) + ", column " +
              std::to_string(cursor));
        }

        line_tokens.push_back({TOKEN_TYPE::SHIFT_AMT, chunk, imm_val, cursor});
      } else {
        line_tokens.push_back({TOKEN_TYPE::IMMEDIATE, chunk, imm_val, cursor});
      }
    }

    // if the register starts with 'R' or 'r' AND the rest are digits, it's a register
    else if ((chunk[0] == 'R' || chunk[0] == 'r') && chunk.size() > 1 &&
             std::all_of(chunk.begin() + 1, chunk.end(),
                         [](unsigned char c) { return std::isdigit(c); })) {
      std::string reg_num_str = chunk.substr(1);
      int32_t reg_num_signed;
      try {
        size_t parsed = 0;
        reg_num_signed = std::stoi(reg_num_str, &parsed, 10);
        if (parsed != reg_num_str.size()) {
          throw std::invalid_argument("trailing characters in register");
        }
      } catch (const std::invalid_argument& e) {
        throw std::runtime_error("Invalid register number: " + reg_num_str +
                                 " at line " + std::to_string(line_number) +
                                 ", column " + std::to_string(cursor));
      } catch (const std::out_of_range& e) {
        throw std::runtime_error(
            "Register number out of range: " + reg_num_str + " at line " +
            std::to_string(line_number) + ", column " + std::to_string(cursor));
      }

      if (reg_num_signed > 31) {
        throw std::runtime_error("Register number too large: R" +
                                 std::to_string(reg_num_signed) + " at line " +
                                 std::to_string(line_number) + ", column " +
                                 std::to_string(cursor));
      } else if (reg_num_signed < 0) {
        throw std::runtime_error("Register number cannot be negative: R" +
                                 std::to_string(reg_num_signed) + " at line " +
                                 std::to_string(line_number) + ", column " +
                                 std::to_string(cursor));
      }
      uint8_t reg_num = static_cast<uint8_t>(reg_num_signed);
      line_tokens.push_back({TOKEN_TYPE::REGISTER, chunk, reg_num, cursor});
    }
    // If the Chunk is exactly "LSL, LSR or ASR, it's a shift operator
    else if (chunk == "LSL" || chunk == "LSR" || chunk == "ASR" ||
             chunk == "lsl" || chunk == "lsr" || chunk == "asr") {
      line_tokens.push_back({TOKEN_TYPE::SHIFT_OP, chunk, 0, cursor});
    }
    // if the chunk ends in ':' it's a label definition
    else if (chunk.back() == ':') {
      std::string label_name = chunk.substr(0, chunk.size() - 1);
      line_tokens.push_back({TOKEN_TYPE::LABEL_DEF, label_name, 0, cursor});
      if (label_name.empty()) {
        throw std::runtime_error("Label name cannot be empty at line " +
                                 std::to_string(line_number) + ", column " +
                                 std::to_string(cursor));
      } else if (!std::isalpha(static_cast<unsigned char>(label_name[0])) &&
                 label_name[0] != '_' && label_name[0] != '.') {
        throw std::runtime_error(
            "Label name must start with a letter, underscore, or period at "
            "line " +
            std::to_string(line_number) + ", column " + std::to_string(cursor));
      } else if (std::any_of(label_name.begin(), label_name.end(), [](char c) {
                   return !std::isalnum(static_cast<unsigned char>(c)) &&
                          c != '_' && c != '.';
                 })) {
        throw std::runtime_error(
            "Label name can only contain letters, digits, underscores, and "
            "periods at "
            "line " +
            std::to_string(line_number) + ", column " + std::to_string(cursor));
      }
    }
    // it must now be a mnemonic or a label reference. If the chunk matches one of the known mnemonics, it's a mnemonic. Otherwise, it's a label reference (which we will resolve later). If the chunk has a '.', it's a flag and a guaranteed mnemonic, so split on the '.'. Otherwise, check if the chunk is in the list of mnemonics. If it is, it's a mnemonic. If not, it's a label reference.
    else {
      bool has_flag = false;
      std::string mnem_str = chunk;
      std::string flag_str = "";

      if (chunk.size() >= 2 && (chunk.substr(chunk.size() - 2) == ".F" ||
                                chunk.substr(chunk.size() - 2) == ".f")) {
        has_flag = true;
        mnem_str = chunk.substr(0, chunk.size() - 2);
        flag_str = chunk.substr(chunk.size() - 1);
      }

      bool is_mnemonic = false;
      int32_t mnemonic_val = -1;
      std::string chunk_lc = mnem_str;
      std::transform(chunk_lc.begin(), chunk_lc.end(), chunk_lc.begin(),
                     ::tolower);

      size_t num_mnemonics = std::size(MNEMONICS);
      for (size_t i = 0; i < num_mnemonics; i++) {
        if (chunk_lc == MNEMONICS[i]) {
          is_mnemonic = true;
          mnemonic_val = static_cast<int32_t>(i);
          break;
        }
      }

      if (has_flag) {
        if (!is_mnemonic) {
          throw std::runtime_error("Invalid mnemonic with '.F' flag at line " +
                                   std::to_string(line_number) + ", column " +
                                   std::to_string(cursor));
        }
        line_tokens.push_back(
            {TOKEN_TYPE::MNEMONIC, mnem_str, mnemonic_val, cursor});
        line_tokens.push_back(
            {TOKEN_TYPE::FS, flag_str, 0, cursor});  // only one flag is allowed
      } else {
        if (is_mnemonic) {
          line_tokens.push_back(
              {TOKEN_TYPE::MNEMONIC, chunk, mnemonic_val, cursor});
        } else {
          // label reference
          line_tokens.push_back({TOKEN_TYPE::LABEL_REF, chunk, 0, cursor});
        }
      }
    }
    cursor = end;
  }
  tokens.push_back(line_tokens);
};

void Lexer::lex_file(std::string file_path, bool verbose) {
  std::ifstream asm_file(file_path, std::ios::binary | std::ios::ate);

  if (!asm_file.is_open()) {
    throw std::runtime_error("Failed to open file " + file_path);
  }

  std::streamsize total_bytes = asm_file.tellg();
  asm_file.seekg(0, std::ios::beg);

  indicators::BlockProgressBar bar{
      indicators::option::BarWidth{80},
      indicators::option::Start{"["},
      indicators::option::End{"]"},
      indicators::option::ForegroundColor{indicators::Color::white},
      indicators::option::ShowPercentage{true},
      indicators::option::FontStyles{
          std::vector<indicators::FontStyle>{indicators::FontStyle::bold}}};

  if (verbose) {
    fmt::println("Lexing file: {}", file_path);
  }

  std::string line;
  while (std::getline(asm_file, line)) {
    add_line(line);

    if (verbose) {
      std::streamsize current_bytes = asm_file.tellg();
      if (current_bytes == -1)
        current_bytes = total_bytes;
      bar.set_progress(static_cast<size_t>(
          (static_cast<double>(current_bytes) / total_bytes) * 100.0));
    }
  }

  if (verbose) {
    bar.mark_as_completed();
    indicators::show_console_cursor(true);
  }
};
}  // namespace sam32
