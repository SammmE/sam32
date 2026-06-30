# **SAM32 ISA Specification**

## **1. Core Overview**

The SAM32 is a fixed-width 32-bit RISC processor operating on a strict load/store paradigm. 

**Key Features:**
*   **Commutative Immediate Toggles:** Allows injection of an immediate value into either the first or second operand slot dynamically, facilitating non-commutative operations such as subtraction.
*   **Output Barrel Shifter & Rotator:** Hardware support for shifting and rotating the ALU output result before it is written to the destination register in R-Type operations.
*   **Centralized Status Register:** Global `Z` (Zero), `N` (Negative), `C` (Carry), and `V` (Overflow) flags for conditional branching, with a toggle to freeze flag states.
*   **Hardware Memory Protection:** Native logical-to-physical address translation and boundary checking to isolate user programs from OS memory.
*   **Automatic Context Saving:** Hardware automatically preserves ALU flags during interrupts/exceptions to prevent handlers from corrupting application state.

---

## **2. Register File & State**

*   **General Purpose Registers:** 32 x 32-bit registers (`R0` through `R31`).
    *   `R0`: **Zero Register.** Hardwired to `0`. Reads return 0; writes are discarded.
    *   `R1` – `R29`: **General Purpose.**
    *   `R30`: **Link Register (LR).** Stores return addresses for function calls.
    *   `R31`: **Stack Pointer (SP).** Points to the top of the stack.
*   **Status Register:** Stores the 4 primary arithmetic flags updated by the ALU:
    *   `Z` (Zero): Set if the ALU result is `0`.
    *   `N` (Negative): Set if the ALU result is negative (MSB is `1`).
    *   `C` (Carry): Set on unsigned overflow.
    *   `V` (Overflow): Set on signed overflow.

---

## **3. The Global Opcode Block (Bits 6:0)**

Every instruction begins with a 7-bit Opcode Block (Bits 0-6) holding a 2-bit Category and 5-bit Operation.

| Operation [6:2] | Category [1:0] |
| :---: | :---: |
| **Specific Command** | **Instruction Class**<br>`00`: ALU Math<br>`01`: Branch/Jump<br>`10`: Memory/Load-Store<br>`11`: System/Custom |

---

## **4. Instruction Formats**

The format is determined by the Category and the **Commutative Immediate Toggles** (Bits 8 and 7).

### **Format R (R-Type): Register-to-Register Operations**
**Trigger:** Category `00` (ALU) or `11` (System), `Rs1_IMM` = `0`, `Rs2_IMM` = `0`.

| F [31] | Shift Amt [30:26] | Shift Type [25:24] | Rs2 [23:19] | Rs1 [18:14] | Rd [13:9] | Rs2_IMM [8] | Rs1_IMM [7] | Opcode [6:0] |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| 1 Bit | 5 Bits | 2 Bits | 5 Bits | 5 Bits | 5 Bits | `0` | `0` | 7 Bits |

*   **Shift Type (Bits 25:24):** Hardware shift/rotate applied to the **ALU's output result** before writing to `Rd`.
    *   `00`: Logical Shift Left (LSL)
    *   `01`: Logical Shift Right (LSR)
    *   `10`: Arithmetic Shift Right (ASR)
    *   `11`: Rotate Right (ROR)
*   **Shift Amount (Bits 30:26):** 5-bit unsigned integer (0 to 31).
*   **Freeze Status (Bit 31):**
    *   `0`: Updates `Rd` and the Z, N, C, V flags.
    *   `1`: Updates `Rd` but leaves flags unchanged (indicated by `.F` in assembly).

### **Format I (I-Type): Immediate Operations**
**Trigger:** `Rs1_IMM` = `1` **OR** `Rs2_IMM` = `1` (Categories 00, 10, 11).
The unused register, shift parameters, and freeze bit form a 13-bit signed immediate (-4,096 to +4,095). *I-Type instructions always update Status flags; Freeze is disabled.*

