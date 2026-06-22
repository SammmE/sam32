module csr_file(
    input logic clk,
    input logic we,
    input logic [31:0] wd,

    input logic [4:0] csr_addr,

    output logic [31:0] csr_rd,
    output logic [31:0] STATUS,
    output logic [31:0] EPC,
    output logic [31:0] CAUSE,
    output logic [31:0] TVAL,
    output logic [31:0] GIE,
    output logic [31:0] MTIME,
    output logic [31:0] MTIMECMP
);

    logic [31:0] csrs [7]; // 0: STATUS, 1: EPC, 2: CAUSE, 3: TVAL, 4: GIE, 5: MTIME, 6: MTIMECMP

    assign STATUS = csrs[0];
    assign EPC = csrs[1];
    assign CAUSE = csrs[2];
    assign TVAL = csrs[3];
    assign GIE = csrs[4];
    assign MTIME = csrs[5];
    assign MTIMECMP = csrs[6];
    assign csr_rd = csrs[csr_addr];

    always_ff @(posedge clk) begin
        if (we && csr_addr < 7) begin
            csrs[csr_addr] <= wd;
        end
    end
endmodule