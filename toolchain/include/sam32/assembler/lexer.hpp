#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace sam32 {
enum TOKEN_TYPE {
  MNEMONIC,
  FS,
  REGISTER,
  IMMEDIATE,
  SHIFT_OP,
  SHIFT_AMT,
  COMMA,
  LABEL_DEF,
  LABEL_REF
};

enum Mnemonic {
  M_ABS, M_ADD, M_AND, M_ASR, M_B, M_BEQ, M_BEQR, M_BEQZ,
  M_BGE, M_BGER, M_BLE, M_BLER, M_BGT, M_BGTR, M_BLT, M_BLTR,
  M_BMI, M_BMIR, M_BR, M_BCC, M_BCCR, M_BCS, M_BCSR, M_BNE,
  M_BNER, M_BNEZ, M_BPL, M_BPLR, M_CALL, M_CLZ, M_CLR, M_CMP,
  M_CMN, M_DEC, M_DIV, M_INC, M_JMP, M_JMPR, M_LI, M_LSL,
  M_LSR, M_LW, M_MOD, M_MOV, M_MUL, M_NEG, M_NOP, M_NOT,
  M_OR, M_POP, M_POPM, M_PUSH, M_PUSHM, M_RET, M_SEQ, M_SGE,
  M_SLT, M_SNE, M_SUB, M_SW, M_SWAP, M_TEQ, M_TST, M_XOR
};

constexpr const char* MNEMONICS[] = {
    "abs",  "add",  "and",  "asr",  "b",     "beq",  "beqr", "beqz",
    "bge",  "bger", "ble",  "bler", "bgt",   "bgtr", "blt",  "bltr",
    "bmi",  "bmir", "br",   "bcc",  "bccr",  "bcs",  "bcsr", "bne",
    "bner", "bnez", "bpl",  "bplr", "call",  "clz",  "clr",  "cmp",
    "cmn",  "dec",  "div",  "inc",  "jmp",     "jmpr",  "li",   "lsl",
    "lsr",  "lw",   "mod",  "mov",  "mul",   "neg",  "nop",  "not",
    "or",   "pop",  "popm", "push", "pushm", "ret",  "seq",  "sge",
    "slt",  "sne",  "sub",  "sw",   "swap",  "teq",  "tst",  "xor"};

struct Token {
  TOKEN_TYPE type;
  std::string value_str;
  int32_t value_int;
  size_t column_number;
};

struct Lexer {
 public:
  std::vector<std::vector<Token>> tokens;

  void add_line(std::string line);
  void lex_file(std::string file_path, bool verbose = false);
};
}  // namespace sam32
