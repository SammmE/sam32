#pragma once

#include <cstdint>
#include <vector>

namespace sam32 {
struct Emulator {
 public:
  uint32_t registers[32];  // r0-r31
  uint32_t pc;             // program counter
  bool zero_flag;
  bool negative_flag;
  bool carry_flag;
  bool overflow_flag;

  std::vector<uint32_t> memory;

  Emulator(size_t memory_size = 1024 * 1024);  // Default to 1MB of memory

  void load_program(const std::vector<uint32_t>& program,
                    size_t start_address = 0);
  void tick();
};
}  // namespace sam32