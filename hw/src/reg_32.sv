module reg_32(
    input logic clk,
    input logic rst,
    input logic en,
    input logic [31:0] d_in,
    output logic [31:0] q_out
);

    always_ff @(posedge clk or posedge rst) begin
        if (rst) begin
            q_out <= 32'b0;
        end else if (en) begin
            q_out <= d_in;
        end
    end
endmodule