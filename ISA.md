# **SAM32 ISA Specification**

## **1. Core Philosophy & Overview**

This architecture is a streamlined, fixed-width 32-bit RISC processor designed for maximum hardware simplicity and software flexibility. It operates on a strict Load/Store paradigm, meaning mathematical operations occur exclusively between registers.

The marquee features of this architecture include:

*   **Commutative Immediate Toggles:** The ability to inject an immediate value into *either* the first or second operand slot dynamically, saving instructions on non-commutative math (e.g., Subtraction).
*   **Output Barrel Shifter & Rotator:** Native hardware support for shifting and rotating the **ALU output result** before it is written to the destination register in Register-to-Register operations.
*   **Centralized Status Register:** Relies on global `Z` (Zero), `N` (Negative), `C` (Carry), and `V` (Overflow) flags for conditional branching, with an explicit hardware toggle to freeze flag states.

---

## **2. Register File & State**

*   **General Purpose Registers:** 32 x 32-bit registers (`R0` through `R31`).
    *   `R0`: **Zero Register.** Hardwired to `0`. Reads always return 0; writes are discarded.
    *   `R1` – `R29`: **General Purpose.** Available for arbitrary software use.
    *   `R30`: **Link Register (LR).** Dedicated by the ABI to store return addresses for function calls.
    *   `R31`: **Stack Pointer (SP).** Dedicated by the ABI to point to the top of the stack.
*   **Status Register:** A dedicated internal register storing the 4 primary arithmetic flags updated by the ALU:
    *   `Z` (Zero): Set if the ALU result is exactly `0`.
    *   `N` (Negative): Set if the ALU result is negative (MSB is `1`).
    *   `C` (Carry): Set if an operation results in an unsigned overflow.
    *   `V` (Overflow): Set if an operation results in a signed overflow.

---

## **3. The Global Opcode Block (Bits 6:0)**

Every instruction, regardless of type, begins with a strict 7-bit Opcode Block at the very start of the instruction stream (Bit 0). This allows the hardware decoder to instantly route data through the CPU.

