`timescale 1ns / 1ps

module control_unit(
    input logic [31:0] instruction,

    output logic [6:0] opcode,

    output logic [4:0] Rd,
    output logic [4:0] Rs1,
    output logic [4:0] Rs2,

    output logic [1:0] st, // shift type: 00 - LSL, 01 - LSR, 10 - ASR, 11: ROR
    output logic [4:0] sa, // shift amount
    output logic fs, // freeze flag 1 if freeze sr 

    output logic Rs1Imm, // is Rs1 immediate?
    output logic Rs2Imm, // is Rs2 immediate?
    output logic is_offset, // is there an offset?
    output logic [12:0] Rs1ImmVal,
    output logic [12:0] Rs2ImmVal,
    output logic [17:0] offset,

    output logic ALUMath, // category 00
    output logic Branch, // cateogry 01
    output logic rjmp, // category 01 and MSB operation bit 1 (register jump)
    output logic Load, // cagtegory 10 and MSB operation bit 1
    output logic Store, // category 10 and MSB operation bit 0
    output logic [4:0] operation,

    output logic csr_read, // category 11 and operation 00000
    output logic csr_write, // category 11 and operation 00001 OR 00010
    output logic reti, // category 11 and operation 00011
    output logic ecall // category 11 and operation 01000
);
    // temp stuff
    logic [1:0] category;
    logic [4:0] op;
    logic rs1_imm_toggle;
    logic rs2_imm_toggle;

    assign category = instruction[1:0];
    assign op = instruction[6:2];
    assign rs1_imm_toggle = instruction[7];
    assign rs2_imm_toggle = instruction[8];

    assign opcode = instruction[6:0];

    assign ALUMath = (category == 2'b00);
    assign Branch = (category == 2'b01);
    assign rjmp = (category == 2'b01) && instruction[6]; // MSB of operatoin field
    assign Load = (category == 2'b10) && instruction[6];
    assign Store = (category == 2'b10) && ~instruction[6];

    assign Rs1Imm = rs1_imm_toggle;
    assign Rs2Imm = rs2_imm_toggle;

    always_comb begin
        Rd = 5'b0;
        Rs1 = 5'b0;
        Rs2 = 5'b0;
        st = 2'b0;
        sa = 5'b0;
        fs = 1'b0;
        is_offset = 1'b0;
        Rs1ImmVal = 13'b0;
        Rs2ImmVal = 13'b0;
        offset = 18'b0;
        operation = op;
        csr_read = 1'b0;
        csr_write = 1'b0;
        reti = 1'b0;
        ecall = 1'b0;

        if (category == 2'b00 || category == 2'b11) begin // ALU Math and System
            if (~rs1_imm_toggle && ~rs2_imm_toggle) begin
                // Format R
                Rd = instruction[13:9];
                Rs1 = instruction[18:14];
                Rs2 = instruction[23:19];
                st = instruction[25:24];
                sa = instruction[30:26];
                fs = instruction[31];
                
            end else if (rs2_imm_toggle && ~rs1_imm_toggle) begin
                // Format I-A
                Rd = instruction[13:9];
                Rs1 = instruction[18:14];
                Rs2ImmVal = instruction[31:19];
                
            end else if (rs1_imm_toggle && ~rs2_imm_toggle) begin
                // Format I-B 
                Rd = instruction[13:9];
                Rs2 = instruction[31:27];
                Rs1ImmVal = instruction[26:14];
            end
            
        end else if (category == 2'b01) begin // Branch / Jump
            if (rs1_imm_toggle && rs2_imm_toggle) begin
                // Format J
                Rd = instruction[13:9];
                is_offset = 1'b1;
                offset = instruction[31:14];
                
            end else if (~rs1_imm_toggle && ~rs2_imm_toggle && instruction[6]) begin
                // Format R-Branch
                Rs1 = instruction[13:9];
            end
            
        end else if (category == 2'b10) begin // Memory (Load/Store)
            // Memory strictly uses Format I-A layout
            Rd = instruction[13:9];
            Rs1 = instruction[18:14];
            Rs2ImmVal = instruction[31:19];
            is_offset = 1'b1;
            offset = { {5{instruction[31]}}, instruction[31:19] };
        end else if (category == 2'b11) begin // System
            case (op)
                5'b00000: csr_read = 1'b1;
                5'b00001, 5'b00010: csr_write = 1'b1;
                5'b00011: reti = 1'b1;
                5'b01000: ecall = 1'b1;
                default: begin
                    csr_read = 1'b0;
                    csr_write = 1'b0;
                    reti = 1'b0;
                    ecall = 1'b0;
                end
            endcase
        end
    end

endmodule