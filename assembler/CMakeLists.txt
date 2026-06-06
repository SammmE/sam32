cmake_minimum_required(VERSION 3.24)
project(sam32asm LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(sam32asm
  src/lexer.cpp
  src/parser.cpp
  src/compiler.cpp
)
target_sources(sam32asm PRIVATE
  src/main.cpp
)

target_include_directories(sam32asm PUBLIC include)

# vcpkg
# fmt
find_package(fmt CONFIG REQUIRED)
target_link_libraries(sam32asm PRIVATE fmt::fmt)

# CLI11
find_package(CLI11 CONFIG REQUIRED)
target_link_libraries(sam32asm PRIVATE CLI11::CLI11)

# indicators
find_package(indicators CONFIG REQUIRED)
target_link_libraries(sam32asm PRIVATE indicators::indicators)
