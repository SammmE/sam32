`timescale 1ns / 1ps

module csr_file_tb();
    logic clk;
    logic we;
    logic [31:0] wd;
    logic [4:0] csr_addr;
    logic [31:0] csr_rd;
    
    logic [31:0] STATUS;
    logic [31:0] EPC;
    logic [31:0] CAUSE;
    logic [31:0] TVAL;
    logic [31:0] GIE;
    logic [31:0] MTIME;
    logic [31:0] MTIMECMP;

    csr_file dut (
        .clk(clk),
        .we(we),
        .wd(wd),
        .csr_addr(csr_addr),
        .csr_rd(csr_rd),
        .STATUS(STATUS),
        .EPC(EPC),
        .CAUSE(CAUSE),
        .TVAL(TVAL),
        .GIE(GIE),
        .MTIME(MTIME),
        .MTIMECMP(MTIMECMP)
    );

    initial begin
        clk = 0;
        forever #5 clk = ~clk; // 100MHz clock
    end

    // Initialize the DUT registers since it lacks a reset
    // Otherwise MTIME starts at 'x' and increments to 'x'
    initial begin
        for (int i=0; i<7; i++) dut.csrs[i] = 32'b0;
    end

    task write_csr(input [4:0] addr, input [31:0] data);
        @(posedge clk);
        we = 1;
        csr_addr = addr;
        wd = data;
        @(posedge clk);
        we = 0;
    endtask

    initial begin
        we = 0;
        wd = 0;
        csr_addr = 0;
        
        $display("Starting CSR Testbench...");
        
        // Wait a few cycles to let MTIME increment
        repeat(5) @(posedge clk);
        
        $display("Testing MTIME increment...");
        logic [31:0] mtime_val1, mtime_val2;
        mtime_val1 = MTIME;
        repeat(10) @(posedge clk);
        mtime_val2 = MTIME;
        
        if (mtime_val2 == mtime_val1 + 10)
            $display("MTIME is incrementing correctly: %0d -> %0d", mtime_val1, mtime_val2);
        else
            $error("MTIME failed to increment correctly: %0d -> %0d", mtime_val1, mtime_val2);
            
        // Test Writing to CSRs
        $display("Testing CSR Writes...");
        write_csr(0, 32'hAAAA_BBBB); // STATUS
        write_csr(1, 32'h1111_2222); // EPC
        write_csr(2, 32'h8000_0001); // CAUSE
        write_csr(3, 32'h0000_1000); // TVAL
        write_csr(4, 32'h0000_0001); // GIE
        write_csr(6, 32'h0000_0100); // MTIMECMP
        
        // Let writes settle
        @(posedge clk);
        
        if (STATUS == 32'hAAAA_BBBB) $display("STATUS Write Pass"); else $error("STATUS Write Fail");
        if (EPC == 32'h1111_2222) $display("EPC Write Pass"); else $error("EPC Write Fail");
        if (CAUSE == 32'h8000_0001) $display("CAUSE Write Pass"); else $error("CAUSE Write Fail");
        if (TVAL == 32'h0000_1000) $display("TVAL Write Pass"); else $error("TVAL Write Fail");
        if (GIE == 32'h0000_0001) $display("GIE Write Pass"); else $error("GIE Write Fail");
        if (MTIMECMP == 32'h0000_0100) $display("MTIMECMP Write Pass"); else $error("MTIMECMP Write Fail");
        
        // Test read port
        csr_addr = 1;
        @(posedge clk);
        if (csr_rd == 32'h1111_2222) $display("CSR Read Port Pass"); else $error("CSR Read Port Fail: %h", csr_rd);
        
        $display("Testing CSR write behavior on MTIME...");
        write_csr(5, 32'h0000_0000); // Write 0 to MTIME
        @(posedge clk);
        $display("After writing 0, MTIME is: %0d (Note: Your non-blocking assignment ordering overrides writes to MTIME!)", MTIME);
        
        $display("ALL TESTS COMPLETED!");
        $finish;
    end
endmodule