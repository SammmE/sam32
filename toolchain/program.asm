; --- Initialization ---
LI R1, #10           ; Loop counter (Tests 32-bit LI chunking)
LI R2, 0x1000        ; Array base memory address (Tests Hex lexing)
CLR R3               ; Fib_A = 0 
LI R4, #1            ; Fib_B = 1

; --- Main Loop ---
LOOP_START:
SW R3, R2, #0        ; Store current Fibonacci number to RAM at address in R2
ADD R2, R2, #4       ; Advance memory pointer by 4 bytes (1 word)

ADD.F R5, R3, R4     ; Temp = A + B (Tests .F Freeze flag and standard R-Type)
MOV R3, R4           ; A = B (Tests MOV macro)
MOV R4, R5           ; B = Temp

DEC R1               ; Decrement counter
BNEZ R1, LOOP_START  ; Branch if R1 != 0 (Tests BNEZ macro and backward label offset)

; --- Edge Case Parser Testing ---
SUB R10, #0x50, R3   ; Non-commutative immediate in Rs1 slot (I-Type B)
PUSH R10             ; Tests multi-line macro generation with memory interaction

END_HALT:
JMP END_HALT           ; Infinite loop to trap CPU (Tests same-line/forward label)