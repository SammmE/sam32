`timescale 1ns / 1ps

module ram2ddrxadc (
    input logic clk_200MHz_i,
    input logic rst_i,
    input logic [11:0] device_temp_i,
    
    input logic [26:0] ram_a,
    input logic [15:0] ram_dq_i,
    output logic [15:0] ram_dq_o,
    input logic ram_cen,
    input logic ram_oen,
    input logic ram_wen,
    input logic ram_ub,
    input logic ram_lb,
    
    output logic [12:0] ddr2_addr,
    output logic [2:0]  ddr2_ba,
    output logic        ddr2_ras_n,
    output logic        ddr2_cas_n,
    output logic        ddr2_we_n,
    output logic [0:0]  ddr2_ck_p,
    output logic [0:0]  ddr2_ck_n,
    output logic [0:0]  ddr2_cke,
    output logic [0:0]  ddr2_cs_n,
    output logic [1:0]  ddr2_dm,
    output logic [0:0]  ddr2_odt,
    inout  wire  [15:0] ddr2_dq,
    inout  wire  [1:0]  ddr2_dqs_p,
    inout  wire  [1:0]  ddr2_dqs_n,
    
    output logic ui_clk_o,
    output logic ram_ready_o
);

    initial begin
        ui_clk_o = 0;
        forever #5 ui_clk_o = ~ui_clk_o;
    end

    logic [15:0] mock_mem [logic [26:0]];
    
    initial begin
        ram_ready_o = 0;
        #2000;
        @(posedge ui_clk_o);
        ram_ready_o = 1;
    end

    always_comb begin
        if (~ram_cen && ~ram_oen) begin
            if (mock_mem.exists(ram_a))
                ram_dq_o = mock_mem[ram_a];
            else
                ram_dq_o = 16'h0000; // Default uninitialized to 0
        end else begin
            ram_dq_o = 16'h0000;
        end
    end

    always_ff @(posedge ui_clk_o) begin
        if (~ram_cen && ~ram_wen) begin
            if (~ram_lb) mock_mem[ram_a][7:0] = ram_dq_i[7:0];
            if (~ram_ub) mock_mem[ram_a][15:8] = ram_dq_i[15:8];
        end
    end

    assign ddr2_addr = '0; assign ddr2_ba = '0; 
    assign ddr2_ras_n = 1'b1; assign ddr2_cas_n = 1'b1; assign ddr2_we_n = 1'b1;
    assign ddr2_ck_p = 1'b0; assign ddr2_ck_n = 1'b1; 
    assign ddr2_cke = 1'b0; assign ddr2_cs_n = 1'b1;
    assign ddr2_dm = '0; assign ddr2_odt = 1'b0;
    assign ddr2_dq = 'z; assign ddr2_dqs_p = 'z; assign ddr2_dqs_n = 'z;

endmodule


module ram_tb();
    logic        clk_200MHz;
    logic        rst;
    logic        ui_clk;
    logic [31:0] address;
    logic [31:0] data_in;
    logic [31:0] data_out;
    logic        store;
    logic        load;
    logic        load_unsigned;
    logic [1:0]  width;
    logic        ready;
    
    logic [12:0] ddr2_addr;
    logic [2:0]  ddr2_ba;
    logic        ddr2_ras_n;
    logic        ddr2_cas_n;
    logic        ddr2_we_n;
    logic [0:0]  ddr2_ck_p;
    logic [0:0]  ddr2_ck_n;
    logic [0:0]  ddr2_cke;
    logic [0:0]  ddr2_cs_n;
    logic [1:0]  ddr2_dm;
    logic [0:0]  ddr2_odt;
    wire  [15:0] ddr2_dq;
    wire  [1:0]  ddr2_dqs_p;
    wire  [1:0]  ddr2_dqs_n;

    logic [31:0] exp_data;
    logic [31:0] shadow_mem [logic [31:0]];
    
    int i;
    logic [31:0] rand_addr;
    logic [31:0] rand_data;
    logic [1:0]  rand_width;
    string       desc;

    initial begin
        clk_200MHz = 0;
        forever #2.5 clk_200MHz = ~clk_200MHz;
    end

    ram dut (
        .clk_200MHz(clk_200MHz),
        .rst(rst),
        .ui_clk(ui_clk),
        .address(address),
        .data_in(data_in),
        .data_out(data_out),
        .store(store),
        .load(load),
        .load_unsigned(load_unsigned),
        .width(width),
        .ready(ready),
        .ddr2_addr(ddr2_addr),
        .ddr2_ba(ddr2_ba),
        .ddr2_ras_n(ddr2_ras_n),
        .ddr2_cas_n(ddr2_cas_n),
        .ddr2_we_n(ddr2_we_n),
        .ddr2_ck_p(ddr2_ck_p),
        .ddr2_ck_n(ddr2_ck_n),
        .ddr2_cke(ddr2_cke),
        .ddr2_cs_n(ddr2_cs_n),
        .ddr2_dm(ddr2_dm),
        .ddr2_odt(ddr2_odt),
        .ddr2_dq(ddr2_dq),
        .ddr2_dqs_p(ddr2_dqs_p),
        .ddr2_dqs_n(ddr2_dqs_n)
    );

    task automatic write_mem(input logic [31:0] addr_in, input logic [31:0] data_in_val, input logic [1:0] w_in, input string desc_in);
        address = addr_in;
        data_in = data_in_val;
        width = w_in;
        store = 1;
        load = 0;
        
        @(posedge ui_clk);
        while (!ready) @(posedge ui_clk);
        
        if (w_in == 2'b10) begin
            shadow_mem[addr_in] = data_in_val;
        end else if (w_in == 2'b01) begin
            if (shadow_mem.exists(addr_in))
                shadow_mem[addr_in][15:0] = data_in_val[15:0];
            else
                shadow_mem[addr_in] = {16'b0, data_in_val[15:0]};
        end else begin // Byte
            if (shadow_mem.exists(addr_in)) begin
                if (addr_in[0] == 0) shadow_mem[addr_in][7:0] = data_in_val[7:0];
                else                 shadow_mem[addr_in][15:8] = data_in_val[7:0];
            end else begin
                if (addr_in[0] == 0) shadow_mem[addr_in] = {24'b0, data_in_val[7:0]};
                else                 shadow_mem[addr_in] = {16'b0, data_in_val[7:0], 8'b0};
            end
        end
        
        store = 0;
        @(posedge ui_clk);
        $display("Write passed for %s (Addr: %h, Data: %h, Width: %b)", desc_in, addr_in, data_in_val, w_in);
    endtask

    task automatic read_mem(input logic [31:0] addr_in, input logic [1:0] w_in, input logic u_in, input string desc_in);
        address = addr_in;
        width = w_in;
        load_unsigned = u_in;
        store = 0;
        load = 1;
        
        @(posedge ui_clk);
        while (!ready) @(posedge ui_clk);
        
        exp_data = 32'b0;
        if (shadow_mem.exists(addr_in)) begin
            if (w_in == 2'b10) begin
                exp_data = shadow_mem[addr_in];
            end else if (w_in == 2'b01) begin
                if (u_in) exp_data = {16'b0, shadow_mem[addr_in][15:0]};
                else      exp_data = {{16{shadow_mem[addr_in][15]}}, shadow_mem[addr_in][15:0]};
            end else begin
                if (addr_in[0] == 0) begin
                    if (u_in) exp_data = {24'b0, shadow_mem[addr_in][7:0]};
                    else      exp_data = {{24{shadow_mem[addr_in][7]}}, shadow_mem[addr_in][7:0]};
                end else begin
                    if (u_in) exp_data = {24'b0, shadow_mem[addr_in][15:8]};
                    else      exp_data = {{24{shadow_mem[addr_in][15]}}, shadow_mem[addr_in][15:8]};
                end
            end
        end
        
        assert(data_out === exp_data) else $fatal(1, "Read mismatch for %s at addr %h: Expected %h, got %h", desc_in, addr_in, exp_data, data_out);
        
        load = 0;
        @(posedge ui_clk);
        $display("Read passed for %s (Addr: %h, Data: %h, Width: %b)", desc_in, addr_in, data_out, w_in);
    endtask

    initial begin
        $display("Starting RAM Testbench...");
        
        rst = 1;
        address = 0; data_in = 0; store = 0; load = 0; load_unsigned = 0; width = 0;
        #100;
        rst = 0;
        
        #3000; 
        
        // Word Write & Read
        write_mem(32'h0000_1000, 32'hDEAD_BEEF, 2'b10, "Word Write");
        read_mem(32'h0000_1000, 2'b10, 1'b0, "Word Read");
        
        // Halfword Write & Read
        write_mem(32'h0000_2000, 32'h1234_5678, 2'b01, "Halfword Write");
        read_mem(32'h0000_2000, 2'b01, 1'b0, "Halfword Read Signed");
        read_mem(32'h0000_2000, 2'b01, 1'b1, "Halfword Read Unsigned");
        
        // Halfword Write & Read (Negative)
        write_mem(32'h0000_2004, 32'h1234_F678, 2'b01, "Halfword Write Negative");
        read_mem(32'h0000_2004, 2'b01, 1'b0, "Halfword Read Negative Signed");
        read_mem(32'h0000_2004, 2'b01, 1'b1, "Halfword Read Negative Unsigned");
        
        // Byte Write & Read (Even Address)
        write_mem(32'h0000_3000, 32'h0000_00AB, 2'b00, "Byte Write (Even)");
        read_mem(32'h0000_3000, 2'b00, 1'b0, "Byte Read Signed (Even)");
        read_mem(32'h0000_3000, 2'b00, 1'b1, "Byte Read Unsigned (Even)");
        
        // Byte Write & Read (Odd Address)
        write_mem(32'h0000_3001, 32'h0000_00CD, 2'b00, "Byte Write (Odd)");
        read_mem(32'h0000_3001, 2'b00, 1'b0, "Byte Read Signed (Odd)");
        read_mem(32'h0000_3001, 2'b00, 1'b1, "Byte Read Unsigned (Odd)");
        
        // Back to back Word operations
        write_mem(32'h0000_4000, 32'h1111_2222, 2'b10, "Word Write 1");
        write_mem(32'h0000_4004, 32'h3333_4444, 2'b10, "Word Write 2");
        read_mem(32'h0000_4000, 2'b10, 1'b0, "Word Read 1");
        read_mem(32'h0000_4004, 2'b10, 1'b0, "Word Read 2");

        $display("Starting 50 Randomized Tests...");
        
        for (i = 0; i < 50; i = i + 1) begin
            rand_addr = $urandom;
            rand_width = $urandom_range(0, 2);
            
            // Force alignment
            if (rand_width == 2'b10) rand_addr[1:0] = 2'b00;
            else if (rand_width == 2'b01) rand_addr[0] = 1'b0;
            
            rand_data = $urandom;
            
            desc = "Random Write";
            write_mem(rand_addr, rand_data, rand_width, desc);
            
            desc = "Random Read";
            read_mem(rand_addr, rand_width, $urandom_range(0, 1), desc);
        end

        $display("ALL TESTS PASSED SUCCESSFULLY!");
        $finish;
    end
endmodule