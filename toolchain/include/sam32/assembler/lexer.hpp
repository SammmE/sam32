#pragma once

#include <string>
#include <vector>

namespace sam32 {
enum TokenType {
  TOKEN_IDENTIFIER,
  TOKEN_DIRECTIVE,
  TOKEN_IMMEDIATE,
  TOKEN_NUMBER,
  TOKEN_STRING,
  TOKEN_COMMA,
  TOKEN_COLON,
  TOKEN_NEWLINE,
  TOKEN_EOF
};

struct Token {
  TokenType type;
  std::string value;
  size_t line;
  size_t column;

  Token(TokenType type, const std::string& value, size_t line, size_t column)
      : type(type), value(value), line(line), column(column) {}
};

struct Lexer {
  std::string input;
  std::vector<Token> tokens;
  size_t position = 0;

  Lexer(const std::string& line) : input(line) {};
  Lexer(const std::vector<std::string>& lines);

  void tokenize();
};
}  // namespace sam32