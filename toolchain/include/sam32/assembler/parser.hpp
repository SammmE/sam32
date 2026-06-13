#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>
#include "sam32/assembler/lexer.hpp"

namespace sam32 {

enum class OperandType { REGISTER, IMMEDIATE, LABEL };

/// Output barrel-shifter / rotator type (bits 25:24 of R-Type)
enum ShiftType : uint8_t {
  SHIFT_LSL = 0b00,
  SHIFT_LSR = 0b01,
  SHIFT_ASR = 0b10,
  SHIFT_ROR = 0b11,
};

struct Operand {
  OperandType type;
  int reg_id;
  uint32_t value;
  std::string label;

  static Operand Register(int reg_id) {
    return Operand{OperandType::REGISTER, reg_id, 0, ""};
  }

  // Accept signed int32_t so that negative immediates (e.g. #-4) are stored
  // correctly in the lower 32 bits via two's complement bit pattern.
  static Operand Immediate(int32_t value) {
    return Operand{OperandType::IMMEDIATE, 0, static_cast<uint32_t>(value), ""};
  }

  static Operand Label(const std::string& label) {
    return Operand{OperandType::LABEL, 0, 0, label};
  }
};

enum Mnemonic {
  // Category 00: ALU Operations
  M_ADD,
  M_SUB,
  M_MUL,
  M_MULH,
  M_DIV,
  M_DIVU,
  M_REM,
  M_REMU,
  M_AND,
  M_OR,
  M_XOR,
  M_NOT,
  M_CLZ,

  // Category 01: Branching & Jumps (PC-Relative)
  M_B,
  M_BEQ,
  M_BNE,
  M_BLT,
  M_BGE,
  M_BLE,
  M_BGT,
  M_BCS,
  M_BCC,
  M_BMI,
  M_BPL,

  // Category 01: Branching & Jumps (Register-Indirect)
  M_BR,
  M_BEQR,
  M_BNER,
  M_BLTR,
  M_BGER,
  M_BLER,
  M_BGTR,
  M_BCSR,
  M_BCCR,
  M_BMIR,
  M_BPLR,

  // Category 10: Memory (Load / Store)
  M_SB,
  M_SH,
  M_SW,
  M_LB,
  M_LBU,
  M_LH,
  M_LHU,
  M_LW,

  // pseudo-instructions
  M_NOP,
  M_MOV,
  M_NEG,
  M_LI,
  M_PUSH,
  M_POP,
  M_LSL,
  M_LSR,
  M_ASR,
  M_ROR,
  M_INC,
  M_DEC,
  M_CMP,
  M_TST,
  M_JMP,
  M_CALL,
  M_RET
};

const std::string mnemonic_names[] = {
    // Category 00: ALU Operations
    "add", "sub", "mul", "mulh", "div", "divu", "rem", "remu", "and", "or",
    "xor", "not", "clz",

    // Category 01: Branching & Jumps (PC-Relative)
    "b", "beq", "bne", "blt", "bge", "ble", "bgt", "bcs", "bcc", "bmi", "bpl",

    // Category 01: Branching & Jumps (Register-Indirect)
    "br", "beqr", "bner", "bltr", "bger", "bler", "bgtr", "bcsr", "bccr",
    "bmir", "bplr",

    // Category 10: Memory (Load / Store)
    "sb", "sh", "sw", "lb", "lbu", "lh", "lhu", "lw",

    // pseudo-instructions
    "nop", "mov", "neg", "li", "push", "pop", "lsl", "lsr", "asr", "ror", "inc",
    "dec", "cmp", "tst", "jmp", "call", "ret"};

enum Directive {
  D_DATA,
  D_TEXT,
  D_ORG,
  D_EQU,
  D_BYTE,
  D_HALF,
  D_WORD,
  D_STRING,
  D_SPACE,
  D_INCLUDE
};

const std::string directive_names[] = {".data",  ".text",   ".org",  ".equ",
                                       ".byte",  ".half",   ".word", ".string",
                                       ".space", ".include"};

struct InstructionSignature {
  Mnemonic mnemonic;
  std::vector<std::vector<OperandType>> operand_patterns;
  bool is_offset;      // is the immediate an offset?
  bool shift_allowed;  // only allowed for R-type instructions
  bool flag_allowed;   // only allowed for R-type instructions

  constexpr InstructionSignature(Mnemonic m,
                                 std::vector<std::vector<OperandType>> patterns,
                                 bool offset = false, bool shift = false,
                                 bool flag = false)
      : mnemonic(m),
        operand_patterns(std::move(patterns)),
        is_offset(offset),
        shift_allowed(shift),
        flag_allowed(flag) {}
};

struct ParsedInstruction {
  Mnemonic mnemonic;
  std::vector<Operand> operands;

