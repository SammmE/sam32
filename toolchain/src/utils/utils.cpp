#include <algorithm>
#include <array>
#include <cstdint>
#include <fstream>
#include <sstream>

#include <sam32/utils/utils.hpp>
#include "sam32/assembler/compiler.hpp"

namespace sam32 {
bool is_number(const std::string& str) {
  if (str.empty())
    return false;

  size_t start = 0;
  if (str[0] == '-') {
    if (str.size() == 1)
      return false;
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

void write_file(const std::string& filename, const std::string& content) {
  std::ofstream file(filename);
  if (!file.is_open()) {
    throw std::runtime_error("Failed to open file: " + filename);
  }

  file << content;
  file.close();
}

void write_file(const std::string& filename,
                const std::vector<uint8_t>& content) {
  std::ofstream file(filename, std::ios::binary);
  if (!file.is_open()) {
    throw std::runtime_error("Failed to open file: " + filename);
  }

  for (uint8_t byte : content) {
    file.put(byte);
  }
  file.close();
}

std::array<uint8_t, 4> to_bytes(uint32_t word, uint8_t size) {
  std::array<uint8_t, 4> bytes = {0, 0, 0, 0};
  for (uint8_t i = 0; i < size; ++i) {
    bytes[i] = (word >> (8 * i)) & 0xFF;
  }
  return bytes;
}

uint32_t extract_bits(uint32_t value, size_t start, size_t length) {
  return (value >> start) & ((1u << length) - 1);
}

uint32_t sign_extend(uint32_t value, size_t bits) {
  uint32_t mask = 1u << (bits - 1);
  return (value ^ mask) - mask;
}

uint32_t apply_barrel_shift(uint32_t value, uint8_t shift_type,
                            uint8_t shift_amount) {
  if (shift_amount == 0)
    return value;  // No shift

  switch (shift_type) {
    case 0:  // Logical left
      return value << shift_amount;
    case 1:  // Logical right
      return value >> shift_amount;
    case 2:  // Arithmetic right
      return static_cast<int32_t>(value) >> shift_amount;
    case 3:  // Rotate right
      if (shift_amount >= 32)
        shift_amount %= 32;

      return (value >> shift_amount) | (value << (32 - shift_amount));
    default:
      throw std::invalid_argument("Invalid shift type");
  }
}

std::vector<Token> LexString(const std::string& line, int line_number) {
  sam32::Lexer lexer(line);
  lexer.tokenize();

  for (Token& token : lexer.tokens) {
    token.line = line_number;
  }
  return lexer.tokens;
}

std::pair<Segment, Segment> ParseTokens(const std::vector<Token>& tokens) {
  sam32::Parser parser(tokens);
  parser.parse();
  return {parser.text_segment, parser.data_segment};
}

std::pair<std::vector<uint8_t>, std::vector<LstRow>> ParseAssembly(
    std::pair<Segment, Segment> segments) {
  return encode(segments.first, segments.second);
}

std::pair<std::vector<uint8_t>, std::vector<LstRow>> Assemble(
    const std::string& code) {
  return ParseAssembly(ParseTokens(LexString(code)));
}
}  // namespace sam32