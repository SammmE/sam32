module reg_file(
    input logic clk,
    input logic we,
    input logic [31:0] wd,

    input logic [4:0] rs1,
    input logic [4:0] rs2,
    input logic [4:0] rd,

    output logic [31:0] rd1,
    output logic [31:0] rd2
);

    logic [31:0] regs [32];

    assign rd1 = (rs1 == 5'd0) ? 32'b0 : regs[rs1]; // x0 is alwasy 0
    assign rd2 = (rs2 == 5'd0) ? 32'b0 : regs[rs2]; // same here ^

    always_ff @(posedge clk) begin
        if (we && rd != 5'd0) begin // don't write to x0
            regs[rd] <= wd;
        end
    end

endmodule