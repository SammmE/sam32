`timescale 1ns / 1ps

module pc_reg_tb();
    logic clk;
    logic rst;
    logic en;
    logic trap;
    logic reti;
    logic [31:0] tvec;
    logic [31:0] epc;
    logic src;
    logic [31:0] d_in;
    logic [31:0] q_out;

    pc_reg uut (
        .clk(clk),
        .rst(rst),
        .en(en),
        .trap(trap),
        .reti(reti),
        .tvec(tvec),
        .epc(epc),
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
        trap = 0; reti = 0; tvec = 32'h0; epc = 32'h0;
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

        // Trap test
        trap = 1; tvec = 32'h8000_1000; src = 0;
        @(posedge clk);
        if (q_out !== 32'h8000_1000) $display("FAIL: Trap failed! Got %h", q_out);
        else $display("PASS: Trap jump works.");
        
        trap = 0;
        @(posedge clk); // En is 1, src is 0, so should PC+4
        if (q_out !== 32'h8000_1004) $display("FAIL: Post-Trap PC+4 failed! Got %h", q_out);
        
        // Reti test
        reti = 1; epc = 32'hDEADBEF0;
        @(posedge clk);
        if (q_out !== 32'hDEADBEF0) $display("FAIL: Reti failed! Got %h", q_out);
        else $display("PASS: Reti restore works.");
        
        reti = 0;
        @(posedge clk);

        #20;
        $display("Simulation Complete");
        $finish;
    end
endmodule