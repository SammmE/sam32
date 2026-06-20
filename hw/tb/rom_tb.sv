`timescale 1ns / 1ps

module rom_tb();
    logic clk;
    logic [9:0] addr;
    logic [31:0] data;

    logic [31:0] expected_mem [0:1023];
    logic [31:0] exp_data;

    
    int i;
    logic [9:0] rand_addr;

    // Instantiate DUT
    rom_32x1024 dut (
        .clk(clk),
        .addr(addr),
        .data(data)
    );

    initial begin
        clk = 0;
        forever #5 clk = ~clk;
    end

    initial begin
        $readmemh("rom.hex", expected_mem);
    end

    task automatic test_case(input logic [9:0] addr_in, input string desc_in);
        addr = addr_in;
        @(posedge clk);
        #1;
        
        exp_data = expected_mem[addr_in];
        
        if (exp_data === 32'bx) begin
            $warning("rom.hex might be missing or unread! Expected data is X.");
        end

        assert(data === exp_data) else $fatal(1, "Data mismatch for %s at addr %h: Expected %h, got %h", desc_in, addr_in, exp_data, data);
        
        $display("Test passed for %s (Addr: %h, Data: %h)", desc_in, addr_in, data);
    endtask

    initial begin
        $display("Starting ROM Testbench...");
        
        // Wait a bit
        #20; 

        test_case(10'd0, "Address 0 (Base)");
        test_case(10'd1, "Address 1");
        test_case(10'd512, "Address 512 (Mid)");
        test_case(10'd1023, "Address 1023 (Max)");

        $display("Starting 100 Randomized Tests...");
        
        for (i = 0; i < 100; i = i + 1) begin
            rand_addr = $urandom_range(0, 1023);
            test_case(rand_addr, "Random Address");
        end

        $display("ALL TESTS PASSED SUCCESSFULLY!");
        $finish;
    end
endmodule