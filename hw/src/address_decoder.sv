`timescale 1ns / 1ps

module address_decoder(
    input logic [31:0] address,
    input logic we,
    input logic re,
    input logic boot, // in boot mode, the address decoder will only read from the ROM
    input logic [31:0] umem_base,
    input logic [31:0] umem_limit,
    
    output logic [31:0] physical_addr,
    output logic mem_fault,
    output logic ram_we,
    output logic ram_re,
    output logic rom_re,
    output logic uart_tx_we,
    output logic uart_rx_we,
    output logic led_we,
    output logic switches_re
);

always_comb begin
    physical_addr = address;
    mem_fault = 1'b0;
    ram_we = 1'b0;
    ram_re = 1'b0;
    rom_re = 1'b0;
    uart_tx_we = 1'b0;
    uart_rx_we = 1'b0;
    led_we = 1'b0;
    switches_re = 1'b0;

    if (boot) begin
        rom_re = re;
    end else begin
        if (address[31] == 1'b0) begin
            if (address < umem_limit) begin
                ram_we = we;
                ram_re = re;
                physical_addr = address + umem_base;
            end else begin
                mem_fault = 1'b1;
            end
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
end

endmodule