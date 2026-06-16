`timescale 1ns / 1ps

module reg_32_tb();
    logic clk;
    logic rst;
    logic en;
    logic [31:0] d_in;
    logic [31:0] q_out;

    reg_32 uut (
        .clk(clk),
        .rst(rst),
        .en(en),
        .d_in(d_in),
        .q_out(q_out)
    );

    initial begin
        clk = 0;
        forever #5 clk = ~clk;
    end

    initial begin
        rst = 1; en = 0; d_in = 32'h0;
        
        #12; 
        
        if (q_out !== 32'h0) $display("FAIL: Initial Reset failed! Expected 0, got %h", q_out);
        else $display("PASS: Initial Reset works (q_out = 0).");

        rst = 0;
        @(posedge clk); // Wait for the next clock edge

        en = 1; 
        d_in = 32'hDEADBEEF;
        @(posedge clk);
        
        if (q_out !== 32'hDEADBEEF) $display("FAIL: Load failed! Expected DEADBEEF, got %h", q_out);
        else $display("PASS: Load works (q_out = DEADBEEF).");

        en = 0; 
        d_in = 32'h12345678;
        @(posedge clk); 
        
        if (q_out !== 32'hDEADBEEF) $display("FAIL: Hold state failed! Expected DEADBEEF, got %h", q_out);
        else $display("PASS: Hold state works (q_out stayed DEADBEEF despite input change).");

        en = 1;
        @(posedge clk);
        
        if (q_out !== 32'h12345678) $display("FAIL: Second load failed! Expected 12345678, got %h", q_out);
        else $display("PASS: Second load works (q_out = 12345678).");
        
        @(negedge clk);
        rst = 1;
        #2;
        
        if (q_out !== 32'h0) $display("FAIL: Async reset failed! Expected 0 immediately, got %h", q_out);
        else $display("PASS: Asynchronous reset works immediately (didn't wait for clock edge).");

        #20;
        $display("Simulation Complete");
        $finish;
    end
endmodule