  // Freeze-flag modifier (.F suffix): if true, ALU output is written but
  // the Z/N/C/V status flags are NOT updated (bit 31 of R-Type = 1).
  bool freeze_flag = false;

  // Output barrel-shifter applied to the ALU result before Rd write.
  bool has_shift = false;
  ShiftType shift_type = SHIFT_LSL;
  uint8_t shift_amt = 0;

  size_t addr_offset = 0;  ///< byte offset from segment base at parse time
  size_t line_number = 0;
};

struct Data {
  uint32_t value;  ///< raw numeric payload
  uint8_t size;    ///< byte width: 1 (.byte), 2 (.half), 4 (.word)
  size_t line_number;
  size_t addr_offset;
};

struct Segment {
  std::vector<std::variant<ParsedInstruction, Data>> instructions;
  size_t addr_base;
  size_t cursor;
};

enum INSTRUCTION_CATEGORY { CAT_ALU, CAT_BRANCH, CAT_MEM, CAT_PSEUDO };

struct Parser {
  std::vector<Token> tokens;  ///< may grow if .include is encountered
  size_t pos;
  Segment text_segment;
  Segment data_segment;
  Segment* current_segment;

  /// symbol_table maps label/constant names to:
  ///   { pointer-to-segment-base (nullptr = absolute constant),
  ///     byte offset from that base }
  std::unordered_map<std::string, std::pair<size_t*, size_t>> symbol_table;

  Parser(const std::vector<Token>& tokens);

  /// First pass: tokenize all statements, build segment instruction lists,
  /// and populate symbol_table with label definitions seen so far.
  /// Forward references are stored as Operand::Label for the second pass.
  void parse_1();
  void parse_2();

 private:
  Token current() const;
  Token peek() const;
  Token consume();
  void skip_newlines();

  void parse_directive(Directive directive, const Token& dir_tok);

  std::vector<Operand> parse_operands(bool& has_shift, ShiftType& shift_type,
                                      uint8_t& shift_amt);

  std::vector<ParsedInstruction> result_pseudo_instructions(
      const ParsedInstruction& instr);

