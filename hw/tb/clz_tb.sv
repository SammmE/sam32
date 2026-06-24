`timescale 1ns / 1ps

module clz_tb();
    logic [31:0] data_in;
    logic [5:0]  lz_count;
    logic [5:0]  exp_lz_count;

    clz_32 dut (
        .data_in(data_in),
        .lz_count(lz_count)
    );

    task automatic test_case(input logic [31:0] d_in, input logic [5:0] expected, input string desc);
        data_in = d_in;
        exp_lz_count = expected;
        #10;
        assert(lz_count === exp_lz_count) else $fatal(1, "%s failed: expected %0d, got %0d for input %h", desc, exp_lz_count, lz_count, data_in);
        $display("%s passed (Input: %h, CLZ: %0d)", desc, data_in, lz_count);
    endtask

    initial begin
        $display("Starting CLZ Testbench...");

        test_case(32'h0000_0000, 32, "All zeros");
        test_case(32'hFFFF_FFFF, 0, "All ones");
        test_case(32'h8000_0000, 0, "Only MSB set");
        test_case(32'h4000_0000, 1, "Bit 30 set");
        test_case(32'h0000_0001, 31, "Only LSB set");
        test_case(32'h000F_FFFF, 12, "Lower 20 bits set");
        
        $display("Starting Randomized Tests...");
        for (int i = 0; i < 100; i++) begin
            logic [31:0] rand_val = $urandom;
            logic [5:0]  calc_clz = 32;
            for (int j = 31; j >= 0; j--) begin
                if (rand_val[j]) begin
                    calc_clz = 31 - j;
                    break;
                end
            end
            test_case(rand_val, calc_clz, "Random Test");
        end

        $display("ALL TESTS PASSED!");
        $finish;
    end
endmodule
