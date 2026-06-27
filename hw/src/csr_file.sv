module csr_file(
    input logic clk,
    input logic we,
    input logic [31:0] wd,

    input logic [4:0] csr_addr,
    input logic trap,
    input logic reti,
    input logic [31:0] pc_in,
    input logic [31:0] cause_in,

    output logic [31:0] csr_rd,
    output logic [31:0] STATUS,
    output logic [31:0] EPC,
    output logic [31:0] CAUSE,
    output logic [31:0] TVEC,
    output logic [31:0] GIE,
    output logic [31:0] MTIME,
    output logic [31:0] MTIMECMP,
    output logic [31:0] BOOT,
    output logic [31:0] UMEM_BASE,
    output logic [31:0] UMEM_LIMIT,
    output logic [31:0] ESTATUS
);

    logic [31:0] csrs [11]; // 0: STATUS, 1: EPC, 2: CAUSE, 3: TVEC, 4: GIE, 5: MTIME, 6: MTIMECMP, 7: BOOT, 8: UMEM_BASE, 9: UMEM_LIMIT, 10: ESTATUS

    assign STATUS = csrs[0];
    assign EPC = csrs[1];
    assign CAUSE = csrs[2];
    assign TVEC = csrs[3];
    assign GIE = csrs[4];
    assign MTIME = csrs[5];
    assign MTIMECMP = csrs[6];
    assign BOOT = csrs[7];
    assign UMEM_BASE = csrs[8];
    assign UMEM_LIMIT = csrs[9];
    assign ESTATUS = csrs[10];
    assign csr_rd = csrs[csr_addr];

    always_ff @(posedge clk) begin
        csrs[5] <= MTIME + 1;
        if (trap) begin
            csrs[1] <= pc_in;
            csrs[2] <= cause_in;
            csrs[4] <= 32'b0;
            csrs[10] <= csrs[0]; // Backup STATUS to ESTATUS
        end else if (reti) begin
            csrs[4] <= 32'b1;
            csrs[0] <= csrs[10]; // Restore STATUS from ESTATUS
        end else if (we && csr_addr < 11) begin
            csrs[csr_addr] <= wd;
        end
    end
endmodule