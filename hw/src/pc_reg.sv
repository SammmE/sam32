`timescale 1ns / 1ps

module pc_reg(
    input logic clk,
    input logic rst,
    input logic en,
    input logic src, // 0: PC+4, 1: d_in
    input logic [31:0] d_in,
    output logic [31:0] q_out
);

    always_ff @(posedge clk or posedge rst) begin
        if (rst) begin
            q_out <= 32'b0;
        end else if (en) begin
            if (src == 1'b0) begin
                q_out <= q_out + 4; // PC + 4
            end else begin
                q_out <= d_in; // d_in
            end
        end
    end

endmodule