  void check_operand_types(const ParsedInstruction& instr);
};

inline const std::vector<InstructionSignature> SIGNATURES = {
    // Category 00: ALU Operations
    InstructionSignature{
        M_ADD,
        {{OperandType::REGISTER, OperandType::REGISTER, OperandType::REGISTER},
         {OperandType::REGISTER, OperandType::REGISTER, OperandType::IMMEDIATE},
         {OperandType::REGISTER, OperandType::IMMEDIATE,
          OperandType::REGISTER}},
        false,
        true,
        true},
    InstructionSignature{
        M_SUB,
        {{OperandType::REGISTER, OperandType::REGISTER, OperandType::REGISTER},
         {OperandType::REGISTER, OperandType::REGISTER, OperandType::IMMEDIATE},
         {OperandType::REGISTER, OperandType::IMMEDIATE,
          OperandType::REGISTER}},
        false,
        true,
        true},
    InstructionSignature{
        M_MUL,
        {{OperandType::REGISTER, OperandType::REGISTER, OperandType::REGISTER},
         {OperandType::REGISTER, OperandType::REGISTER, OperandType::IMMEDIATE},
         {OperandType::REGISTER, OperandType::IMMEDIATE,
          OperandType::REGISTER}},
        false,
        true,
        true},
    InstructionSignature{
        M_MULH,
        {{OperandType::REGISTER, OperandType::REGISTER, OperandType::REGISTER},
         {OperandType::REGISTER, OperandType::REGISTER, OperandType::IMMEDIATE},
         {OperandType::REGISTER, OperandType::IMMEDIATE,
          OperandType::REGISTER}},
        false,
        true,
        true},
    InstructionSignature{
        M_DIV,
        {{OperandType::REGISTER, OperandType::REGISTER, OperandType::REGISTER},
         {OperandType::REGISTER, OperandType::REGISTER, OperandType::IMMEDIATE},
         {OperandType::REGISTER, OperandType::IMMEDIATE,
          OperandType::REGISTER}},
        false,
        true,
        true},
    InstructionSignature{
        M_DIVU,
        {{OperandType::REGISTER, OperandType::REGISTER, OperandType::REGISTER},
         {OperandType::REGISTER, OperandType::REGISTER, OperandType::IMMEDIATE},
         {OperandType::REGISTER, OperandType::IMMEDIATE,
          OperandType::REGISTER}},
        false,
        true,
        true},
    InstructionSignature{
        M_REM,
        {{OperandType::REGISTER, OperandType::REGISTER, OperandType::REGISTER},
         {OperandType::REGISTER, OperandType::REGISTER, OperandType::IMMEDIATE},
         {OperandType::REGISTER, OperandType::IMMEDIATE,
          OperandType::REGISTER}},
        false,
        true,
        true},
    InstructionSignature{
        M_REMU,
        {{OperandType::REGISTER, OperandType::REGISTER, OperandType::REGISTER},
         {OperandType::REGISTER, OperandType::REGISTER, OperandType::IMMEDIATE},
         {OperandType::REGISTER, OperandType::IMMEDIATE,
          OperandType::REGISTER}},
        false,
        true,
        true},
    InstructionSignature{
        M_AND,
        {{OperandType::REGISTER, OperandType::REGISTER, OperandType::REGISTER},
         {OperandType::REGISTER, OperandType::REGISTER, OperandType::IMMEDIATE},
         {OperandType::REGISTER, OperandType::IMMEDIATE,
          OperandType::REGISTER}},
        false,
        true,
        true},
    InstructionSignature{
        M_OR,
        {{OperandType::REGISTER, OperandType::REGISTER, OperandType::REGISTER},
         {OperandType::REGISTER, OperandType::REGISTER, OperandType::IMMEDIATE},
         {OperandType::REGISTER, OperandType::IMMEDIATE,
          OperandType::REGISTER}},
        false,
        true,
        true},
    InstructionSignature{
        M_XOR,
        {{OperandType::REGISTER, OperandType::REGISTER, OperandType::REGISTER},
         {OperandType::REGISTER, OperandType::REGISTER, OperandType::IMMEDIATE},
         {OperandType::REGISTER, OperandType::IMMEDIATE,
          OperandType::REGISTER}},
        false,
        true,
        true},
    InstructionSignature{M_NOT,
                         {{OperandType::REGISTER, OperandType::REGISTER}},
                         false,
                         true,
                         true},
    InstructionSignature{M_CLZ,
                         {{OperandType::REGISTER, OperandType::REGISTER}},
                         false,
                         true,
                         true},

    // Category 01: Branching & Jumps (PC-Relative)
    InstructionSignature{M_B,
                         {{OperandType::REGISTER, OperandType::LABEL},
                          {OperandType::REGISTER, OperandType::IMMEDIATE}},
                         true,
                         false,
                         false},
    InstructionSignature{M_BEQ,
                         {{OperandType::REGISTER, OperandType::LABEL},
                          {OperandType::REGISTER, OperandType::IMMEDIATE}},
                         true,
                         false,
                         false},
    InstructionSignature{M_BNE,
                         {{OperandType::REGISTER, OperandType::LABEL},
                          {OperandType::REGISTER, OperandType::IMMEDIATE}},
                         true,
                         false,
                         false},
    InstructionSignature{M_BLT,
                         {{OperandType::REGISTER, OperandType::LABEL},
                          {OperandType::REGISTER, OperandType::IMMEDIATE}},
                         true,
                         false,
                         false},
    InstructionSignature{M_BGE,
                         {{OperandType::REGISTER, OperandType::LABEL},
                          {OperandType::REGISTER, OperandType::IMMEDIATE}},
                         true,
                         false,
                         false},
    InstructionSignature{M_BLE,
                         {{OperandType::REGISTER, OperandType::LABEL},
                          {OperandType::REGISTER, OperandType::IMMEDIATE}},
                         true,
                         false,
                         false},
    InstructionSignature{M_BGT,
                         {{OperandType::REGISTER, OperandType::LABEL},
                          {OperandType::REGISTER, OperandType::IMMEDIATE}},
                         true,
                         false,
                         false},
    InstructionSignature{M_BCS,
                         {{OperandType::REGISTER, OperandType::LABEL},
                          {OperandType::REGISTER, OperandType::IMMEDIATE}},
                         true,
                         false,
                         false},
    InstructionSignature{M_BCC,
                         {{OperandType::REGISTER, OperandType::LABEL},
                          {OperandType::REGISTER, OperandType::IMMEDIATE}},
                         true,
                         false,
                         false},
    InstructionSignature{M_BMI,
                         {{OperandType::REGISTER, OperandType::LABEL},
                          {OperandType::REGISTER, OperandType::IMMEDIATE}},
                         true,
                         false,
                         false},
    InstructionSignature{M_BPL,
                         {{OperandType::REGISTER, OperandType::LABEL},
                          {OperandType::REGISTER, OperandType::IMMEDIATE}},
                         true,
                         false,
                         false},

    // Category 01: Branching & Jumps (Register-Indirect)
    InstructionSignature{M_BR, {{OperandType::REGISTER}}, false, false, false},
    InstructionSignature{M_BEQR,
                         {{OperandType::REGISTER}},
                         false,
                         false,
                         false},
    InstructionSignature{M_BNER,
                         {{OperandType::REGISTER}},
                         false,
                         false,
                         false},
    InstructionSignature{M_BLTR,
                         {{OperandType::REGISTER}},
                         false,
                         false,
                         false},
    InstructionSignature{M_BGER,
                         {{OperandType::REGISTER}},
                         false,
                         false,
                         false},
    InstructionSignature{M_BLER,
                         {{OperandType::REGISTER}},
                         false,
                         false,
                         false},
    InstructionSignature{M_BGTR,
                         {{OperandType::REGISTER}},
                         false,
                         false,
                         false},
    InstructionSignature{M_BCSR,
                         {{OperandType::REGISTER}},
                         false,
                         false,
                         false},
    InstructionSignature{M_BCCR,
                         {{OperandType::REGISTER}},
                         false,
                         false,
                         false},
    InstructionSignature{M_BMIR,
                         {{OperandType::REGISTER}},
                         false,
                         false,
                         false},
    InstructionSignature{M_BPLR,
                         {{OperandType::REGISTER}},
                         false,
                         false,
                         false},

    // Category 10: Memory (Load / Store)
    InstructionSignature{M_SB,
                         {{OperandType::REGISTER, OperandType::REGISTER,
                           OperandType::IMMEDIATE}},
                         true,
                         false,
                         false},
    InstructionSignature{M_SH,
                         {{OperandType::REGISTER, OperandType::REGISTER,
                           OperandType::IMMEDIATE}},
                         true,
                         false,
                         false},
    InstructionSignature{M_SW,
                         {{OperandType::REGISTER, OperandType::REGISTER,
                           OperandType::IMMEDIATE}},
                         true,
                         false,
                         false},
    InstructionSignature{M_LB,
                         {{OperandType::REGISTER, OperandType::REGISTER,
                           OperandType::IMMEDIATE}},
                         true,
                         false,
                         false},
    InstructionSignature{M_LBU,
                         {{OperandType::REGISTER, OperandType::REGISTER,
                           OperandType::IMMEDIATE}},
                         true,
                         false,
                         false},
    InstructionSignature{M_LH,
                         {{OperandType::REGISTER, OperandType::REGISTER,
                           OperandType::IMMEDIATE}},
                         true,
                         false,
                         false},
    InstructionSignature{M_LHU,
                         {{OperandType::REGISTER, OperandType::REGISTER,
                           OperandType::IMMEDIATE}},
                         true,
                         false,
                         false},
    InstructionSignature{M_LW,
                         {{OperandType::REGISTER, OperandType::REGISTER,
                           OperandType::IMMEDIATE}},
                         true,
                         false,
                         false},

    // pseudo-instructions
    InstructionSignature{M_NOP, {{}}, false, false, false},
    InstructionSignature{M_MOV,
                         {{OperandType::REGISTER, OperandType::REGISTER}},
                         false,
                         false,
                         false},
    InstructionSignature{M_NEG,
                         {{OperandType::REGISTER, OperandType::REGISTER}},
                         false,
                         false,
                         false},
    InstructionSignature{M_LI,
                         {{OperandType::REGISTER, OperandType::IMMEDIATE},
                          {OperandType::REGISTER, OperandType::LABEL}},
                         false,
                         false,
                         false},
    InstructionSignature{M_PUSH,
                         {{OperandType::REGISTER}},
                         false,
                         false,
                         false},
    InstructionSignature{M_POP, {{OperandType::REGISTER}}, false, false, false},
    InstructionSignature{M_LSL,
                         {{OperandType::REGISTER, OperandType::REGISTER,
                           OperandType::IMMEDIATE}},
                         false,
                         false,
                         false},
    InstructionSignature{M_LSR,
                         {{OperandType::REGISTER, OperandType::REGISTER,
                           OperandType::IMMEDIATE}},
                         false,
                         false,
                         false},
    InstructionSignature{M_ASR,
                         {{OperandType::REGISTER, OperandType::REGISTER,
                           OperandType::IMMEDIATE}},
                         false,
                         false,
                         false},
    InstructionSignature{M_ROR,
                         {{OperandType::REGISTER, OperandType::REGISTER,
                           OperandType::IMMEDIATE}},
                         false,
                         false,
                         false},
    InstructionSignature{M_INC, {{OperandType::REGISTER}}, false, false, false},
    InstructionSignature{M_DEC, {{OperandType::REGISTER}}, false, false, false},
    InstructionSignature{M_CMP,
                         {{OperandType::REGISTER, OperandType::REGISTER},
                          {OperandType::REGISTER, OperandType::IMMEDIATE}},
                         false,
                         false,
                         false},
    InstructionSignature{M_TST,
                         {{OperandType::REGISTER, OperandType::REGISTER},
                          {OperandType::REGISTER, OperandType::IMMEDIATE}},
                         false,
                         false,
                         false},
    InstructionSignature{M_JMP, {{OperandType::LABEL}}, true, false, false},
    InstructionSignature{M_CALL, {{OperandType::LABEL}}, true, false, false},
    InstructionSignature{M_RET, {{}}, false, false, false}};

}  // namespace sam32