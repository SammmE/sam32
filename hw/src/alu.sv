`timescale 1ns / 1ps

module alu(
    input logic [31:0] a,
    input logic [31:0] b,
    input logic [4:0] alu_op,
    output logic [31:0] result,
    output logic z,
    output logic n,
    output logic c,
    output logic v
);
    logic [5:0] clz_out;
    clz_32 clz_inst (
        .data_in(a),
        .lz_count(clz_out)
    );

    always_comb begin
        result = 32'b0;       

        case (alu_op)
            5'b00000: result = a + b; // ADD
            5'b00001: result = a - b; // SUB
            5'b00010: result = a * b; // MUL
            5'b10010: result = ({32'b0, a} * {32'b0, b}) >> 32; // MULH
            // 5'b00011: result = $signed(a) / $signed(b); // DIV
            // 5'b10011: result = $unsigned(a) / $unsigned(b); // DIVU
            // 5'b00100: result = $signed(a) % $signed(b); // REM
            // 5'b10100: result = $unsigned(a) % $unsigned(b); // REMU
            5'b00101: result = a & b; // AND
            5'b00110: result = a | b; // OR
            5'b00111: result = a ^ b; // XOR
            5'b01000: result = ~a; // NOT
            5'b01001: result = {{26{1'b0}}, clz_out}; // CLZ
        endcase
        z = (result == 32'b0);
        n = result[31];

        c = 1'b0;
        v = 1'b0;

        if (alu_op == 5'b00000) begin
            c = ({1'b0, a} + {1'b0, b}) > 32'hFFFFFFFF;
            v = (a[31] == b[31]) && (result[31] != a[31]);
        end else if (alu_op == 5'b00001) begin
            c = a < b;
            v = (a[31] != b[31]) && (result[31] != a[31]);
        end
    end
endmodule
