module rom_32x1024(
    input logic clk,
    input logic [9:0] addr,
    output logic [31:0] data
);

logic [31:0] rom_mem [0:1023];

initial begin
    $readmemh("rom.hex", rom_mem);
end

always_ff @(posedge clk) begin
    data <= rom_mem[addr];
end
endmodule