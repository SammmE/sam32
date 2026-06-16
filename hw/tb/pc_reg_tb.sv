`timescale 1ns / 1ps

module pc_reg_tb();
    logic clk;
    logic rst;
    logic en;
    logic src;
    logic [31:0] d_in;
    logic [31:0] q_out;

    pc_reg uut (
        .clk(clk),
        .rst(rst),
        .en(en),
        .src(src),
        .d_in(d_in),
        .q_out(q_out)
    );

    initial begin
        clk = 0;
        forever #5 clk = ~clk; 
    end

    initial begin
        rst = 1; en = 0; src = 0; d_in = 32'h0;
        #12; 
        
        if (q_out !== 32'h0) $display("FAIL: Reset failed! Expected 0, got %h", q_out);
        else $display("PASS: Asynchronous Reset works.");

        rst = 0;
        @(posedge clk);

        en = 1; src = 0;
        @(posedge clk); // PC should be 4
        if (q_out !== 32'h4) $display("FAIL: PC+4 failed! Got %h", q_out);
        else $display("PASS: PC+4 works (q_out=4).");
        
        @(posedge clk); // PC should be 8
        if (q_out !== 32'h8) $display("FAIL: PC+4 increment failed! Got %h", q_out);
        else $display("PASS: PC+4 works (q_out=8).");

        en = 0; 
        @(posedge clk); 
        if (q_out !== 32'h8) $display("FAIL: Hold state failed! Got %h", q_out);
        else $display("PASS: Hold state works (PC stayed at 8).");

        en = 1; src = 1; d_in = 32'hDEADBEEF;
        @(posedge clk);
        if (q_out !== 32'hDEADBEEF) $display("FAIL: Branch load failed! Got %h", q_out);
        else $display("PASS: Branch/Jump load works.");

        #20;
        $display("Simulation Complete");
        $finish;
    end
endmodule