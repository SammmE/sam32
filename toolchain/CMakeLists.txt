cmake_minimum_required(VERSION 3.24)
project(sam32 LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find packages
find_package(fmt CONFIG REQUIRED)
find_package(CLI11 CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(SDL3 CONFIG REQUIRED)

# Common sources
set(COMMON_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/src/assembler/lexer.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/assembler/parser.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/assembler/compiler.cpp

  ${CMAKE_CURRENT_SOURCE_DIR}/src/emulator/emulator.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/emulator/gui.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/emulator/texteditor.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/emulator/ImGuiFileDialog.cpp

  ${CMAKE_CURRENT_SOURCE_DIR}/src/utils/utils.cpp
)

if(WEB_BUILD OR EMSCRIPTEN)
  add_executable(sam32_web src/web/main.cpp ${COMMON_SOURCES})
  target_include_directories(sam32_web PUBLIC include)
  target_link_libraries(sam32_web PRIVATE fmt::fmt imgui::imgui SDL3::SDL3)

  # Emscripten specific link flags
  set_target_properties(sam32_web PROPERTIES SUFFIX ".html")
  target_link_options(sam32_web PRIVATE
    "-sUSE_WEBGL2=1"
    "-sALLOW_MEMORY_GROWTH=1"
    "-sNO_DISABLE_EXCEPTION_CATCHING"
  )
else()
  add_executable(sam32
    ${COMMON_SOURCES}
  )
  target_sources(sam32 PRIVATE src/cli/main.cpp)
  target_include_directories(sam32 PUBLIC include)

  target_link_libraries(sam32 PRIVATE
    fmt::fmt
    CLI11::CLI11
    imgui::imgui
    SDL3::SDL3
  )

  # Setup external project for Web build so they build together
  include(ExternalProject)

  # Only add the web project if the EMSDK environment variable is available or we find it
  set(EMSDK_PATH "$ENV{EMSDK}")
  if(NOT EMSDK_PATH AND EXISTS "/usr/lib/emscripten")
    set(EMSDK_PATH "/usr/lib/emscripten")
  endif()

  if(EMSDK_PATH)
    ExternalProject_Add(sam32_web_ext
      SOURCE_DIR ${CMAKE_SOURCE_DIR}
      PREFIX ${CMAKE_BINARY_DIR}/web-ext
      BINARY_DIR ${CMAKE_BINARY_DIR}/web
      CMAKE_ARGS
      -DCMAKE_TOOLCHAIN_FILE=$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake
      -DVCPKG_CHAINLOAD_TOOLCHAIN_FILE=${EMSDK_PATH}/cmake/Modules/Platform/Emscripten.cmake
      -DVCPKG_TARGET_TRIPLET=wasm32-emscripten
      -DWEB_BUILD=ON
      -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
      BUILD_ALWAYS 1
      INSTALL_COMMAND ""
    )
  else()
    message(WARNING "EMSDK not found. Skipping web build target configuration.")
  endif()
endif()

add_executable(test_editor /home/sam/.gemini/antigravity/brain/ebea5e05-baf8-4caf-aaea-db07121a90f8/scratch/test_editor.cpp src/emulator/texteditor.cpp)
target_include_directories(test_editor PUBLIC include)
target_link_libraries(test_editor PRIVATE imgui::imgui SDL3::SDL3)