`timescale 1ns / 1ps

module address_decoder_tb();
    logic [31:0] address;
    logic we, re, boot;
    logic [31:0] umem_base;
    logic [31:0] umem_limit;
    
    logic [31:0] physical_addr;
    logic mem_fault;
    logic ram_we, ram_re, rom_re, uart_tx_we, uart_rx_we, led_we, switches_re;

    logic [31:0] exp_physical_addr;
    logic exp_mem_fault;
    logic exp_ram_we, exp_ram_re, exp_rom_re, exp_uart_tx_we, exp_uart_rx_we, exp_led_we, exp_switches_re;

    int i;
    logic [31:0] rand_addr;
    logic rand_we, rand_re, rand_boot;
    logic [31:0] rand_base;
    logic [31:0] rand_limit;
    string desc;

    address_decoder dut (
        .address(address),
        .we(we),
        .re(re),
        .boot(boot),
        .umem_base(umem_base),
        .umem_limit(umem_limit),
        .physical_addr(physical_addr),
        .mem_fault(mem_fault),
        .ram_we(ram_we),
        .ram_re(ram_re),
        .rom_re(rom_re),
        .uart_tx_we(uart_tx_we),
        .uart_rx_we(uart_rx_we),
        .led_we(led_we),
        .switches_re(switches_re)
    );

    task automatic test_case(input logic [31:0] addr_in, input logic we_in, input logic re_in, input logic boot_in, 
                             input logic [31:0] base_in, input logic [31:0] limit_in, input string desc_in);
        address = addr_in;
        we = we_in;
        re = re_in;
        boot = boot_in;
        umem_base = base_in;
        umem_limit = limit_in;
        #10;
        
        exp_ram_we = 0;
        exp_ram_re = 0;
        exp_rom_re = 0;
        exp_uart_tx_we = 0;
        exp_uart_rx_we = 0;
        exp_led_we = 0;
        exp_switches_re = 0;
        exp_physical_addr = addr_in;
        exp_mem_fault = 0;
        
        if (boot) begin
            exp_rom_re = re;
        end else begin
            if (address[31] == 1'b0) begin
                if (address < umem_limit) begin
                    exp_ram_we = we;
                    exp_ram_re = re;
                    exp_physical_addr = address + umem_base;
                end else begin
                    exp_mem_fault = 1;
                end
            end else begin
                case (address[15:0])
                    16'h0000: exp_uart_tx_we = we;
                    16'h0004: exp_uart_rx_we = re;
                    16'h0100: exp_led_we = we;
                    16'h0104: exp_switches_re = re;
                    default: ; // all remain 0 for invalid addresses
                endcase
            end
        end
        
        // Assertions
        assert(mem_fault === exp_mem_fault) else $fatal(1, "mem_fault mismatch for %s: Expected %b, got %b", desc_in, exp_mem_fault, mem_fault);
        assert(physical_addr === exp_physical_addr) else $fatal(1, "physical_addr mismatch for %s: Expected %h, got %h", desc_in, exp_physical_addr, physical_addr);
        assert(ram_we === exp_ram_we) else $fatal(1, "ram_we mismatch for %s: Expected %b, got %b", desc_in, exp_ram_we, ram_we);
        assert(ram_re === exp_ram_re) else $fatal(1, "ram_re mismatch for %s: Expected %b, got %b", desc_in, exp_ram_re, ram_re);
        assert(rom_re === exp_rom_re) else $fatal(1, "rom_re mismatch for %s: Expected %b, got %b", desc_in, exp_rom_re, rom_re);
        assert(uart_tx_we === exp_uart_tx_we) else $fatal(1, "uart_tx_we mismatch for %s: Expected %b, got %b", desc_in, exp_uart_tx_we, uart_tx_we);
        assert(uart_rx_we === exp_uart_rx_we) else $fatal(1, "uart_rx_we mismatch for %s: Expected %b, got %b", desc_in, exp_uart_rx_we, uart_rx_we);
        assert(led_we === exp_led_we) else $fatal(1, "led_we mismatch for %s: Expected %b, got %b", desc_in, exp_led_we, led_we);
        assert(switches_re === exp_switches_re) else $fatal(1, "switches_re mismatch for %s: Expected %b, got %b", desc_in, exp_switches_re, switches_re);
        
        $display("Test passed for %s (Addr: %h, WE: %b, RE: %b)", desc_in, addr_in, we_in, re_in);
    endtask

    initial begin
        $display("Starting Address Decoder Testbench...");
        
        
        // Boot Mode Tests (addr, we, re, boot, base, limit, desc)
        test_case(32'h0000_0000, 0, 1, 1, 0, 32'hFFFF_FFFF, "ROM Read (Boot Mode)");
        test_case(32'h0000_0000, 1, 0, 1, 0, 32'hFFFF_FFFF, "ROM Write Attempt (Boot Mode)");
        test_case(32'h8000_0000, 1, 0, 1, 0, 32'hFFFF_FFFF, "UART Write Attempt (Boot Mode)");
        
        // RAM (MSB == 0)
        test_case(32'h0000_0000, 1, 0, 0, 32'h0000_1000, 32'h0000_2000, "RAM Write (Base + Trans)");
        test_case(32'h0000_1000, 0, 1, 0, 32'h0000_1000, 32'h0000_2000, "RAM Read (Mid + Trans)");
        test_case(32'h0000_2000, 1, 1, 0, 32'h0000_1000, 32'h0000_2000, "RAM Out of Limit Attempt");
        
        // Peripheral (MSB == 1) - Valid Addresses
        test_case(32'h8000_0000, 1, 0, 0, 32'h0000_1000, 32'h0000_2000, "UART TX Write");
        test_case(32'h8000_0004, 0, 1, 0, 32'h0000_1000, 32'h0000_2000, "UART RX Read");
        test_case(32'h8000_0100, 1, 0, 0, 32'h0000_1000, 32'h0000_2000, "LED Write");
        test_case(32'h8000_0104, 0, 1, 0, 32'h0000_1000, 32'h0000_2000, "Switches Read");
        
        // Peripheral - Negative Tests (Wrong WE/RE or Invalid Addresses)
        test_case(32'h8000_0000, 0, 1, 0, 32'h0000_1000, 32'h0000_2000, "UART TX Read (Should be ignored)");
        test_case(32'h8000_0004, 1, 0, 0, 32'h0000_1000, 32'h0000_2000, "UART RX Write (Should be ignored)");
        test_case(32'h8000_0008, 1, 1, 0, 32'h0000_1000, 32'h0000_2000, "Invalid Peripheral Address (0x0008)");
        test_case(32'h8000_FFFF, 1, 1, 0, 32'h0000_1000, 32'h0000_2000, "Invalid High Peripheral Address");
        test_case(32'hFFFF_FFFF, 1, 1, 0, 32'h0000_1000, 32'h0000_2000, "Max Address Space (Invalid)");

        $display("Starting 1000 Randomized Tests...");
        
        for (i = 0; i < 1000; i = i + 1) begin
            rand_addr = $urandom;
            rand_we = $urandom_range(0, 1);
            rand_re = $urandom_range(0, 1);
            rand_boot = $urandom_range(0, 1);
            rand_base = $urandom;
            rand_limit = $urandom;
            
            test_case(rand_addr, rand_we, rand_re, rand_boot, rand_base, rand_limit, "Random");
        end

        $display("ALL TESTS PASSED SUCCESSFULLY!");
        $finish;
    end
endmodule