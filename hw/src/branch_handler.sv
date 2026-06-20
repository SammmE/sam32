module branch_handler(
    input logic [4:0] operation,
    input logic z,
    input logic n,
    input logic c,
    input logic v,

    output logic branch
);
    logic [3:0] cond;
    assign cond = operation[3:0];

    always_comb begin
        case (cond)
            4'b0000: branch = 1; // B
            4'b0001: branch = z; // BEQ
            4'b0010: branch = ~z; // BNE
            4'b0011: branch = n != v; // BLT
            4'b0100: branch = n == v; // BGE
            4'b0101: branch = z & n == v; // BLE
            4'b0110: branch = z == 0 & n == v; // BGT
            4'b0111: branch = c; // BCS
            4'b1000: branch = ~c; // BCC
            4'b1001: branch = n; // BMI
            4'b1010: branch = ~n; // BPL
            default: branch = 0; // Invalid condition
        endcase
    end
endmodule