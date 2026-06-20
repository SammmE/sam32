`timescale 1ns / 1ps

module ram (
    input  logic        clk_200MHz, // 200MHz reference clock for DDR
    input  logic        rst,
    
    output logic        ui_clk,
    
    input  logic [31:0] address,
    input  logic [31:0] data_in,
    output logic [31:0] data_out,
    input  logic        store,
    input  logic        load,
    input  logic [1:0]  width, // 00=byte, 01=half-word, 10=word
    output logic        ready,
    
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
    inout  logic [15:0] ddr2_dq,
    inout  logic [1:0]  ddr2_dqs_p,
    inout  logic [1:0]  ddr2_dqs_n
);

    logic ram_ready;
    
    logic [26:0] ram_a;
    logic [15:0] ram_dq_i;
    logic [15:0] ram_dq_o;
    logic        ram_cen;
    logic        ram_oen;
    logic        ram_wen;
    logic        ram_ub;
    logic        ram_lb;

    ram2ddrxadc ram_inst (
        .clk_200MHz_i (clk_200MHz),
        .rst_i        (rst),
        .device_temp_i(12'b0),
        
        .ram_a        (ram_a),
        .ram_dq_i     (ram_dq_i),
        .ram_dq_o     (ram_dq_o),
        .ram_cen      (ram_cen),
        .ram_oen      (ram_oen),
        .ram_wen      (ram_wen),
        .ram_ub       (ram_ub),
        .ram_lb       (ram_lb),
        
        .ddr2_addr    (ddr2_addr),
        .ddr2_ba      (ddr2_ba),
        .ddr2_ras_n   (ddr2_ras_n),
        .ddr2_cas_n   (ddr2_cas_n),
        .ddr2_we_n    (ddr2_we_n),
        .ddr2_ck_p    (ddr2_ck_p),
        .ddr2_ck_n    (ddr2_ck_n),
        .ddr2_cke     (ddr2_cke),
        .ddr2_cs_n    (ddr2_cs_n),
        .ddr2_dm      (ddr2_dm),
        .ddr2_odt     (ddr2_odt),
        .ddr2_dq      (ddr2_dq),
        .ddr2_dqs_p   (ddr2_dqs_p),
        .ddr2_dqs_n   (ddr2_dqs_n),
        
        .ui_clk_o     (ui_clk),
        .ram_ready_o  (ram_ready)
    );

    typedef enum logic [2:0] {
        IDLE,
        WAIT_RAM_READY,
        WORD_TRANSITION,
        WAIT_RAM_READY_2,
        DONE
    } state_t;

    state_t state, next_state;
    
    logic [31:0] read_data_reg;
    
    always_ff @(posedge ui_clk) begin
        if (rst) begin
            state <= IDLE;
            read_data_reg <= 32'b0;
        end else begin
            state <= next_state;
            
            // Capture read data
            if (state == WAIT_RAM_READY && ram_ready && load) begin
                if (width == 2'b10) begin
                    read_data_reg[15:0] <= ram_dq_o;
                end else begin
                    // Byte or Half word
                    read_data_reg[15:0] <= ram_dq_o;
                end
            end else if (state == WAIT_RAM_READY_2 && ram_ready && load) begin
                read_data_reg[31:16] <= ram_dq_o;
            end
        end
    end

    always_comb begin
        next_state = state;
        
        ram_cen = 1'b1; 
        ram_oen = 1'b1;
        ram_wen = 1'b1;
        ram_ub  = 1'b1;
        ram_lb  = 1'b1;
        
        ram_a   = address[27:1]; 
        ram_dq_i = 16'b0;
        
        ready = 1'b0;

        case (state)
            IDLE: begin
                if (store || load) begin
                    next_state = WAIT_RAM_READY;
                end
            end
            
            WAIT_RAM_READY: begin
                ram_cen = 1'b0;
                ram_a = address[27:1]; // Lower 16-bit word address
                
                if (store) begin
                    ram_wen = 1'b0;
                    if (width == 2'b00) begin // Byte
                        if (address[0] == 1'b0) begin
                            ram_lb = 1'b0;
                            ram_dq_i[7:0] = data_in[7:0];
                        end else begin
                            ram_ub = 1'b0;
                            ram_dq_i[15:8] = data_in[7:0];
                        end
                    end else begin // Halfword or Word (first half)
                        ram_lb = 1'b0;
                        ram_ub = 1'b0;
                        ram_dq_i = data_in[15:0];
                    end
                end else if (load) begin
                    ram_oen = 1'b0;
                    ram_lb = 1'b0;
                    ram_ub = 1'b0;
                end

                if (ram_ready) begin
                    if (width == 2'b10) begin 
                        // Word meeds second half
                        next_state = WORD_TRANSITION;
                    end else begin
                        next_state = DONE;
                    end
                end
            end
            
            WORD_TRANSITION: begin
                ram_cen = 1'b1;
                next_state = WAIT_RAM_READY_2;
            end
            
            WAIT_RAM_READY_2: begin
                ram_cen = 1'b0;
                ram_a = address[27:1] + 27'd1; // Upper 16-bit word address
                
                if (store) begin
                    ram_wen = 1'b0;
                    ram_dq_i = data_in[31:16];
                    ram_lb = 1'b0;
                    ram_ub = 1'b0;
                end else if (load) begin
                    ram_oen = 1'b0;
                    ram_lb = 1'b0;
                    ram_ub = 1'b0;
                end
                
                if (ram_ready) begin
                    next_state = DONE;
                end
            end
            
            DONE: begin
                ready = 1'b1;
                // Deassert immediately once CPU drops the request
                if (!store && !load) begin
                    next_state = IDLE;
                end
            end
            
            default: next_state = IDLE;
        endcase
    end
    
    // Format output based on width
    always_comb begin
        data_out = read_data_reg;
        if (width == 2'b00) begin
            if (address[0] == 1'b0) data_out = {24'b0, read_data_reg[7:0]};
            else                    data_out = {24'b0, read_data_reg[15:8]};
        end else if (width == 2'b01) begin
            data_out = {16'b0, read_data_reg[15:0]};
        end
    end

endmodule
