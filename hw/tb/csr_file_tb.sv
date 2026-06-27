`timescale 1ns / 1ps

module csr_file_tb();
    logic clk;
    logic we;
    logic [31:0] wd;
    logic [4:0] csr_addr;
    logic trap;
    logic reti;
    logic [31:0] pc_in;
    logic [31:0] cause_in;
    logic [31:0] csr_rd;
    
    logic [31:0] STATUS;
    logic [31:0] EPC;
    logic [31:0] CAUSE;
    logic [31:0] TVEC;
    logic [31:0] GIE;
    logic [31:0] MTIME;
    logic [31:0] MTIMECMP;
    logic [31:0] BOOT;
    logic [31:0] UMEM_BASE;
    logic [31:0] UMEM_LIMIT;
    logic [31:0] ESTATUS;

    csr_file dut (
        .clk(clk),
        .we(we),
        .wd(wd),
        .csr_addr(csr_addr),
        .trap(trap),
        .reti(reti),
        .pc_in(pc_in),
        .cause_in(cause_in),
        .csr_rd(csr_rd),
        .STATUS(STATUS),
        .EPC(EPC),
        .CAUSE(CAUSE),
        .TVEC(TVEC),
        .GIE(GIE),
        .MTIME(MTIME),
        .MTIMECMP(MTIMECMP),
        .BOOT(BOOT),
        .UMEM_BASE(UMEM_BASE),
        .UMEM_LIMIT(UMEM_LIMIT),
        .ESTATUS(ESTATUS)
    );

    initial begin
        clk = 0;
        forever #5 clk = ~clk; // 100MHz clock
    end

    // Initialize the DUT registers since it lacks a reset
    // Otherwise MTIME starts at 'x' and increments to 'x'
    initial begin
        for (int i=0; i<11; i++) dut.csrs[i] = 32'b0;
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
        logic [31:0] mtime_val1, mtime_val2;
        we = 0;
        wd = 0;
        csr_addr = 0;
        trap = 0;
        reti = 0;
        pc_in = 0;
        cause_in = 0;
        
        $display("Starting CSR Testbench...");
        
        // Wait a few cycles to let MTIME increment
        repeat(5) @(posedge clk);
        
        $display("Testing MTIME increment...");
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
        write_csr(3, 32'h0000_1000); // TVEC
        write_csr(4, 32'h0000_0001); // GIE
        write_csr(6, 32'h0000_0100); // MTIMECMP
        write_csr(7, 32'h0000_0001); // BOOT
        write_csr(8, 32'h4000_0000); // UMEM_BASE
        write_csr(9, 32'h0000_2000); // UMEM_LIMIT
        write_csr(10, 32'hFEED_FACE); // ESTATUS
        
        // Let writes settle
        @(posedge clk);
        
        if (STATUS == 32'hAAAA_BBBB) $display("STATUS Write Pass"); else $error("STATUS Write Fail");
        if (EPC == 32'h1111_2222) $display("EPC Write Pass"); else $error("EPC Write Fail");
        if (CAUSE == 32'h8000_0001) $display("CAUSE Write Pass"); else $error("CAUSE Write Fail");
        if (TVEC == 32'h0000_1000) $display("TVEC Write Pass"); else $error("TVEC Write Fail");
        if (GIE == 32'h0000_0001) $display("GIE Write Pass"); else $error("GIE Write Fail");
        if (MTIMECMP == 32'h0000_0100) $display("MTIMECMP Write Pass"); else $error("MTIMECMP Write Fail");
        if (BOOT == 32'h0000_0001) $display("BOOT Write Pass"); else $error("BOOT Write Fail");
        if (UMEM_BASE == 32'h4000_0000) $display("UMEM_BASE Write Pass"); else $error("UMEM_BASE Write Fail");
        if (UMEM_LIMIT == 32'h0000_2000) $display("UMEM_LIMIT Write Pass"); else $error("UMEM_LIMIT Write Fail");
        if (ESTATUS == 32'hFEED_FACE) $display("ESTATUS Write Pass"); else $error("ESTATUS Write Fail");
        
        // Test read port
        csr_addr = 1;
        @(posedge clk);
        if (csr_rd == 32'h1111_2222) $display("CSR Read Port Pass"); else $error("CSR Read Port Fail: %h", csr_rd);
        
        $display("Testing TRAP override...");
        write_csr(0, 32'hAAAA_5555); // Set STATUS to a known value
        @(posedge clk);
        trap = 1;
        pc_in = 32'hCAFE_F00D;
        cause_in = 32'h0000_0007; // Machine Timer Int
        @(posedge clk);
        trap = 0;
        @(posedge clk);
        if (EPC == 32'hCAFE_F00D && CAUSE == 32'h0000_0007 && GIE == 32'b0 && ESTATUS == 32'hAAAA_5555)
            $display("TRAP context save PASS");
        else
            $error("TRAP context save FAIL (EPC: %h, CAUSE: %h, GIE: %h, ESTATUS: %h)", EPC, CAUSE, GIE, ESTATUS);

        $display("Testing RETI restore...");
        // Scramble STATUS to ensure it gets restored properly
        write_csr(0, 32'h0000_0000); 
        @(posedge clk);
        reti = 1;
        @(posedge clk);
        reti = 0;
        @(posedge clk);
        if (GIE == 32'b1 && STATUS == 32'hAAAA_5555)
            $display("RETI restore PASS");
        else
            $error("RETI restore FAIL (GIE: %b, STATUS: %h)", GIE, STATUS);
            
        $display("ALL TESTS COMPLETED!");
        $finish;
    end
endmodule