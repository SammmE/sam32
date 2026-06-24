module timer(
    input logic clk,
    input logic [31:0] mtime,
    input logic [31:0] mtimecmp,
    output logic timer_interrupt
);

    always_ff @(posedge clk) begin
        if (mtime > mtimecmp) begin
            timer_interrupt <= 1;
        end else begin
            timer_interrupt <= 0;
        end
    end
endmodule