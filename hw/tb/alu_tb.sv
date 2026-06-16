`timescale 1ns / 1ps

module alu_tb();
    logic [31:0] a, b;
    logic [4:0]  alu_op;
    logic [31:0] result;
    logic z, n, c, v;

    alu dut (
        .a(a), .b(b), .alu_op(alu_op), 
        .result(result), .z(z), .n(n), .c(c), .v(v)
    );

    logic [31:0] exp_result;
    logic exp_z, exp_n, exp_c, exp_v;
    logic [63:0] mul_full;

    task automatic test_case(input logic [31:0] a_in, input logic [31:0] b_in, input logic [4:0] op);
        a = a_in;
        b = b_in;
        alu_op = op;
        #10;

        exp_c = 1'b0;
        exp_v = 1'b0;

        case (op)
            5'b00000: begin // ADD
                exp_result = a + b;
                exp_c = ({1'b0, a} + {1'b0, b}) > 32'hFFFFFFFF;
                exp_v = (a[31] == b[31]) && (exp_result[31] != a[31]);
            end
            5'b00001: begin // SUB
                exp_result = a - b;
                exp_c = a < b;
                exp_v = (a[31] != b[31]) && (exp_result[31] != a[31]);
            end
            5'b00010: begin // MUL
                exp_result = a * b;
            end
            5'b10010: begin // MULH
                mul_full = {32'b0, a} * {32'b0, b};
                exp_result = mul_full >> 32;
            end
            5'b00101: exp_result = a & b; // AND
            5'b00110: exp_result = a | b; // OR
            5'b00111: exp_result = a ^ b; // XOR
            5'b01000: exp_result = ~a;    // NOT
            
            5'b01001: begin // CLZ
                exp_result = 32'd32;
                for(int j = 31; j >= 0; j = j - 1) begin
                    if(a[j]) begin 
                        exp_result = 31 - j; 
                        break; 
                    end
                end
            end
            
            default: exp_result = 32'bx;
        endcase

        exp_z = (exp_result == 32'b0);
        exp_n = exp_result[31];

        // Assertions
        assert(result === exp_result) else $fatal(1, "Result mismatch for op %b: Expected %h, got %h", op, exp_result, result);
        assert(z === exp_z) else $fatal(1, "Zero flag mismatch for op %b", op);
        assert(n === exp_n) else $fatal(1, "Negative flag mismatch for op %b", op);
        assert(c === exp_c) else $fatal(1, "Carry flag mismatch for op %b: Expected %b, got %b", op, exp_c, c);
        assert(v === exp_v) else $fatal(1, "Overflow flag mismatch for op %b: Expected %b, got %b", op, exp_v, v);
        
        $display("Test passed for op %b with a=%h, b=%h", op, a, b);
    endtask

    initial begin
        $display("Starting ALU Testbench...");
        
        test_case(32'd5, 32'd3, 5'b00000); // ADD basic
        test_case(32'hFFFFFFFF, 32'd1, 5'b00000); // ADD with Carry
        test_case(32'h7FFFFFFF, 32'd1, 5'b00000); // ADD with Overflow
        test_case(32'd10, 32'd4, 5'b00001); // SUB basic
        test_case(32'd4, 32'd10, 5'b00001); // SUB with Borrow
        test_case(32'hFF00FF00, 32'h0F0F0F0F, 5'b00101); // AND
        test_case(32'h80000000, 32'd0, 5'b01001); // CLZ (0 leading zeros)
        test_case(32'h00000001, 32'd0, 5'b01001); // CLZ (31 leading zeros)
        test_case(32'h00000000, 32'd0, 5'b01001); // CLZ (32 leading zeros)

        $display("Starting 1000 Randomized Tests...");
        
        for (int i = 0; i < 1000; i = i + 1) begin
            a = $urandom; 
            b = $urandom;
            
            case ($urandom_range(0, 8))
                0: alu_op = 5'b00000; // ADD
                1: alu_op = 5'b00001; // SUB
                2: alu_op = 5'b00010; // MUL
                3: alu_op = 5'b10010; // MULH (18 in decimal)
                4: alu_op = 5'b00101; // AND
                5: alu_op = 5'b00110; // OR
                6: alu_op = 5'b00111; // XOR
                7: alu_op = 5'b01000; // NOT
                8: alu_op = 5'b01001; // CLZ
            endcase
            
            test_case(a, b, alu_op);
        end

        $display("ALL TESTS PASSED SUCCESSFULLY!");
        $finish;
    end
endmodule