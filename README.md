# SAM32: **S**uper **A**wesome **M**achine 32-bit

This is my very own personal computer project!
A fully custom, ground-up 32-bit computer architecture complete with an ISA, an integrated assembler, a web emulator, and an in-development FPGA hardware implementation.

[Try it out in an emulator here](https://sam32.samhithe.dev/)

Everything is designed and programmed by me, from the ground up. The project has a number of milestones:

1. [ISA](/ISA.md)                   ✅
2. [CPU](/CPU)                      ✅
3. SystemVerilog
4. [Assembler](/toolchain)          ✅
5. [Emulator/Debugger](/toolchain)  ✅
6. CPU on an FPGA
7. Display output
8. Operating system

## Quick Start:
The fastest way to try it out is to:
1. Open the [web emulator](https://sam32.samhithe.dev/)
2. Write some assembly!
3. Click "Compile and Load"
4. Step through the instructions and watch the registers, RAM, and flags update in real time!

Example Assembly:
```asm
; SAM32 ISA Demo
LI R10, 0x2000
MOV R1, #5
MOV R2, #0
loop:
ADD R3, R2, #42
SW R3, R10, #0
ADD R10, R10, #4
ADD R2, R2, #1
SUB R1, R1, #1
BNE R0, loop
end:
JMP end
```

## Features:
1. ***Custom SAM32 ISA**: I made the ISA with a ton of inspiration from RISC-V and ARM!
2. **Full assembler**: The assembler supports directives, and has many pseudo-instructions to make it easier to make a program
3. **Interactive Emulator**: You don't need to flash the SystemVerilog onto an FPGA board to try SAM32 out! The Web Emulator/Debugger has a ton of features:
    - **Onboard assembler**
    - **Time-Travel Debugging**: Ever CPU step pushes the CPU state onto an array that can be popped from
    - **LST assembly viwer**: To make debugging easier, there is a built-in LST viwer that shows each instruction along with its: decompiled version, memory address, and line in the file
    - **Watchlist**: You can add memory addresses or registers to the watchlist to make it easy to keep track of them!
    - **Breakpoints**
4. **COMING SOON: Hardware and OS**: SystemVerilog implementation for FPGA, MMIO-based VGA output, and a UNIX-like OS!

## How to run it locally
***Prerequisites*:
- CMake
- a C compiler (I prefer Clang)
- vcpkg

```bash
git clone https://github.com/sammme/sam32.git
cd sam32

cd toolchain
cmake --preset default
cmake --build build

./build/sam32 --help
```

## Credits
- Built using Emscripten, Clang, CMake, VSCode, Neovim and Codicons
- [ocornut's ImGui](https://github.com/ocornut/imgui)
- [BalazsJako's ImGuiColorTextEdit](https://github.com/BalazsJako/ImGuiColorTextEdit)
- [juliettef's IconFontCppHeaders](https://github.com/juliettef/IconFontCppHeaders)
- [aiekick's ImGuiFileDialog](https://github.com/aiekick/ImGuiFileDialog)