**Format I-A (`Rs2_IMM` = 1):** Immediate replaces the second operand.
| Immediate [31:19] | Rs1 [18:14] | Rd [13:9] | Rs2_IMM [8] | Rs1_IMM [7] | Opcode [6:0] |
| :---: | :---: | :---: | :---: | :---: | :---: |
| 13-Bit Signed | 5 Bits | 5 Bits | `1` | `0` | 7 Bits |

**Format I-B (`Rs1_IMM` = 1):** Immediate replaces the first operand.
| Rs2 [31:27] | Immediate [26:14] | Rd [13:9] | Rs2_IMM [8] | Rs1_IMM [7] | Opcode [6:0] |
| :---: | :---: | :---: | :---: | :---: | :---: |
| 5 Bits | 13-Bit Signed | 5 Bits | `0` | `1` | 7 Bits |

### **Format J (J-Type): PC-Relative Branch and Jump Operations**
**Trigger:** Category `01` (Branch), `Rs1_IMM` = `1`, `Rs2_IMM` = `1`.
Encodes an 18-bit signed jump offset in bits [31:14] (±131,072 instructions, word-aligned).

| Branch Offset [31:14] | Rd [13:9] | Rs2_IMM [8] | Rs1_IMM [7] | Opcode [6:0] |
| :---: | :---: | :---: | :---: | :---: |
| 18-Bit Signed Offset | 5 Bits | `1` | `1` | 7 Bits |

*   **Rd (Bits 13:9):** Destination register for "Jump and Link" to save the return address. Use `R0` to discard, or `R30` (LR) for function calls.

### **Format R-Branch: Register-Indirect Branches**
**Trigger:** Category `01` (Branch) with `Operation[6] == 1`, `Rs1_IMM` = `0`, `Rs2_IMM` = `0`.

| Unused [31:14] | Rs1 (Target) [13:9] | Rs2_IMM [8] | Rs1_IMM [7] | Opcode [6:0] |
| :---: | :---: | :---: | :---: | :---: |
| 18 Bits (Set to 0) | 5 Bits | `0` | `0` | 7 Bits |

*   **Rs1 (Bits 13:9):** Target memory address register to jump to.

---

## **5. The Instruction Set**

### **Category 00: ALU Operations**
| Mnemonic | Operation [6:2] | Description | Math Logic |
| --- | --- | --- | --- |
| `ADD` | `00000` | Addition | `Rd = Rs1 + Rs2` |
| `SUB` | `00001` | Subtraction | `Rd = Rs1 - Rs2` |
| `MUL` | `00010` | Multiply | `Rd = (Rs1 * Rs2)[31:0]` |
| `MULH` | `10010` | Multiply High | `Rd = (Rs1 * Rs2)[63:32]` |
| `DIV` | `00011` | Divide | `Rd = Rs1 / Rs2` |
| `DIVU` | `10011` | Divide (Unsigned) | `Rd = Rs1 / Rs2` |
| `REM` | `00100` | Modulo | `Rd = Rs1 % Rs2` |
| `REMU` | `10100` | Modulo (Unsigned) | `Rd = Rs1 % Rs2` |
| `AND` | `00101` | Bitwise AND | `Rd = Rs1 & Rs2` |
| `OR`  | `00110` | Bitwise OR | `Rd = Rs1 \| Rs2` |
| `XOR` | `00111` | Bitwise XOR | `Rd = Rs1 ^ Rs2` |
| `NOT` | `01000` | Bitwise NOT | `Rd = ~Rs1` (Rs2 ignored) |
| `CLZ` | `01001` | Count Leading Zeros | `Rd = count_leading_zeros(Rs1)` (Rs2 ignored) |

