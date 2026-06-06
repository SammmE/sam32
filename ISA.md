# **SAM32 ISA**

## **1. Core Philosophy & Overview**

This architecture is a streamlined, fixed-width 32-bit RISC processor designed for maximum hardware simplicity and software flexibility. It operates on a strict Load/Store paradigm, meaning mathematical operations occur exclusively between registers.

The marquee features of this architecture include:

*   **Commutative Immediate Toggles:** The ability to inject an immediate value into *either* the first or second operand slot dynamically, saving instructions on non-commutative math (e.g., Subtraction).
*   **Inline Barrel Shifter:** Native hardware support for shifting operands prior to execution in Register-to-Register operations.
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

## **3. The Global Opcode Block (Bits 31:23)**

Every instruction, regardless of type, begins with a strict 9-bit Opcode Block. This allows the hardware decoder to instantly route data through the CPU.

| Category [31:30] | Operation [29:25] | Imm Rs2 [24] | Imm Rs1 [23] |
| :---: | :---: | :---: | :---: |
| **Instruction Class**<br>`00`: ALU Math<br>`01`: Branch/Jump<br>`10`: Memory/Load-Store<br>`11`: System/Custom | **Specific Command**<br>(Up to 32 ops per category) | **Immediate Toggle B**<br>`1`: Transitions to I-Type B<br>`0`: Normal | **Immediate Toggle A**<br>`1`: Transitions to I-Type A<br>`0`: Normal |

*   **Category:** Determines the instruction class.
*   **Operation:** Specifies the exact command within the category.
*   **Imm Rs2 / Imm Rs1:** Mutually exclusive flags. If either is `1`, the hardware transitions to an **I-Type** layout.

---

## **4. Instruction Formats**

### **Format R (R-Type): Register-to-Register Operations**

**Trigger:** Category `00` (ALU), Imm Rs2 = `0`, Imm Rs1 = `0`.
Used for standard arithmetic and logical operations utilizing three registers and an optional hardware shift.

| Opcode Block [31:23] | Rd [22:18] | Rs1 [17:13] | Rs2 [12:8] | Shift Type [7:6] | Shift Amt [5:1] | F [0] |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| 9 Bits | 5 Bits | 5 Bits | 5 Bits | 2 Bits | 5 Bits | 1 Bit |

*   **Shift Type (Bits 7:6):** Defines the inline hardware shift behavior applied to `Rs2`.
    *   `00`: Logical Shift Left (LSL)
    *   `01`: Logical Shift Right (LSR)
    *   `10`: Arithmetic Shift Right (ASR)
    *   `11`: Reserved
*   **Shift Amount (Bits 5:1):** 5-bit unsigned integer dictating the shift distance (0 to 31 places).
*   **Freeze Status (Bit 0):**
    *   `0`: The ALU writes the results to `Rd` AND updates the Z, N, C, V flags.
    *   `1`: The ALU writes the results to `Rd` but leaves the Z, N, C, V flags completely unchanged (indicated by `.F` in assembly).

### **Format I (I-Type): Immediate Operations**

**Trigger:** Imm Rs2 = `1` **OR** Imm Rs1 = `1`.
The hardware decoder merges the unused register, shift parameters, and freeze bit into a single 13-bit signed immediate value (allowing constants from -4,096 to +4,095). *Note: I-Type instructions always update the Status flags.*

**Format I-A (Imm Rs2 = 1):** The immediate replaces the second operand.

| Opcode Block [31:23] | Rd [22:18] | Rs1 [17:13] | Immediate [12:0] |
| :---: | :---: | :---: | :---: |
| 9 Bits | 5 Bits | 5 Bits | 13-Bit Signed |

**Format I-B (Imm Rs1 = 1):** The immediate replaces the first operand. *(Note: Immediate field is placed before Rs2 to maintain logical operand ordering in the bitstream).*

| Opcode Block [31:23] | Rd [22:18] | Immediate [17:5] | Rs2 [4:0] |
| :---: | :---: | :---: | :---: |
| 9 Bits | 5 Bits | 13-Bit Signed | 5 Bits |

### **Format J (J-Type): PC-Relative Branch and Jump Operations**

**Trigger:** Category Bits = `01` (Branch) for PC-relative operations.
Converts all bits following `Rd` into an 18-bit signed jump offset, allowing jumps of ±131,072 instructions.

| Opcode Block [31:23] | Rd [22:18] | Branch Offset [17:0] |
| :---: | :---: | :---: |
| 9 Bits | 5 Bits (Link Register) | 18-Bit Signed Offset |

*   **Rd (Bits 22:18):** Used for "Jump and Link" operations to save the return address. Use the discard register (`R0`) if no link is needed, or the Link Register (`R30`) for function calls.

