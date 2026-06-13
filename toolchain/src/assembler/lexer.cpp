#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

#include <sam32/assembler/lexer.hpp>

namespace sam32 {

bool is_number(const std::string& str) {
  if (str.empty()) return false;

  size_t start = 0;
  if (str[0] == '-') {
    if (str.size() == 1) return false;
    start = 1;
  }

  const std::string body = str.substr(start);

  if (std::all_of(body.begin(), body.end(),
                  [](unsigned char c) { return std::isdigit(c); })) {
    return true;
  }
  if (body.size() > 2 && body.substr(0, 2) == "0x" &&
      std::all_of(body.begin() + 2, body.end(),
                  [](unsigned char c) { return std::isxdigit(c); })) {
    return true;
  }
  if (body.size() > 2 && body.substr(0, 2) == "0b" &&
      std::all_of(body.begin() + 2, body.end(),
                  [](unsigned char c) { return c == '0' || c == '1'; })) {
    return true;
  }

  return false;
}

std::string read_file(const std::string& filename) {
  std::ifstream file(filename);
  if (!file.is_open()) {
    throw std::runtime_error("Failed to open file: " + filename);
  }

  std::stringstream buffer;
  buffer << file.rdbuf();
  file.close();
  return buffer.str();
}

Lexer::Lexer(const std::string& filename) : input(read_file(filename)) {}

Lexer::Lexer(const std::vector<std::string>& lines) {
  input = "";
  for (const std::string& line : lines) {
    input += line + "\n";
  }
}

void Lexer::tokenize() {
  size_t column = 0;
  size_t line = 1;

  while (position < input.size()) {
    char ch = input[position];

    switch (ch) {
      case ';':
        while (position < input.size() && input[position] != '\n') {
          position++;
          column++;
        }
        break;

      case ' ':
        column++;
        position++;
        break;
      case '\t':
        column += 4;
        position++;
        break;
      case '\r':
        position++;  // discard CR from CRLF pairs
        break;

      case '\n':
        tokens.emplace_back(TOKEN_NEWLINE, "\n", line, column);
        line++;
        column = 0;
        position++;
        break;

      case ',':
        tokens.emplace_back(TOKEN_COMMA, ",", line, column);
        column++;
        position++;
        break;
      case ':':
        tokens.emplace_back(TOKEN_COLON, ":", line, column);
        column++;
        position++;
        break;

      case '"': {
        position++;
        column++;
        std::string str;
        bool escaped = false;
        while (position < input.size()) {
          char c = input[position];
          if (escaped) {
            switch (c) {
              case 'n':  str += '\n'; break;
              case 't':  str += '\t'; break;
              case 'r':  str += '\r'; break;
              case '0':  str += '\0'; break;
              case '\\': str += '\\'; break;
              case '"':  str += '"';  break;
              default:   str += '\\'; str += c; break;
            }
            escaped = false;
            position++;
            column++;
          } else if (c == '\\') {
            escaped = true;
            position++;
            column++;
          } else if (c == '"') {
            break;
          } else {
            str += c;
            position++;
            column++;
          }
        }
        if (position >= input.size() || input[position] != '"') {
          throw std::runtime_error("Unterminated string literal at line " +
                                   std::to_string(line) + ", column " +
                                   std::to_string(column));
        }
        position++;
        column++;
        tokens.emplace_back(TOKEN_STRING, str, line, column);
      } break;

      case '#': {
        size_t start_col = column;
        position++;
        column++;

        std::string imm;
        if (position < input.size() && input[position] == '-') {
          imm += '-';
          position++;
          column++;
        }
        while (position < input.size() && !std::isspace(input[position]) &&
               input[position] != ',' && input[position] != ':') {
          imm += input[position];
          position++;
          column++;
        }

        if (imm.empty() || imm == "-") {
          throw std::runtime_error(
              "Expected immediate value after '#' at line " +
              std::to_string(line) + ", column " + std::to_string(start_col));
        }
        if (!is_number(imm)) {
          throw std::runtime_error("Invalid immediate value '" + imm +
                                   "' at line " + std::to_string(line) +
                                   ", column " + std::to_string(start_col));
        }

        tokens.emplace_back(TOKEN_IMMEDIATE, imm, line, start_col);
      } break;

      case '.': {
        size_t start_col = column;
        std::string directive;
        while (position < input.size() && !std::isspace(input[position]) &&
               input[position] != ',' && input[position] != ':') {
          directive += input[position];
          position++;
          column++;
        }
        tokens.emplace_back(TOKEN_DIRECTIVE, directive, line, start_col);
      } break;

      case '-': {
        if (position + 1 < input.size() && std::isdigit(input[position + 1])) {
          size_t start_col = column;
          std::string number;
          number += '-';
          position++;
          column++;
          while (position < input.size() &&
                 (std::isdigit(input[position]) ||
                  (number.size() == 2 && number[1] == '0' &&
                   (input[position] == 'x' || input[position] == 'b')) ||
                  (number.size() > 2 &&
                   ((number[2] == 'x' && std::isxdigit(input[position])) ||
                    (number[2] == 'b' &&
                     (input[position] == '0' || input[position] == '1')))))) {
            number += input[position];
            position++;
            column++;
          }
          tokens.emplace_back(TOKEN_NUMBER, number, line, start_col);
        } else {
          throw std::runtime_error(std::string("Unexpected character '-' at line ") +
                                   std::to_string(line) + ", column " +
                                   std::to_string(column));
        }
      } break;

      case '0':
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
      case '8':
      case '9': {
        size_t start_col = column;
        std::string number;
        while (position < input.size() &&
               (std::isdigit(input[position]) ||
                (number.size() == 1 && number[0] == '0' &&
                 (input[position] == 'x' || input[position] == 'b')) ||
                (number.size() > 1 &&
                 ((number[1] == 'x' && std::isxdigit(input[position])) ||
                  (number[1] == 'b' &&
                   (input[position] == '0' || input[position] == '1')))))) {
          number += input[position];
          position++;
          column++;
        }
        if (!is_number(number)) {
          throw std::runtime_error("Invalid number '" + number + "' at line " +
                                   std::to_string(line) + ", column " +
                                   std::to_string(start_col));
        }
        tokens.emplace_back(TOKEN_NUMBER, number, line, start_col);
      } break;

      default: {
        if (std::isalpha(ch) || ch == '_') {
          size_t start_col = column;
          std::string identifier;
          while (position < input.size() &&
                 (std::isalnum(input[position]) || input[position] == '_')) {
            identifier += input[position];
            position++;
            column++;
          }

          // Handle the mnemonic freeze-flag modifier: "ADD.F", "SUB.F", etc.
          if (position < input.size() && input[position] == '.') {
            size_t saved_pos = position;
            size_t saved_col = column;
            std::string suffix;
            suffix += '.';
            size_t tmp = position + 1;
            while (tmp < input.size() && std::isalpha(input[tmp])) {
              suffix += input[tmp];
              tmp++;
            }
            if (!suffix.empty() && suffix.size() > 1 &&
                (tmp >= input.size() || std::isspace(input[tmp]) ||
                 input[tmp] == ',' || input[tmp] == ':' ||
                 input[tmp] == ';')) {
              identifier += suffix;
              position = tmp;
              column = saved_col + suffix.size();
            }
          }

          tokens.emplace_back(TOKEN_IDENTIFIER, identifier, line, start_col);
        } else {
          throw std::runtime_error(std::string("Unexpected character '") + ch +
                                   "' at line " + std::to_string(line) +
                                   ", column " + std::to_string(column));
        }
      } break;
    }
  }

  tokens.emplace_back(TOKEN_EOF, "", line, column);
}

}  // namespace sam32