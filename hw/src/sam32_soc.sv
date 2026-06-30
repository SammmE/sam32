`timescale 1ns / 1ps

module sam32_soc(
    input logic clk,
    input logic rst,
    input logic clk_200MHz, // DDR clock

    // DDR2 stuff
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
    inout  logic [1:0]  ddr2_dqs_n,

    // Basic IO from addr dec.
    output logic uart_tx_we,
    output logic uart_rx_we,
    output logic led_we,
    output logic switches_re,
    
    // External interrupts
    input logic ext_interrupt
);

    // wires
    // PC & IFU
    logic [31:0] pc_out;
    logic [31:0] next_pc;
    logic pc_src;
    logic pc_en;
    logic [31:0] instruction;
    
    // CU
    logic trap;
    logic [31:0] trap_cause;
    logic [6:0] opcode;
    logic [4:0] rd_addr, rs1_addr, rs2_addr;
    logic [1:0] st;
    logic [4:0] sa;
    logic fs;
    logic rs1_imm, rs2_imm, is_offset;
    logic [12:0] rs1_imm_val, rs2_imm_val;
    logic [17:0] branch_offset;
    logic alu_math, branch, rjmp, load, store;
    logic [4:0] operation;
    logic csr_read, csr_write, reti, ecall;
    
    // RegFile
    logic rf_we;
    logic [31:0] rd1, rd2, writeback_data;
    
    // ALU
    logic [31:0] alu_a, alu_b, alu_result;
    logic alu_z, alu_n, alu_c, alu_v;
    
    // Output Shifter
    logic [31:0] shifter_result;
    
    // CSRs
    logic [31:0] csr_rd;
    logic [31:0] status_csr, epc_csr, cause_csr, tvec_csr, gie_csr;
    logic [31:0] mtime_csr, mtimecmp_csr, boot_csr;
    logic [31:0] umem_base_csr, umem_limit_csr, estatus_csr;
    logic [31:0] cause_in;
    
    // interrupts
    logic timer_interrupt;
    logic global_interrupt;
    
    // jumping
    logic take_branch;
    
    // mem
    logic [31:0] mem_physical_addr;
    logic mem_fault;
    logic ram_we, ram_re, rom_re;
    logic [31:0] ram_data_out;
    logic ui_clk, ram_ready;

    // interrupt for timer or external
    assign global_interrupt = timer_interrupt | ext_interrupt;
    
    always_comb begin
        cause_in = 32'b0;
        if (ext_interrupt) begin
            cause_in = 32'h8000_0002;
        end else if (timer_interrupt) begin
            cause_in = 32'h8000_0001;
        end else if (ecall) begin
            cause_in = 32'h0000_0008;
        end
    end

    // fetch instructions
    assign pc_en = 1'b1;

    pc_reg u_pc_reg (
        .clk(clk),
        .rst(rst),
        .en(pc_en),
        .trap(trap | ecall),
        .reti(reti),
        .tvec(tvec_csr),
        .epc(epc_csr),
        .src(pc_src), // 0 for PC+4, 1 for branch
        .d_in(next_pc),
        .q_out(pc_out)
    );

    rom_32x1024 u_rom (
        .clk(clk),
        .addr(pc_out[11:2]), // Word
        .data(instruction)
    );

    // decode - step 2
    control_unit u_cu (
        .instruction(instruction),
        .interrupt(global_interrupt),
        .gie(gie_csr[0]),
        .mem_fault(mem_fault),
        
        .trap(trap),
        .trap_cause(trap_cause),
        .opcode(opcode),
        
        .Rd(rd_addr),
        .Rs1(rs1_addr),
        .Rs2(rs2_addr),
        
        .st(st),
        .sa(sa),
        .fs(fs),
        
        .Rs1Imm(rs1_imm),
        .Rs2Imm(rs2_imm),
        .is_offset(is_offset),
        .Rs1ImmVal(rs1_imm_val),
        .Rs2ImmVal(rs2_imm_val),
        .offset(branch_offset),
        
        .ALUMath(alu_math),
        .Branch(branch),
        .rjmp(rjmp),
        .Load(load),
        .Store(store),
        .operation(operation),
        
        .csr_read(csr_read),
        .csr_write(csr_write),
        .reti(reti),
        .ecall(ecall)
    );

    // reg file
    // Write enable if ALU math, load, or CSR read
    // Or if it's a Branch.
    assign rf_we = (alu_math | load | csr_read | (branch & ~rjmp)) & ~trap & ~ecall;

    // Immediate[31:19], Rs1[18:14], Rd[13:9]
    // CU maps Rd to rd_addr. rd_addr is data to store.
    logic [4:0] actual_rs2;
    assign actual_rs2 = store ? rd_addr : rs2_addr;

    reg_file u_rf (
        .clk(clk),
        .we(rf_we),
        .wd(writeback_data),
        .rs1(rs1_addr),
        .rs2(actual_rs2),
        .rd(rd_addr),
        .rd1(rd1),
        .rd2(rd2)
    );

    // ALU
    logic [31:0] rs1_imm_ext;
    logic [31:0] rs2_imm_ext;
    assign rs1_imm_ext = {{19{rs1_imm_val[12]}}, rs1_imm_val}; // extend to 32 bits with sign
    assign rs2_imm_ext = {{19{rs2_imm_val[12]}}, rs2_imm_val}; // ^

    assign alu_a = rs1_imm ? rs1_imm_ext : rd1;
    assign alu_b = rs2_imm ? rs2_imm_ext : rd2;

    alu u_alu (
        .a(alu_a),
        .b(alu_b),
        .alu_op(operation),
        .result(alu_result),
        .z(alu_z),
        .n(alu_n),
        .c(alu_c),
        .v(alu_v)
    );

    // shift outputs
    always_comb begin
        case (st)
            2'b00: shifter_result = alu_result << sa; // LSL
            2'b01: shifter_result = alu_result >> sa; // LSR
            2'b10: shifter_result = $signed(alu_result) >>> sa; // ASR
            2'b11: shifter_result = (alu_result >> sa) | (alu_result << (32 - sa)); // ROR
        endcase
    end

    // vbranches
    // Status flags are passed to the branch handler
    // If fs, use old status.
    logic cur_z, cur_n, cur_c, cur_v;
    assign cur_z = fs ? status_csr[0] : alu_z;
    assign cur_n = fs ? status_csr[1] : alu_n;
    assign cur_c = fs ? status_csr[2] : alu_c;
    assign cur_v = fs ? status_csr[3] : alu_v;

    branch_handler u_bh (
        .operation(operation),
        .z(cur_z),
        .n(cur_n),
        .c(cur_c),
        .v(cur_v),
        .branch(take_branch)
    );

    assign pc_src = branch & take_branch;
    
    // calc next pc
    logic [31:0] branch_offset_ext;
    // shift left by 2 to get byte offset (multiply by 4 for instruction width)
    assign branch_offset_ext = {{12{branch_offset[17]}}, branch_offset, 2'b00};
    
    assign next_pc = rjmp ? rd1 : (pc_out + branch_offset_ext);

    // csr file
    logic [31:0] csr_wd;
    assign csr_wd = rs1_imm ? rs1_imm_ext : rd1;
    
    csr_file u_csr (
        .clk(clk),
        .we(csr_write),
        .wd(csr_wd),
        .csr_addr(rd_addr), // Rd acts as CSR index
        .trap(trap | ecall),
        .reti(reti),
        .pc_in(pc_out),
        .cause_in(trap ? trap_cause : cause_in),
        
        .csr_rd(csr_rd),
        .STATUS(status_csr),
        .EPC(epc_csr),
        .CAUSE(cause_csr),
        .TVEC(tvec_csr),
        .GIE(gie_csr),
        .MTIME(mtime_csr),
        .MTIMECMP(mtimecmp_csr),
        .BOOT(boot_csr),
        .UMEM_BASE(umem_base_csr),
        .UMEM_LIMIT(umem_limit_csr),
        .ESTATUS(estatus_csr)
    );
    

    // hw timer
    timer u_timer (
        .clk(clk),
        .mtime(mtime_csr),
        .mtimecmp(mtimecmp_csr),
        .timer_interrupt(timer_interrupt)
    );

    // memory
    logic [31:0] mem_addr;
    assign mem_addr = rd1 + rs2_imm_ext; // Base + Offset (Format I-A)
    
    address_decoder u_ad (
        .address(mem_addr),
        .we(store),
        .re(load),
        .boot(boot_csr[0]),
        .umem_base(umem_base_csr),
        .umem_limit(umem_limit_csr),
        
        .physical_addr(mem_physical_addr),
        .mem_fault(mem_fault),
        .ram_we(ram_we),
        .ram_re(ram_re),
        .rom_re(rom_re),
        .uart_tx_we(uart_tx_we),
        .uart_rx_we(uart_rx_we),
        .led_we(led_we),
        .switches_re(switches_re)
    );

    // Memory Width encoding
    // Category 10: 00000 SB, 00001 SH, 00010 SW
    // Category 10: 10000 LB, 11000 LBU, 10001 LH, 11001 LHU, 10010 LW
    logic [1:0] mem_width;
    logic load_unsigned;
    
    always_comb begin
        if (operation[1:0] == 2'b00) mem_width = 2'b00; // Byte
        else if (operation[1:0] == 2'b01) mem_width = 2'b01; // Half
        else mem_width = 2'b10; // Word
        
        load_unsigned = operation[3];
    end

    ram u_ram (
        .clk_200MHz(clk_200MHz),
        .rst(rst),
        .ui_clk(ui_clk),
        
        .address(mem_physical_addr),
        .data_in(rd2), // rd2 holds the data to store
        .data_out(ram_data_out),
        .store(ram_we),
        .load(ram_re),
        .load_unsigned(load_unsigned),
        .width(mem_width),
        .ready(ram_ready),
        
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

    // Writeback
    always_comb begin
        if (load) begin
            writeback_data = ram_data_out;
        end else if (csr_read) begin
            writeback_data = csr_rd;
        end else if (branch && !rjmp) begin
            writeback_data = pc_out + 4; // Save return address for JAL (Format J)
        end else begin
            writeback_data = shifter_result; // Default ALU Math
        end
    end

endmodule
