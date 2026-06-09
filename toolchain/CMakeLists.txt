cmake_minimum_required(VERSION 3.24)
project(sam32 LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(sam32
  src/assembler/lexer.cpp
  src/assembler/parser.cpp
  src/assembler/compiler.cpp
  src/emulator/emulator.cpp
)
target_sources(sam32 PRIVATE
  src/main.cpp
)

target_include_directories(sam32 PUBLIC include)

# vcpkg
# fmt
find_package(fmt CONFIG REQUIRED)
target_link_libraries(sam32 PRIVATE fmt::fmt)

# CLI11
find_package(CLI11 CONFIG REQUIRED)
target_link_libraries(sam32 PRIVATE CLI11::CLI11)

# indicators
find_package(indicators CONFIG REQUIRED)
target_link_libraries(sam32 PRIVATE indicators::indicators)
