#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>
#include "sam32/assembler/compiler.hpp"
#include "sam32/assembler/lexer.hpp"
#include "sam32/assembler/parser.hpp"

namespace sam32 {
bool is_number(const std::string& str);

std::string read_file(const std::string& filename);
void write_file(const std::string& filename, const std::string& content);
void write_file(const std::string& filename,
                const std::vector<uint8_t>& content);

std::array<uint8_t, 4> to_bytes(uint32_t word, uint8_t size = 4);
uint32_t extract_bits(uint32_t value, size_t start, size_t length);
uint32_t sign_extend(uint32_t value, size_t bits);
uint32_t apply_barrel_shift(uint32_t value, uint8_t shift_type, uint8_t shift_amount);

std::vector<Token> LexString(const std::string& line, int line_number = 0);
std::pair<Segment, Segment> ParseTokens(const std::vector<Token>& tokens);
std::pair<std::vector<uint8_t>, std::vector<LstRow>> ParseAssembly(
    std::pair<Segment, Segment> segments);
std::pair<std::vector<uint8_t>, std::vector<LstRow>> Assemble(
    const std::string& code);
}  // namespace sam32