#pragma once

#include <cstdint>
#include <unordered_map>
#include <variant>
#include <vector>

#include "sam32/assembler/parser.hpp"

namespace sam32 {
struct LstRow {
  size_t address;
  uint32_t instruction;
  std::variant<ParsedInstruction, Data> parsed;

  LstRow(size_t addr, uint32_t instr, const ParsedInstruction& parsed_instr)
      : address(addr), instruction(instr), parsed(parsed_instr) {}

  LstRow(size_t addr, uint32_t instr, const Data& data)
      : address(addr), instruction(instr), parsed(data) {}
};

uint32_t encode_instruction(const ParsedInstruction& instr);
std::pair<std::vector<uint8_t>, std::vector<LstRow>> encode(const Segment& text_segment,
                            const Segment& data_segment);

}  // namespace sam32

inline std::unordered_map<sam32::Mnemonic, uint32_t> opcode_map = {

    // Category 00: ALU Operations (Category = 0b00)
    {sam32::M_ADD, 0x00},   // Op: 00000 (0)
    {sam32::M_SUB, 0x04},   // Op: 00001 (1)
    {sam32::M_MUL, 0x08},   // Op: 00010 (2)
    {sam32::M_MULH, 0x48},  // Op: 10010 (18)
    {sam32::M_DIV, 0x0C},   // Op: 00011 (3)
    {sam32::M_DIVU, 0x4C},  // Op: 10011 (19)
    {sam32::M_REM, 0x10},   // Op: 00100 (4)
    {sam32::M_REMU, 0x50},  // Op: 10100 (20)
    {sam32::M_AND, 0x14},   // Op: 00101 (5)
    {sam32::M_OR, 0x18},    // Op: 00110 (6)
    {sam32::M_XOR, 0x1C},   // Op: 00111 (7)
    {sam32::M_NOT, 0x20},   // Op: 01000 (8)
    {sam32::M_CLZ, 0x24},   // Op: 01001 (9)

    // Category 01: Branching & Jumps (PC-Relative, Op[6]=0, Category=0b01)
    {sam32::M_B, 0x01},    // Op: 00000 (0)
    {sam32::M_BEQ, 0x05},  // Op: 00001 (1)
    {sam32::M_BNE, 0x09},  // Op: 00010 (2)
    {sam32::M_BLT, 0x0D},  // Op: 00011 (3)
    {sam32::M_BGE, 0x11},  // Op: 00100 (4)
    {sam32::M_BLE, 0x15},  // Op: 00101 (5)
    {sam32::M_BGT, 0x19},  // Op: 00110 (6)
    {sam32::M_BCS, 0x1D},  // Op: 00111 (7)
    {sam32::M_BCC, 0x21},  // Op: 01000 (8)
    {sam32::M_BMI, 0x25},  // Op: 01001 (9)
    {sam32::M_BPL, 0x29},  // Op: 01010 (10)

    // Category 01: Branching & Jumps (Register-Indirect, Op[6]=1, Category=0b01)
    {sam32::M_BR, 0x41},    // Op: 10000 (16)
    {sam32::M_BEQR, 0x45},  // Op: 10001 (17)
    {sam32::M_BNER, 0x49},  // Op: 10010 (18)
    {sam32::M_BLTR, 0x4D},  // Op: 10011 (19)
    {sam32::M_BGER, 0x51},  // Op: 10100 (20)
    {sam32::M_BLER, 0x55},  // Op: 10101 (21)
    {sam32::M_BGTR, 0x59},  // Op: 10110 (22)
    {sam32::M_BCSR, 0x5D},  // Op: 10111 (23)
    {sam32::M_BCCR, 0x61},  // Op: 11000 (24)
    {sam32::M_BMIR, 0x65},  // Op: 11001 (25)
    {sam32::M_BPLR, 0x69},  // Op: 11010 (26)

    // Category 10: Memory (Load / Store) (Category=0b10)
    // Stores
    {sam32::M_SB, 0x02},  // Op: 00000 (0)
    {sam32::M_SH, 0x06},  // Op: 00001 (1)
    {sam32::M_SW, 0x0A},  // Op: 00010 (2)

    // Loads
    {sam32::M_LB, 0x42},   // Op: 10000 (16)
    {sam32::M_LH, 0x46},   // Op: 10001 (17)
    {sam32::M_LW, 0x4A},   // Op: 10010 (18)
    {sam32::M_LBU, 0x62},  // Op: 11000 (24)
    {sam32::M_LHU, 0x66},  // Op: 11001 (25)
};