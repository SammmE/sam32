#pragma once
#include <string>
#include <unordered_map>
#include <vector>

#include <sam32asm/compiler.hpp>
#include <sam32asm/lexer.hpp>

namespace sam32asm {
struct Parser {
 public:
  std::vector<std::vector<Token>> tokens;
  std::vector<Instruction> instructions;
  std::unordered_map<std::string, size_t> label_addresses;

  inline static const std::vector<TOKEN_TYPE> R_MODEL = {
      TOKEN_TYPE::MNEMONIC, TOKEN_TYPE::COMMA,    TOKEN_TYPE::REGISTER,
      TOKEN_TYPE::COMMA,    TOKEN_TYPE::REGISTER, TOKEN_TYPE::COMMA,
      TOKEN_TYPE::REGISTER};

  inline static const std::vector<TOKEN_TYPE> R_SHIFT_MODEL = {
      TOKEN_TYPE::MNEMONIC, TOKEN_TYPE::COMMA,    TOKEN_TYPE::REGISTER,
      TOKEN_TYPE::COMMA,    TOKEN_TYPE::REGISTER, TOKEN_TYPE::COMMA,
      TOKEN_TYPE::REGISTER, TOKEN_TYPE::COMMA,    TOKEN_TYPE::SHIFT_OP,
      TOKEN_TYPE::SHIFT_AMT};

  inline static const std::vector<TOKEN_TYPE> IA_MODEL = {
      TOKEN_TYPE::MNEMONIC, TOKEN_TYPE::COMMA,    TOKEN_TYPE::REGISTER,
      TOKEN_TYPE::COMMA,    TOKEN_TYPE::REGISTER, TOKEN_TYPE::COMMA,
      TOKEN_TYPE::IMMEDIATE};

  inline static const std::vector<TOKEN_TYPE> IB_MODEL = {
      TOKEN_TYPE::MNEMONIC, TOKEN_TYPE::COMMA,     TOKEN_TYPE::REGISTER,
      TOKEN_TYPE::COMMA,    TOKEN_TYPE::IMMEDIATE, TOKEN_TYPE::COMMA,
      TOKEN_TYPE::REGISTER};

  inline static const std::vector<TOKEN_TYPE> J_MODEL = {
      TOKEN_TYPE::MNEMONIC, TOKEN_TYPE::COMMA, TOKEN_TYPE::IMMEDIATE};

  inline static const std::vector<TOKEN_TYPE> J_TYPE_MODEL = {
      TOKEN_TYPE::MNEMONIC, TOKEN_TYPE::COMMA, TOKEN_TYPE::REGISTER,
      TOKEN_TYPE::COMMA, TOKEN_TYPE::IMMEDIATE};  // Native J-Type Branches

  inline static const std::vector<TOKEN_TYPE> RBR_MODEL = {TOKEN_TYPE::MNEMONIC,
                                             TOKEN_TYPE::COMMA,
                                             TOKEN_TYPE::REGISTER};  // R-Branch

  inline static const std::vector<TOKEN_TYPE> MEM_MODEL = {
      TOKEN_TYPE::MNEMONIC, TOKEN_TYPE::COMMA,    TOKEN_TYPE::REGISTER,
      TOKEN_TYPE::COMMA,    TOKEN_TYPE::REGISTER, TOKEN_TYPE::COMMA,
      TOKEN_TYPE::IMMEDIATE};  // Load/Store

  inline static const std::vector<TOKEN_TYPE> UNARY_MODEL = {
      TOKEN_TYPE::MNEMONIC, TOKEN_TYPE::COMMA, TOKEN_TYPE::REGISTER,
      TOKEN_TYPE::COMMA, TOKEN_TYPE::REGISTER};  // NOT, CLZ

