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

    logic [31:0] shadow_csrs [7];

    int i;
    logic [4:0] rand_addr;
    logic [31:0] rand_data;
    string desc;

    // Clock (10ns period = 100MHz)
    initial begin
        clk = 0;
        forever #5 clk = ~clk;
    end

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

    task automatic test_write(input logic [4:0] addr_in, input logic [31:0] data_in, input string desc_in);
        csr_addr = addr_in;
        wd = data_in;
        we = 1;
        @(posedge clk);
        #1;
        we = 0;
        
        shadow_csrs[addr_in] = data_in;
        
        assert(csr_rd === data_in) else $fatal(1, "csr_rd mismatch for %s: Expected %h, got %h", desc_in, data_in, csr_rd);
        
        case (addr_in)
            0: assert(STATUS === data_in) else $fatal(1, "STATUS mismatch for %s", desc_in);
            1: assert(EPC === data_in) else $fatal(1, "EPC mismatch for %s", desc_in);
            2: assert(CAUSE === data_in) else $fatal(1, "CAUSE mismatch for %s", desc_in);
            3: assert(TVAL === data_in) else $fatal(1, "TVAL mismatch for %s", desc_in);
            4: assert(GIE === data_in) else $fatal(1, "GIE mismatch for %s", desc_in);
            5: assert(MTIME === data_in) else $fatal(1, "MTIME mismatch for %s", desc_in);
            6: assert(MTIMECMP === data_in) else $fatal(1, "MTIMECMP mismatch for %s", desc_in);
            default: ;
        endcase
        
        $display("Write passed for %s (Addr: %d, Data: %h)", desc_in, addr_in, data_in);
    endtask

    task automatic test_read(input logic [4:0] addr_in, input string desc_in);
        csr_addr = addr_in;
        we = 0;
        #10; // Combinational read
        
        assert(csr_rd === shadow_csrs[addr_in]) else $fatal(1, "csr_rd read mismatch for %s: Expected %h, got %h", desc_in, shadow_csrs[addr_in], csr_rd);
        
        $display("Read passed for %s (Addr: %d, Data: %h)", desc_in, addr_in, csr_rd);
    endtask

    initial begin
        $display("Starting CSR File Testbench...");
        
        for (i = 0; i < 7; i = i + 1) begin
            shadow_csrs[i] = 32'h0;
        end
        
        // Reset
        we = 0;
        wd = 0;
        csr_addr = 0;
        #20;

        test_write(0, 32'hDEAD_BEEF, "STATUS");
        test_read(0, "STATUS");
        
        test_write(1, 32'h1111_1111, "EPC");
        test_read(1, "EPC");
        
        test_write(2, 32'h2222_2222, "CAUSE");
        test_read(2, "CAUSE");
        
        test_write(3, 32'h3333_3333, "TVAL");
        test_read(3, "TVAL");
        
        test_write(4, 32'h4444_4444, "GIE");
        test_read(4, "GIE");
        
        test_write(5, 32'h5555_5555, "MTIME");
        test_read(5, "MTIME");
        
        test_write(6, 32'h6666_6666, "MTIMECMP");
        test_read(6, "MTIMECMP");

        $display("Starting 1000 Randomized Tests...");
        
        for (i = 0; i < 1000; i = i + 1) begin
            rand_addr = $urandom_range(0, 6);
            rand_data = $urandom;
            desc = "Random";
            
            test_write(rand_addr, rand_data, desc);
            test_read(rand_addr, desc);
        end

        $display("ALL TESTS PASSED SUCCESSFULLY!");
        $finish;
    end
endmodule