#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "sam32/assembler/parser.hpp"

namespace sam32 {
uint32_t encode_instruction(const ParsedInstruction& instr);
std::vector<uint8_t> encode(const Segment& text_segment,
                            const Segment& data_segment);
}  // namespace sam32

inline std::unordered_map<sam32::Mnemonic, uint32_t> opcode_map = {

    // Category 00: ALU Operations (Category = 0b00 << 30 = 0x00000000)

    {sam32::M_ADD, 0x00000000},   // Op: 00000 (0)
    {sam32::M_SUB, 0x02000000},   // Op: 00001 (1)
    {sam32::M_MUL, 0x04000000},   // Op: 00010 (2)
    {sam32::M_MULH, 0x10000000},  // Op: 01000 (8)  [Inferred]
    {sam32::M_DIV, 0x06000000},   // Op: 00011 (3)
    {sam32::M_DIVU, 0x12000000},  // Op: 01001 (9)  [Inferred]
    {sam32::M_REM, 0x08000000},   // Op: 00100 (4)  (Mapped from MOD)
    {sam32::M_REMU, 0x18000000},  // Op: 01100 (12) [Inferred]
    {sam32::M_AND, 0x0A000000},   // Op: 00101 (5)
    {sam32::M_OR, 0x0C000000},    // Op: 00110 (6)
    {sam32::M_XOR, 0x0E000000},   // Op: 00111 (7)
    {sam32::M_NOT, 0x14000000},   // Op: 01010 (10)
    {sam32::M_CLZ, 0x16000000},   // Op: 01011 (11)

    // Category 01: Branching & Jumps (Category = 0b01 << 30 = 0x40000000)

    // PC-Relative (J-Type)
    {sam32::M_B, 0x40000000},    // Op: 00000 (0)
    {sam32::M_BEQ, 0x42000000},  // Op: 00001 (1)
    {sam32::M_BNE, 0x44000000},  // Op: 00010 (2)
    {sam32::M_BLT, 0x46000000},  // Op: 00011 (3)
    {sam32::M_BGE, 0x48000000},  // Op: 00100 (4)
    {sam32::M_BLE, 0x4A000000},  // Op: 00101 (5)
    {sam32::M_BGT, 0x4C000000},  // Op: 00110 (6)
    {sam32::M_BCS, 0x4E000000},  // Op: 00111 (7)
    {sam32::M_BCC, 0x50000000},  // Op: 01000 (8)
    {sam32::M_BMI, 0x52000000},  // Op: 01001 (9)
    {sam32::M_BPL, 0x54000000},  // Op: 01010 (10)

    // Register-Indirect (R-Branch)
    {sam32::M_BR, 0x60000000},    // Op: 10000 (16)
    {sam32::M_BEQR, 0x62000000},  // Op: 10001 (17)
    {sam32::M_BNER, 0x64000000},  // Op: 10010 (18)
    {sam32::M_BLTR, 0x66000000},  // Op: 10011 (19)
    {sam32::M_BGER, 0x68000000},  // Op: 10100 (20)
    {sam32::M_BLER, 0x6A000000},  // Op: 10101 (21)
    {sam32::M_BGTR, 0x6C000000},  // Op: 10110 (22)
    {sam32::M_BCSR, 0x6E000000},  // Op: 10111 (23)
    {sam32::M_BCCR, 0x70000000},  // Op: 11000 (24)
    {sam32::M_BMIR, 0x72000000},  // Op: 11001 (25)
    {sam32::M_BPLR, 0x74000000},  // Op: 11010 (26)

    // Category 10: Memory (Load / Store) (Category = 0b10 << 30 = 0x80000000)

    // Stores
    {sam32::M_SW, 0x80000000},  // Op: 00000 (0)
    {sam32::M_SH, 0x82000000},  // Op: 00001 (1) [Inferred]
    {sam32::M_SB, 0x84000000},  // Op: 00010 (2) [Inferred]

    // Loads
    {sam32::M_LW, 0xA0000000},   // Op: 10000 (16)
    {sam32::M_LH, 0xA2000000},   // Op: 10001 (17) [Inferred]
    {sam32::M_LHU, 0xA4000000},  // Op: 10010 (18) [Inferred]
    {sam32::M_LB, 0xA6000000},   // Op: 10011 (19) [Inferred]
    {sam32::M_LBU, 0xA8000000},  // Op: 10100 (20) [Inferred]
};