*(**Designer's Note:** Bits 0-6 encompass 7 bits total. This holds the 2-bit Category and 5-bit Operation, ensuring the instruction mathematically totals exactly 32 bits).*

| Operation [6:2] | Category [1:0] |
| :---: | :---: |
| **Specific Command**<br>(5 bits, up to 32 ops per category).<br>*Crucial Design Rule:*<br>• In Category `01`, `Operation[6] == 1` triggers Register-Indirect Branching (requires IMM toggles = `00`).<br>• In Category `01`, `Operation[6] == 0` triggers PC-Relative Branching (requires IMM toggles = `11`).<br>• In Category `10`, `Operation[6] == 1` triggers a Load, `0` triggers a Store. | **Instruction Class**<br>`00`: ALU Math<br>`01`: Branch/Jump<br>`10`: Memory/Load-Store<br>`11`: System/Custom |

---

## **4. Instruction Formats**

The architecture uses a unified format structure where fields are read logically. The format is determined by the Category and the **Commutative Immediate Toggles** (Bits 8 and 7).

### **Format R (R-Type): Register-to-Register Operations**

**Trigger:** Category `00` (ALU), `Rs1_IMM` = `0`, `Rs2_IMM` = `0`.
Used for standard arithmetic and logical operations utilizing three registers and an optional hardware shift/rotate on the output.

| F [31] | Shift Amt [30:26] | Shift Type [25:24] | Rs2 [23:19] | Rs1 [18:14] | Rd [13:9] | Rs2_IMM [8] | Rs1_IMM [7] | Opcode [6:0] |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| 1 Bit | 5 Bits | 2 Bits | 5 Bits | 5 Bits | 5 Bits | `0` | `0` | 7 Bits |

*   **Shift Type (Bits 25:24):** Defines the inline hardware shift/rotate behavior applied to the **ALU's output result** (before writing to `Rd`).
    *   `00`: Logical Shift Left (LSL)
    *   `01`: Logical Shift Right (LSR)
    *   `10`: Arithmetic Shift Right (ASR)
    *   `11`: Rotate Right (ROR)
*   **Shift Amount (Bits 30:26):** 5-bit unsigned integer dictating the shift/rotate distance (0 to 31 places).
*   **Freeze Status (Bit 31):**
    *   `0`: The ALU writes the results to `Rd` AND updates the Z, N, C, V flags.
    *   `1`: The ALU writes the results to `Rd` but leaves the Z, N, C, V flags completely unchanged (indicated by `.F` in assembly).

### **Format I (I-Type): Immediate Operations**

**Trigger:** `Rs1_IMM` = `1` **OR** `Rs2_IMM` = `1` (Valid for Categories 00 and 10).
The hardware decoder repurposes the unused register, shift parameters, and freeze bit into a single 13-bit signed immediate value (allowing constants from -4,096 to +4,095). *Note: I-Type instructions always update the Status flags; the Freeze feature is disabled.*

The 13-bit immediate's physical location in the instruction word shifts depending on which operand slot it replaces.

**Format I-A (`Rs2_IMM` = 1):** The immediate replaces the second operand. It is anchored to the upper bits.

| Immediate [31:19] | Rs1 [18:14] | Rd [13:9] | Rs2_IMM [8] | Rs1_IMM [7] | Opcode [6:0] |
| :---: | :---: | :---: | :---: | :---: | :---: |
| 13-Bit Signed | 5 Bits | 5 Bits | `1` | `0` | 7 Bits |

**Format I-B (`Rs1_IMM` = 1):** The immediate replaces the first operand. The unused `Rs2` register shifts to the upper bits, and the immediate occupies the middle.

| Rs2 [31:27] | Immediate [26:14] | Rd [13:9] | Rs2_IMM [8] | Rs1_IMM [7] | Opcode [6:0] |
| :---: | :---: | :---: | :---: | :---: | :---: |
| 5 Bits | 13-Bit Signed | 5 Bits | `0` | `1` | 7 Bits |

### **Format J (J-Type): PC-Relative Branch and Jump Operations**

**Trigger:** Category Bits = `01` (Branch) **AND BOTH** `Rs1_IMM` = `1` and `Rs2_IMM` = `1`.
This dual-toggle acts as a dedicated format selector, converting all bits following `Rd` into an 18-bit signed jump offset, allowing jumps of ±131,072 instructions (implicitly shifted by hardware for word alignment).

| Branch Offset [31:14] | Rd [13:9] | Rs2_IMM [8] | Rs1_IMM [7] | Opcode [6:0] |
| :---: | :---: | :---: | :---: | :---: |
| 18-Bit Signed Offset | 5 Bits (Link Register) | `1` | `1` | 7 Bits |

*   **Rd (Bits 13:9):** Used for "Jump and Link" operations to save the return address. Use the discard register (`R0`) if no link is needed, or the Link Register (`R30`) for function calls.

### **Format R-Branch: Register-Indirect Branches**

**Trigger:** Category `01` (Branch) with `Operation[6] == 1` (e.g., `BR`, `BEQR`) **AND BOTH** `Rs1_IMM` = `0` and `Rs2_IMM` = `0`.
When a branch targets a register rather than a PC-relative offset, the instruction uses the R-Type layout.

| Unused [31:14] | Rs1 (Target) [13:9] | Rs2_IMM [8] | Rs1_IMM [7] | Opcode [6:0] |
| :---: | :---: | :---: | :---: | :---: |
| 18 Bits (Set to 0) | 5 Bits | `0` | `0` | 7 Bits |

*   **Rs1 (Bits 13:9):** Holds the target memory address to jump to. *(Hardware Note: Decoder must route this field to the ALU/Branch logic as a source, rather than the Register File Write port).*

---

## **5. The Instruction Set**

### **Category 00: ALU Operations**
*Operates using R-Type or I-Type formats.*

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
*Format determined by the IMM toggles and `Operation[6]`: `11` toggles = J-Type (PC-Relative), `00` toggles + `Operation[6]==1` = R-Branch (Register-Indirect).*

| Mnemonic | Operation [6:2] | Condition Flag Checked | Format | Description |
| --- | --- | --- | --- | --- |
| `B` | `00000` | None | J-Type | Unconditional PC-Relative Jump. Saves return address to `Rd`. |
| `BEQ` | `00001` | `Z == 1` | J-Type | Branch if Equal (Zero). `Rd` ignored (use `R0`). |
| `BNE` | `00010` | `Z == 0` | J-Type | Branch if Not Equal. |
| `BLT` | `00011` | `N != V` | J-Type | Branch if Less Than (Signed). |
| `BGE` | `00100` | `N == V` | J-Type | Branch if Greater/Equal (Signed). |
| `BLE` | `00101` | `Z == 1 \|\| N != V` | J-Type | Branch if Less/Equal (Signed). |
| `BGT` | `00110` | `Z == 0 && N == V` | J-Type | Branch if Greater Than (Signed). |
| `BCS` | `00111` | `C == 1` | J-Type | Branch if Carry Set (Unsigned Higher). |
| `BCC` | `01000` | `C == 0` | J-Type | Branch if Carry Clear (Unsigned Lower). |
| `BMI` | `01001` | `N == 1` | J-Type | Branch if Minus (Negative). |
| `BPL` | `01010` | `N == 0` | J-Type | Branch if Plus (Positive). |
| `BR` | `10000` | None | R-Branch | Unconditional Jump to address in `Rs1`. |
| `BEQR` | `10001` | `Z == 1` | R-Branch | Jump to address in `Rs1` if Equal. |
| `BNER` | `10010` | `Z == 0` | R-Branch | Jump to address in `Rs1` if Not Equal. |
| `BLTR` | `10011` | `N != V` | R-Branch | Jump to address in `Rs1` if Less Than. |
| `BGER` | `10100` | `N == V` | R-Branch | Jump to address in `Rs1` if Greater/Equal. |
| `BLER` | `10101` | `Z == 1 \|\| N != V` | R-Branch | Jump to address in `Rs1` if Less/Equal. |
| `BGTR` | `10110` | `Z == 0 && N == V` | R-Branch | Jump to address in `Rs1` if Greater Than. |
| `BCSR` | `10111` | `C == 1` | R-Branch | Jump to address in `Rs1` if Carry Set. |
| `BCCR` | `11000` | `C == 0` | R-Branch | Jump to address in `Rs1` if Carry Clear. |
| `BMIR` | `11001` | `N == 1` | R-Branch | Jump to address in `Rs1` if Minus. |
| `BPLR` | `11010` | `N == 0` | R-Branch | Jump to address in `Rs1` if Plus. |

### **Category 10: Memory (Load / Store)**
*Operates exclusively using **Format I-A**. `Rs1` is the Base Address, `Immediate` is the Byte Offset, and `Rd` acts as the Data Source (for Stores) or Destination (for Loads).*

| Mnemonic | Operation [6:2] | Description |
| :--- | :---: | :--- |
| `SB` | `00000` | **Store Byte:** Writes an 8-bit byte from `Rd` to `Rs1 + Imm`.<br>*[0:Store \| 0:N/A \| 0:Norm \| 00:Byte]* |
| `SH` | `00001` | **Store Half-Word:** Writes a 16-bit half-word from `Rd` to `Rs1 + Imm`.<br>*[0:Store \| 0:N/A \| 0:Norm \| 01:Half]* |
| `SW` | `00010` | **Store Word:** Writes a 32-bit word from `Rd` to `Rs1 + Imm`.<br>*[0:Store \| 0:N/A \| 0:Norm \| 10:Word]* |
| `LB` | `10000` | **Load Byte (Signed):** Reads an 8-bit signed byte from `Rs1 + Imm` to `Rd`.<br>*[1:Load \| 0:Signed \| 0:Norm \| 00:Byte]* |
| `LBU` | `11000` | **Load Byte Unsigned:** Reads an 8-bit unsigned byte from `Rs1 + Imm` to `Rd`.<br>*[1:Load \| 1:Unsigned \| 0:Norm \| 00:Byte]* |
| `LH` | `10001` | **Load Half-Word (Signed):** Reads a 16-bit signed half-word from `Rs1 + Imm` to `Rd`.<br>*[1:Load \| 0:Signed \| 0:Norm \| 01:Half]* |
| `LHU` | `11001` | **Load Half-Word Unsigned:** Reads a 16-bit unsigned half-word from `Rs1 + Imm` to `Rd`.<br>*[1:Load \| 1:Unsigned \| 0:Norm \| 01:Half]* |
| `LW` | `10010` | **Load Word:** Reads a 32-bit word from `Rs1 + Imm` to `Rd`.<br>*[1:Load \| 0:N/A \| 0:Norm \| 10:Word]* |

---

## **6. Pseudo-Instructions & Assembler Macros**

The architecture's unique hardware features—such as the Commutative Immediate Toggles, the Output Barrel Shifter, and the Zero Register (`R0`)—enable a highly efficient set of pseudo-instructions.

### **1. Data Movement & Constants**
*   **`NOP` (No Operation):**
    *   **Expansion:** `ADD R0, R0, #0`
*   **`MOV Rd, Rs` (Move Register):**
    *   **Expansion:** `ADD Rd, Rs, #0` (I-Type A)
*   **`NEG Rd, Rs` (Negate / Two's Complement):**
    *   **Expansion:** `SUB Rd, #0, Rs` (I-Type B)
    *   **Why:** Utilizes the **Rs1_IMM** toggle! The hardware performs `Rd = Imm - Rs2`, which becomes `Rd = 0 - Rs`. The immediate `#0` sits in the middle of the instruction word, and `Rs` shifts to the upper bits.
*   **`LI Rd, #imm32` (Load 32-bit Immediate):**
    *   **Expansion:** Chunks larger constants.
    *   **Example (`LI R1, #0x12345`):**
        ```assembly
        ADD R1, R0, #0x12         ; Load upper bits
        ADD R1, R0, R1, LSL #12   ; ALU computes 0+R1=R1; Output Shifter shifts left by 12.
        ADD R1, R1, #0x345        ; Add lower bits
        ```

### **2. Stack Operations (Using R31 as Stack Pointer)**
*   **`PUSH Rs`:**
    *   **Expansion:**
        ```assembly
        SUB R31, R31, #4          ; Decrement SP
        SW Rs, R31, #0            ; Store Rs to [SP]
        ```
*   **`POP Rd`:**
    *   **Expansion:**
        ```assembly
        LW Rd, R31, #0            ; Load from [SP] to Rd
        ADD R31, R31, #4          ; Increment SP
        ```

### **3. ALU & Bitwise Shortcuts**
*   **`LSL Rd, Rs, #amt` (Logical Shift Left):**
    *   **Expansion:** `ADD Rd, R0, Rs, LSL #amt`
    *   **Why:** `Rd = 0 + (Rs << amt)`.
*   **`LSR / ASR / ROR`:**
    *   **Expansion:** `ADD Rd, R0, Rs, [LSR/ASR/ROR] #amt`
*   **`INC / DEC`:**
    *   **Expansion:** `ADD Rd, Rd, #1` / `SUB Rd, Rd, #1`

### **4. Comparisons & Testing (The "Discard" Pattern)**
By directing the result of an operation to `R0`, you discard the mathematical result while still updating the **Z, N, C, V flags**.
*   **`CMP Rs1, Rs2`:** `SUB R0, Rs1, Rs2`
*   **`TST Rs1, Rs2`:** `AND R0, Rs1, Rs2`

### **5. Control Flow**
*   **`JMP label`:** `B R0, label` (Discards Link Address)
*   **`CALL label`:** `B R30, label` (Saves Return Address to Link Register R30)
*   **`RET`:** `BR R30`

---

## **7. Assembly Syntax Examples**

**R-Type with Shift and Status Freeze (Output Shifter):**
```assembly
SUB.F R5, R1, R2, LSL #4   ; R5 = (R1 - R2) << 4. Do not update flags (Bit 31 set to 1).
```

**R-Type with Rotate Right:**
```assembly
ADD R5, R1, R2, ROR #8     ; R5 = (R1 + R2) ROR 8.
```

**I-Type (Immediate Rs2):**
```assembly
ADD R5, R1, #1024          ; R5 = R1 + 1024. Updates flags. Immediate occupies upper bits [31:19].
```

**I-Type (Immediate Rs1 - Non-commutative efficiency):**
```assembly
SUB R5, #100, R2           ; R5 = 100 - R2. Updates flags. Immediate occupies middle bits [26:14], R2 shifts to [31:27].
```

**J-Type (Conditional Branch):**
```assembly
SUB R0, R1, R2             ; Pseudo-op 'CMP': Compare R1 and R2, discard result to R0, update status flags.
BEQ R0, #2048              ; If Z=1, jump forward 2048 instructions.
```

**J-Type (Jump and Link / Function Call):**
```assembly
B R30, #-4000              ; Jump back 4000. Save current PC (return address) to R30 (Link Register).
```