### **Category 01: Branching & Jumps**
| Mnemonic | Operation [6:2] | Condition Flag Checked | Format | Description |
| --- | --- | --- | --- | --- |
| `B` | `00000` | None | J-Type | Unconditional PC-Relative Jump. Saves return address to `Rd`. |
| `BEQ` | `00001` | `Z == 1` | J-Type | Branch if Equal. |
| `BNE` | `00010` | `Z == 0` | J-Type | Branch if Not Equal. |
| `BLT` | `00011` | `N != V` | J-Type | Branch if Less Than (Signed). |
| `BGE` | `00100` | `N == V` | J-Type | Branch if Greater/Equal (Signed). |
| `BLE` | `00101` | `Z == 1 \|\| N != V` | J-Type | Branch if Less/Equal (Signed). |
| `BGT` | `00110` | `Z == 0 && N == V` | J-Type | Branch if Greater Than (Signed). |
| `BCS` | `00111` | `C == 1` | J-Type | Branch if Carry Set. |
| `BCC` | `01000` | `C == 0` | J-Type | Branch if Carry Clear. |
| `BMI` | `01001` | `N == 1` | J-Type | Branch if Minus. |
| `BPL` | `01010` | `N == 0` | J-Type | Branch if Plus. |
| `BR` | `10000` | None | R-Branch | Unconditional Jump to address in `Rs1`. |
| `BEQR` | `10001` | `Z == 1` | R-Branch | Jump to `Rs1` if Equal. |
| `BNER` | `10010` | `Z == 0` | R-Branch | Jump to `Rs1` if Not Equal. |
| `BLTR` | `10011` | `N != V` | R-Branch | Jump to `Rs1` if Less Than. |
| `BGER` | `10100` | `N == V` | R-Branch | Jump to `Rs1` if Greater/Equal. |
| `BLER` | `10101` | `Z == 1 \|\| N != V` | R-Branch | Jump to `Rs1` if Less/Equal. |
| `BGTR` | `10110` | `Z == 0 && N == V` | R-Branch | Jump to `Rs1` if Greater Than. |
| `BCSR` | `10111` | `C == 1` | R-Branch | Jump to `Rs1` if Carry Set. |
| `BCCR` | `11000` | `C == 0` | R-Branch | Jump to `Rs1` if Carry Clear. |
| `BMIR` | `11001` | `N == 1` | R-Branch | Jump to `Rs1` if Minus. |
| `BPLR` | `11010` | `N == 0` | R-Branch | Jump to `Rs1` if Plus. |

### **Category 10: Memory (Load / Store)**
*Operates exclusively using **Format I-A**. `Rs1` is the Base Address, `Immediate` is the Byte Offset.*

| Mnemonic | Operation [6:2] | Description |
| :--- | :---: | :--- |
| `SB` | `00000` | **Store Byte:** Writes an 8-bit byte from `Rd` to `Rs1 + Imm`. |
| `SH` | `00001` | **Store Half-Word:** Writes a 16-bit half-word from `Rd` to `Rs1 + Imm`. |
| `SW` | `00010` | **Store Word:** Writes a 32-bit word from `Rd` to `Rs1 + Imm`. |
| `LB` | `10000` | **Load Byte (Signed):** Reads an 8-bit signed byte from `Rs1 + Imm` to `Rd`. |
| `LBU` | `11000` | **Load Byte Unsigned:** Reads an 8-bit unsigned byte from `Rs1 + Imm` to `Rd`. |
| `LH` | `10001` | **Load Half-Word (Signed):** Reads a 16-bit signed half-word from `Rs1 + Imm` to `Rd`. |
| `LHU` | `11001` | **Load Half-Word Unsigned:** Reads a 16-bit unsigned half-word from `Rs1 + Imm` to `Rd`. |
| `LW` | `10010` | **Load Word:** Reads a 32-bit word from `Rs1 + Imm` to `Rd`. |

### **Category 11: System & Custom Operations**
*The 5-bit `Rd` field [13:9] acts as the CSR address index (0-31). `Rs1` [18:14] acts as the GPR source/destination. If `Rs1_IMM` is toggled to `1`, Format I-B is used and the 13-bit immediate acts as the source data.*

