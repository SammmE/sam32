module address_decoder(
    input logic [31:0] address,
    input logic we,
    input logic re,

    output logic ram_we,
    output logic uart_tx_we,
    output logic uart_rx_we,
    output logic led_we,
    output logic switches_re
);

always_comb begin
    ram_we  = 1'b0;
    uart_tx_we = 1'b0;
    uart_rx_we = 1'b0;
    led_we = 1'b0;
    switches_re = 1'b0;

    if (address[31] == 1'b0) begin
        ram_we = we;
    end else begin
        case (address[15:0])
            16'h0000: uart_tx_we  = we; // 0x8000_0000 -> UART TX
            16'h0004: uart_rx_we = re; // 0x8000_0004 -> UART RX
            16'h0100: led_we = we; // 0x8000_0100 -> LED
            16'h0104: switches_re = re; // 0x8000_0104 -> Switches
            default:;
        endcase
    end
end

endmodule