`timescale 1ns / 1ps

module branch_handler_tb();
    logic [4:0] operation;
    logic z, n, c, v;
    logic branch;

    logic exp_branch;
    logic [3:0] cond;

    int i;
    logic rand_z, rand_n, rand_c, rand_v;
    logic [4:0] rand_op;
    string desc;

    branch_handler dut (
        .operation(operation),
        .z(z), .n(n), .c(c), .v(v),
        .branch(branch)
    );

    task automatic test_case(input logic [4:0] op_in, input logic z_in, input logic n_in, input logic c_in, input logic v_in, input string desc_in);
        operation = op_in;
        z = z_in;
        n = n_in;
        c = c_in;
        v = v_in;
        #10;
        
        cond = op_in[3:0];
        
        case (cond)
            4'b0000: exp_branch = 1'b1; // B
            4'b0001: exp_branch = z; // BEQ
            4'b0010: exp_branch = ~z; // BNE
            4'b0011: exp_branch = (n != v); // BLT
            4'b0100: exp_branch = (n == v); // BGE
            4'b0101: exp_branch = z | (n != v); // BLE
            4'b0110: exp_branch = (~z) & (n == v);   // BGT
            4'b0111: exp_branch = c; // BCS
            4'b1000: exp_branch = ~c; // BCC
            4'b1001: exp_branch = n;  // BMI
            4'b1010: exp_branch = ~n; // BPL
            default: exp_branch = 1'b0;
        endcase
        
        assert(branch === exp_branch) else $fatal(1, "Branch mismatch for %s (cond=%b, z=%b, n=%b, c=%b, v=%b): Expected %b, got %b", desc_in, cond, z, n, c, v, exp_branch, branch);
        
        $display("Test passed for %s (cond=%b, flags z=%b n=%b c=%b v=%b)", desc_in, cond, z, n, c, v);
    endtask

    initial begin
        $display("Starting Branch Handler Testbench...");
        
        // B (Unconditional)
        test_case(5'b00000, 0, 0, 0, 0, "B (Uncond)");
        
        // BEQ / BNE
        test_case(5'b00001, 1, 0, 0, 0, "BEQ (Z=1)");
        test_case(5'b00001, 0, 1, 0, 0, "BEQ (Z=0)");
        test_case(5'b00010, 0, 0, 0, 0, "BNE (Z=0)");
        test_case(5'b00010, 1, 0, 0, 0, "BNE (Z=1)");
        
        // BLT / BGE
        test_case(5'b00011, 0, 1, 0, 0, "BLT (N=1, V=0)");
        test_case(5'b00011, 0, 0, 0, 0, "BLT (N=0, V=0)");
        test_case(5'b00100, 0, 1, 0, 1, "BGE (N=1, V=1)");
        test_case(5'b00100, 0, 1, 0, 0, "BGE (N=1, V=0)");
        
        // BLE / BGT
        test_case(5'b00101, 1, 0, 0, 0, "BLE (Z=1)");
        test_case(5'b00101, 0, 1, 0, 0, "BLE (N!=V)");
        test_case(5'b00101, 0, 0, 0, 0, "BLE (False)");
        
        test_case(5'b00110, 0, 1, 0, 1, "BGT (True)");
        test_case(5'b00110, 1, 1, 0, 1, "BGT (Z=1 False)");
        test_case(5'b00110, 0, 1, 0, 0, "BGT (N!=V False)");
        
        // BCS / BCC
        test_case(5'b00111, 0, 0, 1, 0, "BCS (C=1)");
        test_case(5'b00111, 0, 0, 0, 0, "BCS (C=0)");
        test_case(5'b01000, 0, 0, 0, 0, "BCC (C=0)");
        test_case(5'b01000, 0, 0, 1, 0, "BCC (C=1)");
        
        // BMI / BPL
        test_case(5'b01001, 0, 1, 0, 0, "BMI (N=1)");
        test_case(5'b01001, 0, 0, 0, 0, "BMI (N=0)");
        test_case(5'b01010, 0, 0, 0, 0, "BPL (N=0)");
        test_case(5'b01010, 0, 1, 0, 0, "BPL (N=1)");

        $display("Starting 1000 Randomized Tests...");
        
        for (i = 0; i < 1000; i = i + 1) begin
            rand_op = $urandom;
            rand_z = $urandom_range(0, 1);
            rand_n = $urandom_range(0, 1);
            rand_c = $urandom_range(0, 1);
            rand_v = $urandom_range(0, 1);
            
            test_case(rand_op, rand_z, rand_n, rand_c, rand_v, "Random");
        end

        $display("ALL TESTS PASSED SUCCESSFULLY!");
        $finish;
    end
endmodule