| Mnemonic | Operation [6:2] | Format | Description |
| :--- | :---: | :--- | :--- |
| `CSRR` | `00000` | R-Type | **Read CSR:** Reads the value from the CSR at address `Rd` and writes it to GPR `Rs1`. |
| `CSRW` | `00001` | R-Type / I-B | **Write CSR:** Writes the value from GPR `Rs1` (or a 13-bit Immediate via Format I-B) into the CSR at address `Rd`. |
| `RETI` | `00011` | R-Type | **Return from Exception/Interrupt:** Restores the PC from the `EPC` register, restores `STATUS` flags from `ESTATUS`, and sets `GIE` to 1. (All Reg/Shift fields must be 0). |
| `ECALL` | `01000` | R-Type | **Environment Call:** Triggers a synchronous exception (Syscall). Sets `CAUSE` to `0x00000008`. (All Reg/Shift fields must be 0). |

---

## **6. Pseudo-Instructions & Assembler Macros**

### **1. Data Movement & Constants**
*   **`NOP`:** `ADD R0, R0, #0`
*   **`MOV Rd, Rs`:** `ADD Rd, Rs, #0` (I-Type A)
*   **`NEG Rd, Rs`:** `SUB Rd, #0, Rs` (I-Type B)
*   **`LUI Rd, #imm13` (Load Upper Immediate):**
    *   **Expansion:** `ADD Rd, R0, #imm13, LSL #19`
    *   **Function:** Places a 13-bit immediate into the upper 13 bits (bits 31:19) of the register, zeroing the lower 19 bits. Essential for building 32-bit memory addresses.
*   **`LA Rd, label` (Load Address):**
    *   **Expansion:** 
        ```assembly
        LUI Rd, #(label >> 19)
        ADD Rd, Rd, #(label & 0x7FFFF) ; Assembler handles chunking if > 13 bits
        ```
*   **`LI Rd, #imm32` (Load 32-bit Immediate):**
    *   **Expansion:** Chunks larger constants using shifts and adds.
    *   **Example (`LI R1, #0x12345`):**
        ```assembly
        ADD R1, R0, #0x12         ; Load upper bits
        ADD R1, R0, R1, LSL #12   ; ALU computes 0+R1=R1; Output Shifter shifts left by 12.
        ADD R1, R1, #0x345        ; Add lower bits
        ```

### **2. Stack Operations**
*   **`PUSH Rs`:**
    ```assembly
    SUB R31, R31, #4
    SW Rs, R31, #0
    ```
*   **`POP Rd`:**
    ```assembly
    LW Rd, R31, #0
    ADD R31, R31, #4
    ```

### **3. ALU & Bitwise Shortcuts**
*   **`LSL Rd, Rs, #amt`:** `ADD Rd, R0, Rs, LSL #amt`
*   **`LSR / ASR / ROR`:** `ADD Rd, R0, Rs, [LSR/ASR/ROR] #amt`
*   **`INC / DEC`:** `ADD Rd, Rd, #1` / `SUB Rd, Rd, #1`

### **4. Comparisons & Testing**
*   **`CMP Rs1, Rs2`:** `SUB R0, Rs1, Rs2`
*   **`TST Rs1, Rs2`:** `AND R0, Rs1, Rs2`

### **5. Control Flow**
*   **`JMP label`:** `B R0, label`
*   **`CALL label`:** `B R30, label`
*   **`RET`:** `BR R30`

### **6. System & Interrupt Macros**
*   **`EIC` (Enable Interrupts Globally):** `CSRW 0x04, #1`
*   **`DIC` (Disable Interrupts Globally):** `CSRW 0x04, #0`

---

## **7. Assembly Syntax Examples**

**R-Type with Shift and Status Freeze:**
```assembly
SUB.F R5, R1, R2, LSL #4   ; R5 = (R1 - R2) << 4, flags unchanged.
```

