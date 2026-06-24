`timescale 1ns / 1ps

module timer_tb();
    logic clk;
    logic [31:0] mtime;
    logic [31:0] mtimecmp;
    logic timer_interrupt;

    timer dut (
        .clk(clk),
        .mtime(mtime),
        .mtimecmp(mtimecmp),
        .timer_interrupt(timer_interrupt)
    );

    initial begin
        clk = 0;
        forever #5 clk = ~clk; // 100MHz clock
    end

    initial begin
        $display("Starting Timer Testbench...");
        
        mtime = 32'd0;
        mtimecmp = 32'd10;
        
        // Wait for a few clock cycles
        repeat(5) @(posedge clk);
        
        // mtime < mtimecmp
        assert(timer_interrupt === 1'b0) else $fatal(1, "Interrupt should be 0 when mtime < mtimecmp");
        $display("Check 1 passed: mtime < mtimecmp (Int=0)");
        
        mtime = 32'd10;
        @(posedge clk);
        @(posedge clk);
        // mtime == mtimecmp
        assert(timer_interrupt === 1'b0) else $fatal(1, "Interrupt should be 0 when mtime == mtimecmp");
        $display("Check 2 passed: mtime == mtimecmp (Int=0)");
        
        mtime = 32'd11;
        @(posedge clk);
        @(posedge clk);
        // mtime > mtimecmp
        assert(timer_interrupt === 1'b1) else $fatal(1, "Interrupt should be 1 when mtime > mtimecmp");
        $display("Check 3 passed: mtime > mtimecmp (Int=1)");
        
        // Reset condition
        mtime = 32'd5;
        @(posedge clk);
        @(posedge clk);
        assert(timer_interrupt === 1'b0) else $fatal(1, "Interrupt should drop to 0 when mtime resets below mtimecmp");
        $display("Check 4 passed: mtime drops below mtimecmp (Int=0)");
        
        // Test edge case near max values
        mtimecmp = 32'hFFFF_FFFE;
        mtime = 32'hFFFF_FFFE;
        @(posedge clk);
        @(posedge clk);
        assert(timer_interrupt === 1'b0) else $fatal(1, "Edge case equal failed");
        
        mtime = 32'hFFFF_FFFF;
        @(posedge clk);
        @(posedge clk);
        assert(timer_interrupt === 1'b1) else $fatal(1, "Edge case greater failed");
        $display("Check 5 passed: Large values boundary conditions");

        $display("ALL TESTS PASSED!");
        $finish;
    end
endmodule
