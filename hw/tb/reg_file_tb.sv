`timescale 1ns / 1ps

module reg_file_tb();
    logic clk, we;
    logic [31:0] wd;
    logic [4:0] rs1, rs2, rd;
    logic [31:0] rd1, rd2;

    reg_file uut (.*);

    initial begin
        clk = 0;
        forever #5 clk = ~clk;
    end

    initial begin
        we = 0; wd = 32'h0; rs1 = 0; rs2 = 0; rd = 0;
        
        #10;

        rs1 = 5'd0; rs2 = 5'd0;
        if (rd1 !== 32'h0 || rd2 !== 32'h0) $display("FAIL: Read from x0 failed.");
        else $display("PASS: Reading x0 outputs 0.");

        we = 1; rd = 5'd0; wd = 32'hDEADBEEF;
        @(posedge clk);
        #1;
        if (rd1 !== 32'h0) $display("FAIL: Wrote to x0! Got %h", rd1);
        else $display("PASS: Write to x0 correctly ignored.");

        we = 1; rd = 5'd5; wd = 32'hCAFEBABE;
        @(posedge clk);
        
        we = 0;
        rs1 = 5'd5; rs2 = 5'd5; 
        #1;
        if (rd1 !== 32'hCAFEBABE || rd2 !== 32'hCAFEBABE) 
            $display("FAIL: Read from r5 failed. Got %h", rd1);
        else 
            $display("PASS: Read from r5 works. (rd1 = %h)", rd1);

        we = 0; wd = 32'h11111111; rd = 5'd5;
        @(posedge clk);
        #1;
        if (rd1 !== 32'hCAFEBABE) $display("FAIL: Register changed while we=0!");
        else $display("PASS: Register held value while we=0.");

        #20;
        $display("SSimulation Complete");
        $finish;
    end
endmodule