**R-Type with Rotate Right:**
```assembly
ADD R5, R1, R2, ROR #8     ; R5 = (R1 + R2) ROR 8.
```

**I-Type (Immediate Rs2):**
```assembly
ADD R5, R1, #1024          ; R5 = R1 + 1024.
```

**I-Type (Immediate Rs1):**
```assembly
SUB R5, #100, R2           ; R5 = 100 - R2.
```

**J-Type (Conditional Branch):**
```assembly
SUB R0, R1, R2             ; Compare R1 and R2, update flags.
BEQ R0, #2048              ; If Z=1, jump forward 2048 instructions.
```

**J-Type (Jump and Link):**
```assembly
B R30, #-4000              ; Jump back 4000, save return address to R30.
```

**System Operations (CSRs & Syscalls):**
```assembly
CSRR 0x00, R5              ; Read STATUS register (0x00) into R5.
CSRW 0x03, R10             ; Write R10 to TVEC register (0x03).
CSRW 0x04, #1              ; Write 1 to GIE register (Uses Format I-B immediate).
ECALL                      ; Trigger Syscall exception (OS handles request).
RETI                       ; Return from exception/interrupt.
```

---

## **8. Control and Status Registers (CSRs)**

| Address | Name | Description |
| :--- | :--- | :--- |
| `0x00` | `STATUS` | **Status & Flags Register.** Bits [3:0]: `V`, `C`, `N`, `Z` flags. |
| `0x01` | `EPC` | **Exception Program Counter.** Stores trapping instruction's PC upon exception/interrupt. `RETI` restores PC. |
| `0x02` | `CAUSE` | **Trap Cause Register.** Bit [31]: `1` = Interrupt, `0` = Exception. Bits [30:0]: Exception/Interrupt Code. |
| `0x03` | `TVEC` | **Trap Vector Base Address.** Memory address of the interrupt/exception handler. The processor hardware jumps directly to the address stored in `TVEC` upon any trap. |
| `0x04` | `GIE` | **Global Interrupt Enable.** Bit [0]: `1` = Enabled, `0` = Disabled. |
| `0x05` | `MTIME` | **Machine Timer.** Increments by 1 on every positive clock edge. |
| `0x06` | `MTIMECMP` | **Machine Timer Compare.** Compared to `MTIME` on every positive clock edge. If `MTIMECMP < MTIME`, a Machine Timer interrupt is fired. |
| `0x07` | `BOOT` | **Boot Mode Register.** Bit [0]: `1` = Boot Mode (memory accesses are redirected to ROM instead of RAM), `0` = Normal Mode. |
| `0x08` | `ESTATUS` | **Exception Status Register.** Hardware automatically saves the `STATUS` flags (Z, N, C, V) here immediately upon trapping. `RETI` restores these flags back to `STATUS`. |
| `0x09` | `UMEM_BASE` | **User Memory Base.** The hardware address decoder adds this value to the logical address generated by Load/Store instructions to calculate the physical RAM address. |
| `0x0A` | `UMEM_LIMIT` | **User Memory Limit.** The hardware address decoder checks the logical address against this limit. If the address exceeds this limit, a memory protection exception is triggered to prevent user programs from accessing out-of-bounds RAM. |

### **CAUSE Register Values**

| CAUSE Value (Hex) | Exception/Interrupt Type | Description |
| :--- | :--- | :--- |
| `0x80000001` | Machine Timer Interrupt | Generated by the timer compare logic; used for preemptive multitasking and system timekeeping. |
| `0x80000002` | External Interrupt (e.g., UART RX) | Generated by asynchronous peripheral devices to signal data availability and prevent input buffer overruns. |
| `0x00000000` | Illegal Instruction Exception | Triggered when the processor decodes an invalid or undefined opcode, enabling fault handling for corrupted execution flows. |
| `0x00000008` | Environment Call (Syscall) Exception | Triggered by the `ECALL` instruction to transition from user mode to supervisor mode for OS service requests. |