  inline static const std::vector<TOKEN_TYPE> NOP_MODEL = {TOKEN_TYPE::MNEMONIC};
  inline static const std::vector<TOKEN_TYPE> MOV_MODEL = {
      TOKEN_TYPE::MNEMONIC, TOKEN_TYPE::COMMA, TOKEN_TYPE::REGISTER,
      TOKEN_TYPE::COMMA, TOKEN_TYPE::REGISTER};
  inline static const std::vector<TOKEN_TYPE> CLR_MODEL = {
      TOKEN_TYPE::MNEMONIC, TOKEN_TYPE::COMMA, TOKEN_TYPE::REGISTER};
  inline static const std::vector<TOKEN_TYPE> NEG_MODEL = {
      TOKEN_TYPE::MNEMONIC, TOKEN_TYPE::COMMA, TOKEN_TYPE::REGISTER,
      TOKEN_TYPE::COMMA, TOKEN_TYPE::REGISTER};
  inline static const std::vector<TOKEN_TYPE> LI_MODEL = {
      TOKEN_TYPE::MNEMONIC, TOKEN_TYPE::COMMA, TOKEN_TYPE::REGISTER,
      TOKEN_TYPE::COMMA, TOKEN_TYPE::IMMEDIATE};
  inline static const std::vector<TOKEN_TYPE> PUSH_MODEL = {
      TOKEN_TYPE::MNEMONIC, TOKEN_TYPE::COMMA, TOKEN_TYPE::REGISTER};
  inline static const std::vector<TOKEN_TYPE> POP_MODEL = {
      TOKEN_TYPE::MNEMONIC, TOKEN_TYPE::COMMA, TOKEN_TYPE::REGISTER};
  inline static const std::vector<TOKEN_TYPE> SHIFT_MODEL = {
      TOKEN_TYPE::MNEMONIC, TOKEN_TYPE::COMMA,    TOKEN_TYPE::REGISTER,
      TOKEN_TYPE::COMMA,    TOKEN_TYPE::REGISTER, TOKEN_TYPE::COMMA,
      TOKEN_TYPE::IMMEDIATE};
  inline static const std::vector<TOKEN_TYPE> INC_MODEL = {
      TOKEN_TYPE::MNEMONIC, TOKEN_TYPE::COMMA, TOKEN_TYPE::REGISTER};
  inline static const std::vector<TOKEN_TYPE> SWAP_MODEL = {
      TOKEN_TYPE::MNEMONIC, TOKEN_TYPE::COMMA, TOKEN_TYPE::REGISTER,
      TOKEN_TYPE::COMMA, TOKEN_TYPE::REGISTER};
  inline static const std::vector<TOKEN_TYPE> CMP_MODEL = {
      TOKEN_TYPE::MNEMONIC, TOKEN_TYPE::COMMA, TOKEN_TYPE::REGISTER,
      TOKEN_TYPE::COMMA, TOKEN_TYPE::REGISTER};
  inline static const std::vector<TOKEN_TYPE> BEQZ_MODEL = {
      TOKEN_TYPE::MNEMONIC, TOKEN_TYPE::COMMA, TOKEN_TYPE::REGISTER,
      TOKEN_TYPE::COMMA, TOKEN_TYPE::IMMEDIATE};
  inline static const std::vector<TOKEN_TYPE> ABS_MODEL = {
      TOKEN_TYPE::MNEMONIC, TOKEN_TYPE::COMMA, TOKEN_TYPE::REGISTER,
      TOKEN_TYPE::COMMA, TOKEN_TYPE::REGISTER};
  inline static const std::vector<TOKEN_TYPE> SEQ_MODEL = {
      TOKEN_TYPE::MNEMONIC, TOKEN_TYPE::COMMA,    TOKEN_TYPE::REGISTER,
      TOKEN_TYPE::COMMA,    TOKEN_TYPE::REGISTER, TOKEN_TYPE::COMMA,
      TOKEN_TYPE::REGISTER};

  Parser(const std::vector<std::vector<Token>>& tokens);
  void parse(bool verbose = false);
};
}  // namespace sam32asm