### **Format R-Branch: Register-Indirect Branches**

**Trigger:** Category `01` (Branch) with specific operations (`BR`, `BEQR`, etc.).
When a branch targets a register rather than a PC-relative offset, the instruction uses the R-Type layout.

| Opcode Block [31:23] | Rs1 (Target) [22:18] | Unused [17:0] |
| :---: | :---: | :---: |
| 9 Bits | 5 Bits | 17 Bits (Set to 0) |

*   **Rs1 (Bits 22:18):** Holds the target memory address to jump to.
*   **Linking:** Register-indirect branches do not automatically save a return address. To perform an indirect function call, software must manually save the return address to `R30` prior to the branch.

---

## **5. The Instruction Set**

### **Category 00: ALU Operations**

*Operates using R-Type or I-Type formats.*

| Mnemonic | Operation (5 Bits) | Description | Math Logic |
| --- | --- | --- | --- |
| `ADD` | `00000` | Addition | `Rd = Rs1 + Rs2` |
| `SUB` | `00001` | Subtraction | `Rd = Rs1 - Rs2` |
| `MUL` | `00010` | Multiply | `Rd = Rs1 * Rs2` |
| `DIV` | `00011` | Divide (Unsigned) | `Rd = Rs1 / Rs2` |
| `MOD` | `00100` | Modulo (Unsigned) | `Rd = Rs1 % Rs2` |
| `AND` | `00101` | Bitwise AND | `Rd = Rs1 & Rs2` |
| `OR`  | `00110` | Bitwise OR | `Rd = Rs1 \| Rs2` |
| `XOR` | `00111` | Bitwise XOR | `Rd = Rs1 ^ Rs2` |
| `NOT` | `01010` | Bitwise NOT | `Rd = ~Rs1` (Rs2 ignored) |
| `CLZ` | `01011` | Count Leading Zeros | `Rd = count_leading_zeros(Rs1)` (Rs2 ignored) |

### **Category 01: Branching & Jumps**

*Operates using the J-Type format for PC-relative branches, and the R-Branch format for Register-indirect branches.*

| Mnemonic | Operation (5 Bits) | Condition Flag Checked | Format | Description |
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

*Operates using a hybrid I-Type format, using the Immediate as a memory address offset.*

| Mnemonic | Operation (5 Bits) | Description |
| --- | --- | --- |
| `LW` | `10000` | **Load Word:** Reads a 32-bit word from memory at address `Rs1 + Imm` into `Rd`. |
| `SW` | `00000` | **Store Word:** Writes a 32-bit word from the register labeled `Rd` in the I-Type table into memory at address `Rs1 + Imm`. *(Note: For stores, the `Rd` field functionally acts as the Source Register `Rs`)*. |

---

## **6. Pseudo-Instructions & Assembler Macros**

The architecture's unique hardware features—such as the Commutative Immediate Toggles, the Inline Barrel Shifter, and the Zero Register (`R0`)—enable a highly efficient set of pseudo-instructions. The assembler automatically expands these mnemonics into optimal native instruction sequences.

### **1. Data Movement & Constants**
*   **`NOP` (No Operation):**
    *   **Expansion:** `ADD R0, R0, #0`
    *   **Why:** Adds zero to zero and discards the result.
*   **`MOV Rd, Rs` (Move Register):**
    *   **Expansion:** `ADD Rd, Rs, #0` (I-Type A)
    *   **Why:** Adds zero to the source register.
*   **`CLR Rd` (Clear Register):**
    *   **Expansion:** `MOV Rd, R0` or `ADD Rd, R0, #0`
