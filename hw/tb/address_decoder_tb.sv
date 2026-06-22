`timescale 1ns / 1ps

module address_decoder_tb();
    logic [31:0] address;
    logic we, re;
    logic ram_we, uart_tx_we, uart_rx_we, led_we, switches_re;

    logic exp_ram_we, exp_uart_tx_we, exp_uart_rx_we, exp_led_we, exp_switches_re;

    int i;
    logic [31:0] rand_addr;
    logic rand_we, rand_re;
    string desc;

    address_decoder dut (
        .address(address),
        .we(we),
        .re(re),
        .ram_we(ram_we),
        .uart_tx_we(uart_tx_we),
        .uart_rx_we(uart_rx_we),
        .led_we(led_we),
        .switches_re(switches_re)
    );

    task automatic test_case(input logic [31:0] addr_in, input logic we_in, input logic re_in, input string desc_in);
        address = addr_in;
        we = we_in;
        re = re_in;
        #10;
        
        exp_ram_we = 0;
        exp_uart_tx_we = 0;
        exp_uart_rx_we = 0;
        exp_led_we = 0;
        exp_switches_re = 0;
        
        if (address[31] == 1'b0) begin
            exp_ram_we = we;
        end else begin
            case (address[15:0])
                16'h0000: exp_uart_tx_we = we;
                16'h0004: exp_uart_rx_we = re;
                16'h0100: exp_led_we = we;
                16'h0104: exp_switches_re = re;
                default: ; // all remain 0 for invalid addresses
            endcase
        end
        
        // Assertions
        assert(ram_we === exp_ram_we) else $fatal(1, "ram_we mismatch for %s: Expected %b, got %b", desc_in, exp_ram_we, ram_we);
        assert(uart_tx_we === exp_uart_tx_we) else $fatal(1, "uart_tx_we mismatch for %s: Expected %b, got %b", desc_in, exp_uart_tx_we, uart_tx_we);
        assert(uart_rx_we === exp_uart_rx_we) else $fatal(1, "uart_rx_we mismatch for %s: Expected %b, got %b", desc_in, exp_uart_rx_we, uart_rx_we);
        assert(led_we === exp_led_we) else $fatal(1, "led_we mismatch for %s: Expected %b, got %b", desc_in, exp_led_we, led_we);
        assert(switches_re === exp_switches_re) else $fatal(1, "switches_re mismatch for %s: Expected %b, got %b", desc_in, exp_switches_re, switches_re);
        
        $display("Test passed for %s (Addr: %h, WE: %b, RE: %b)", desc_in, addr_in, we_in, re_in);
    endtask

    initial begin
        $display("Starting Address Decoder Testbench...");
        
        
        // RAM (MSB == 0)
        test_case(32'h0000_0000, 1, 0, "RAM Write (Base)");
        test_case(32'h0000_1000, 0, 1, "RAM Read (Mid)");
        test_case(32'h7FFF_FFFF, 1, 1, "RAM Max Address");
        
        // Peripheral (MSB == 1) - Valid Addresses
        test_case(32'h8000_0000, 1, 0, "UART TX Write");
        test_case(32'h8000_0004, 0, 1, "UART RX Read");
        test_case(32'h8000_0100, 1, 0, "LED Write");
        test_case(32'h8000_0104, 0, 1, "Switches Read");
        
        // Peripheral - Negative Tests (Wrong WE/RE or Invalid Addresses)
        test_case(32'h8000_0000, 0, 1, "UART TX Read (Should be ignored)");
        test_case(32'h8000_0004, 1, 0, "UART RX Write (Should be ignored)");
        test_case(32'h8000_0008, 1, 1, "Invalid Peripheral Address (0x0008)");
        test_case(32'h8000_FFFF, 1, 1, "Invalid High Peripheral Address");
        test_case(32'hFFFF_FFFF, 1, 1, "Max Address Space (Invalid)");

        $display("Starting 1000 Randomized Tests...");
        
        for (i = 0; i < 1000; i = i + 1) begin
            rand_addr = $urandom;
            rand_we = $urandom_range(0, 1);
            rand_re = $urandom_range(0, 1);
            
            test_case(rand_addr, rand_we, rand_re, "Random");
        end

        $display("ALL TESTS PASSED SUCCESSFULLY!");
        $finish;
    end
endmodule