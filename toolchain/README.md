# SAM32 Emulator Features

The SAM32 toolchain includes a powerful, custom-built emulator designed to provide an advanced debugging and visualization environment for your newly defined Instruction Set Architecture. It is available both as a local desktop application and a web-based utility (via Emscripten).

## Core Architecture Support
*   **Full SAM32 ISA Execution**: Complete hardware-accurate emulation of Format R, Format I-A, Format I-B, Format J, and Format R-Branch instructions.
*   **Byte-addressable Memory Engine**: Operates natively on a flat `std::vector<uint8_t>` byte stream for perfect structural alignment with actual binary footprints.
*   **Native ALU Flags**: Accurately evaluates and routes zero (`Z`), negative (`N`), carry (`C`), and overflow (`V`) condition codes.
*   **Hardware Modifiers**: 
    *   **Inline Barrel Shifter**: Natively calculates and routes `LSL`, `LSR`, `ASR`, and `ROR` shifts during Register-to-Register operations.
    *   **Status Freeze (`.F`)**: Accurately traps the Status Register update pipeline when the freeze bit modifier is detected on an instruction.
    *   **Commutative Immediate Toggles**: Resolves dynamic immediate placements into ALU operand paths to allow for natively non-commutative mathematics (e.g. Subtraction).
*   **Hardwired Zero Register**: Enforces physical read-only limits on `R0` (always `0`).

## Interactive GUI Features
The emulator's graphical interface provides a rich, visually driven experience:

*   **Integrated Assembler Editor**: A built-in code editor featuring custom SAM32 syntax highlighting. It allows you to write assembly, click "Compile & Load," and instantly flash the resulting binary into the emulator's memory without touching a terminal.
*   **LST Assembly Viewer**: A split-screen representation of loaded memory that breaks your raw machine code back into a human-readable table, displaying the Address, Hex, Line Number, and Assembly String.
    *   *Visual PC Tracker:* Illuminates the table row corresponding to the active Program Counter, letting you follow along exactly like an IDE.
*   **Dynamic Register Grid**: A 32-cell view of all General Purpose Registers.
    *   *Radix Control:* Instantly toggle the grid between Hexadecimal, Signed Decimal, Unsigned Decimal, and Binary representations.
    *   *Live Mutator Highlights:* Any register that changes value after an execution step flashes Green.
    *   *Distinct Architecture Highlights:* Visually isolates `R0` (Zero), `R30` (Link Register), and `R31` (Stack Pointer).
    *   *Bit-Bucket Indicator:* Briefly flashes an alert (`>>> R0 BIT-BUCKET ROUTED! <<<`) when a mathematical operation's result is intentionally dumped into the zero register.
*   **Instruction BreakdownHUD (ALU Flowchart)**: Queries the exact historical runtime execution of an instruction, allowing you to see what `Operand 1` and `Operand 2` actually evaluated to *before* hitting the ALU, what the `ALU Output` was, and exactly which Memory Address or Register was modified.
*   **LED Status Flags**: Renders the `Z`, `N`, `C`, and `V` flags prominently. If the emulator detects the upcoming instruction has the `.F` (Freeze) bit set, it dims the LEDs and applies a `[FROZEN - FLAGS LOCKED]` badge.
*   **Interactive Watchlist**: A dedicated inspector table where you can add specific Registers (e.g., `R14`) or Memory Addresses (e.g., `0x1000`) and constantly track their live values during execution.

## Time-Travel Debugging
*   **Chronological State Tracking**: Every single step of the CPU saves a deeply integrated `EmulatorState` (including the exact Instruction Execution Details, Registers, Memory, and Status Flags) into a history vector.
*   **Flawless Undo**: Clicking the `Step Back` button completely rewinds the CPU context. Because the history contains ALU flowchart data, the GUI's "Instruction Details" and "Register Highlight" displays perfectly reverse-sync to the precise historical tick.