*   **`NEG Rd, Rs` (Negate / Two's Complement):**
    *   **Expansion:** `SUB Rd, #0, Rs` (I-Type B)
    *   **Why:** This perfectly utilizes the **Imm Rs1** toggle! The hardware performs `Rd = Imm - Rs2`, which becomes `Rd = 0 - Rs`.
*   **`LI Rd, #imm32` (Load 32-bit Immediate):**
    *   **Expansion:** Since the immediate is limited to 13 bits, the assembler chunks larger constants.
    *   **Example (`LI R1, #0x12345`):**
        ```assembly
        ADD R1, R0, #0x12         ; Load upper bits
        ADD R1, R0, R1, LSL #12   ; Shift left by 12 (Barrel shifter shifts R1, R0 adds nothing)
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
*   **`PUSHM {Rlist}` / `POPM {Rlist}`:**
    *   **Expansion:** Assembler macros that generate multiple `SW`/`LW` instructions and a single `SUB`/`ADD` to adjust the SP by `4 * count`.

### **3. ALU & Bitwise Shortcuts**
The **Inline Barrel Shifter** makes isolated shift instructions very efficient because you can use `R0` as the first operand to "discard" the addition.
*   **`LSL Rd, Rs, #amt` (Logical Shift Left):**
    *   **Expansion:** `ADD Rd, R0, Rs, LSL #amt`
    *   **Why:** `Rd = 0 + (Rs << amt)`.
*   **`LSR Rd, Rs, #amt` (Logical Shift Right):**
    *   **Expansion:** `ADD Rd, R0, Rs, LSR #amt`
*   **`ASR Rd, Rs, #amt` (Arithmetic Shift Right):**
    *   **Expansion:** `ADD Rd, R0, Rs, ASR #amt`
*   **`INC Rd` / `DEC Rd`:**
    *   **Expansion:** `ADD Rd, Rd, #1` / `SUB Rd, Rd, #1`
*   **`SWAP R1, R2` (Register Swap without Temp):**
    *   **Expansion:**
        ```assembly
        XOR R1, R1, R2
        XOR R2, R1, R2
        XOR R1, R1, R2
        ```

### **4. Comparisons & Testing (The "Discard" Pattern)**
By directing the result of an operation to `R0` (hardwired to 0), you discard the mathematical result while still updating the **Z, N, C, V flags**. This mimics the classic ARM `CMP` and `TST` instructions.
*   **`CMP Rs1, Rs2` (Compare):**
    *   **Expansion:** `SUB R0, Rs1, Rs2`
    *   **Why:** Updates flags based on `Rs1 - Rs2`.
*   **`CMN Rs1, Rs2` (Compare Negative):**
    *   **Expansion:** `ADD R0, Rs1, Rs2`
    *   **Why:** Updates flags based on `Rs1 + Rs2` (useful for checking if `Rs1 == -Rs2`).
*   **`TST Rs1, Rs2` (Test Bits):**
    *   **Expansion:** `AND R0, Rs1, Rs2`
    *   **Why:** Updates flags based on `Rs1 & Rs2`.
*   **`TEQ Rs1, Rs2` (Test Equivalence):**
    *   **Expansion:** `XOR R0, Rs1, Rs2`
    *   **Why:** Updates flags based on `Rs1 ^ Rs2`.

### **5. Control Flow**
*   **`J label` (Unconditional Jump):**
    *   **Expansion:** `B R0, label` (Discards Link Address)
*   **`CALL label` (Function Call):**
    *   **Expansion:** `B R30, label` (Saves Return Address to Link Register R30)
*   **`RET` (Return from Function):**
    *   **Expansion:** `BR R30`
    *   **Why:** Jumps unconditionally to the address stored in the Link Register (`R30`). Since register-indirect branches use the R-Branch format, the target address is placed in the `Rs1` field.
*   **`BEQZ Rs, label` (Branch if Equal to Zero):**
    *   **Expansion:**
        ```assembly
        CMP Rs, R0
        BEQ R0, label
        ```
*   **`BNEZ Rs, label` (Branch if Not Equal to Zero):**
    *   **Expansion:**
        ```assembly
        CMP Rs, R0
        BNE R0, label
        ```

### **6. Conditional Math**
*   **`ABS Rd, Rs` (Absolute Value):**
    *   **Expansion:**
        ```assembly
        MOV Rd, Rs
        CMP Rd, R0
        BGE R0, #2      ; Skip next instruction if >= 0
        NEG Rd, Rd      ; (SUB Rd, #0, Rd)
        ```
*   **`SEQ Rd, Rs1, Rs2` (Set if Equal):**
    *   **Expansion:**
        ```assembly
        CMP Rs1, Rs2
        MOV Rd, #1
        BNE R0, #2      ; Skip next instruction if not equal
        MOV Rd, #0
        ```
    *   *Similar logic applies for `SNE`, `SLT` (Set if Less Than), `SGE`, etc.*

---

## **7. Assembly Syntax Examples**

**R-Type with Shift and Status Freeze:**
```assembly
SUB.F R5, R1, R2, LSL #4   ; R5 = R1 - (R2 << 4). Do not update flags.
```

**I-Type (Immediate Rs2):**
```assembly
ADD R5, R1, #1024          ; R5 = R1 + 1024. Updates flags.
```

**I-Type (Immediate Rs1 - Non-commutative efficiency):**
```assembly
SUB R5, #100, R2           ; R5 = 100 - R2. Updates flags.
```

**J-Type (Conditional Branch):**
```assembly
SUB R0, R1, R2             ; Pseudo-op 'CMP': Compare R1 and R2, discard result to R0, update status flags.
BEQ R0, #2048              ; If Z=1, jump forward 2048. Discard return address into R0.
```

**J-Type (Jump and Link / Function Call):**
```assembly
B R30, #-4000              ; Jump back 4000. Save current PC (return address) to R30 (Link Register).
```
