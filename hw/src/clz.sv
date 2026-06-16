module clz_32 (
    input  logic [31:0] data_in,
    output logic [5:0]  lz_count
);

    always_comb begin
        lz_count = 32;
        for (int i = 31; i >= 0; i--) begin
            if (data_in[i]) begin
                lz_count = 31 - i;
                break;
            end
        end
    end

endmodule
