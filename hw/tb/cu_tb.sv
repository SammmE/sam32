`timescale 1ns / 1ps

module cu_tb();
    logic [31:0] instruction;
    logic [6:0]  opcode;
    logic [4:0]  Rd, Rs1, Rs2;
    logic [1:0]  st;
    logic [4:0]  sa;
    logic        fs;
    logic        Rs1Imm, Rs2Imm, is_offset;
    logic [12:0] Rs1ImmVal, Rs2ImmVal;
    logic [17:0] offset;
    logic        ALUMath, Branch, rjmp, Load, Store;
    logic [4:0]  operation;

    logic [6:0]  exp_opcode;
    logic [4:0]  exp_operation;
    logic [4:0]  exp_Rd, exp_Rs1, exp_Rs2;
    logic [1:0]  exp_st;
    logic [4:0]  exp_sa;
    logic        exp_fs;
    logic        exp_Rs1Imm, exp_Rs2Imm, exp_is_offset;
    logic [12:0] exp_Rs1ImmVal, exp_Rs2ImmVal;
    logic [17:0] exp_offset;
    logic        exp_ALUMath, exp_Branch, exp_rjmp, exp_Load, exp_Store;

    
    logic [1:0]  cat;
    logic [4:0]  op;
    logic        rs1_tog;
    logic        rs2_tog;
    
    logic [31:0] rand_instr;
    logic [1:0]  rand_cat;
    logic [4:0]  rand_op;
    logic [4:0]  rand_rd;
    logic [4:0]  rand_rs1;
    logic [4:0]  rand_rs2;
    logic [12:0] rand_imm13;
    logic [17:0] rand_off18;
    logic [4:0]  rand_sa;
    logic [1:0]  rand_st;
    logic        rand_fs;
    string       desc;
    int          i;

    control_unit dut (
        .instruction(instruction),
        .opcode(opcode),
        .Rd(Rd), .Rs1(Rs1), .Rs2(Rs2),
        .st(st), .sa(sa), .fs(fs),
        .Rs1Imm(Rs1Imm), .Rs2Imm(Rs2Imm), .is_offset(is_offset),
        .Rs1ImmVal(Rs1ImmVal), .Rs2ImmVal(Rs2ImmVal), .offset(offset),
        .ALUMath(ALUMath), .Branch(Branch), .rjmp(rjmp), .Load(Load), .Store(Store),
        .operation(operation)
    );

    
    task automatic test_case(input logic [31:0] instr_in, input string desc_in);
        instruction = instr_in;
        #10;

        cat = instr_in[1:0];
        op = instr_in[6:2];
        rs1_tog = instr_in[7];
        rs2_tog = instr_in[8];

        exp_opcode = instr_in[6:0];
        exp_operation = op;
        
        exp_ALUMath = (cat == 2'b00);
        exp_Branch = (cat == 2'b01);
        exp_rjmp = (cat == 2'b01) && instr_in[6];
        exp_Load = (cat == 2'b10) && instr_in[6];
        exp_Store = (cat == 2'b10) && ~instr_in[6];
        
        exp_Rs1Imm = rs1_tog;
        exp_Rs2Imm = rs2_tog;

        // defaults
        exp_Rd = 0; exp_Rs1 = 0; exp_Rs2 = 0;
        exp_st = 0; exp_sa = 0; exp_fs = 0;
        exp_is_offset = 0;
        exp_Rs1ImmVal = 0; exp_Rs2ImmVal = 0; exp_offset = 0;

        if (cat == 2'b00) begin
            if (~rs1_tog && ~rs2_tog) begin
                exp_Rd = instr_in[13:9];
                exp_Rs1 = instr_in[18:14];
                exp_Rs2 = instr_in[23:19];
                exp_st = instr_in[25:24];
                exp_sa = instr_in[30:26];
                exp_fs = instr_in[31];
            end else if (rs2_tog && ~rs1_tog) begin
                exp_Rd = instr_in[13:9];
                exp_Rs1 = instr_in[18:14];
                exp_Rs2ImmVal = instr_in[31:19];
            end else if (rs1_tog && ~rs2_tog) begin
                exp_Rd = instr_in[13:9];
                exp_Rs2 = instr_in[31:27];
                exp_Rs1ImmVal = instr_in[26:14];
            end
        end else if (cat == 2'b01) begin
            if (rs1_tog && rs2_tog) begin
                exp_Rd = instr_in[13:9];
                exp_is_offset = 1;
                exp_offset = instr_in[31:14];
            end else if (~rs1_tog && ~rs2_tog && instr_in[6]) begin
                exp_Rs1 = instr_in[13:9];
            end
        end else if (cat == 2'b10) begin
            exp_Rd = instr_in[13:9];
            exp_Rs1 = instr_in[18:14];
            exp_Rs2ImmVal = instr_in[31:19];
            exp_is_offset = 1;
            exp_offset = { {5{instr_in[31]}}, instr_in[31:19] };
        end

        assert(opcode === exp_opcode) else $fatal(1, "Opcode mismatch for %s: Expected %b, got %b", desc_in, exp_opcode, opcode);
        assert(operation === exp_operation) else $fatal(1, "Operation mismatch for %s: Expected %b, got %b", desc_in, exp_operation, operation);
        assert(ALUMath === exp_ALUMath) else $fatal(1, "ALUMath flag mismatch for %s", desc_in);
        assert(Branch === exp_Branch) else $fatal(1, "Branch flag mismatch for %s", desc_in);
        assert(rjmp === exp_rjmp) else $fatal(1, "rjmp flag mismatch for %s", desc_in);
        assert(Load === exp_Load) else $fatal(1, "Load flag mismatch for %s", desc_in);
        assert(Store === exp_Store) else $fatal(1, "Store flag mismatch for %s", desc_in);
        assert(Rs1Imm === exp_Rs1Imm) else $fatal(1, "Rs1Imm mismatch for %s", desc_in);
        assert(Rs2Imm === exp_Rs2Imm) else $fatal(1, "Rs2Imm mismatch for %s", desc_in);
        assert(Rd === exp_Rd) else $fatal(1, "Rd mismatch for %s: Expected %d, got %d", desc_in, exp_Rd, Rd);
        assert(Rs1 === exp_Rs1) else $fatal(1, "Rs1 mismatch for %s: Expected %d, got %d", desc_in, exp_Rs1, Rs1);
        assert(Rs2 === exp_Rs2) else $fatal(1, "Rs2 mismatch for %s: Expected %d, got %d", desc_in, exp_Rs2, Rs2);
        assert(st === exp_st) else $fatal(1, "Shift Type mismatch for %s", desc_in);
        assert(sa === exp_sa) else $fatal(1, "Shift Amount mismatch for %s", desc_in);
        assert(fs === exp_fs) else $fatal(1, "Freeze Status mismatch for %s", desc_in);
        assert(is_offset === exp_is_offset) else $fatal(1, "is_offset mismatch for %s", desc_in);
        assert(Rs1ImmVal === exp_Rs1ImmVal) else $fatal(1, "Rs1ImmVal mismatch for %s: Expected %h, got %h", desc_in, exp_Rs1ImmVal, Rs1ImmVal);
        assert(Rs2ImmVal === exp_Rs2ImmVal) else $fatal(1, "Rs2ImmVal mismatch for %s: Expected %h, got %h", desc_in, exp_Rs2ImmVal, Rs2ImmVal);
        assert(offset === exp_offset) else $fatal(1, "Offset mismatch for %s: Expected %h, got %h", desc_in, exp_offset, offset);

        $display("Test passed for %s (Instr: %h)", desc_in, instr_in);
    endtask

    initial begin
        $display("Starting Control Unit Testbench...");
        
        test_case({1'b1, 5'd4, 2'b01, 5'd15, 5'd10, 5'd5, 2'b00, 5'b00000, 2'b00}, "Format R (ALU)");
        test_case({13'h1A2, 5'd10, 5'd5, 1'b1, 1'b0, 5'b00001, 2'b00}, "Format I-A (ALU)");
        test_case({5'd15, 13'h064, 5'd5, 1'b0, 1'b1, 5'b00001, 2'b00}, "Format I-B (ALU)");
        test_case({18'd2048, 5'd0, 1'b1, 1'b1, 4'b0000, 1'b0, 2'b01}, "Format J (Branch)");
        test_case({18'b0, 5'd30, 1'b0, 1'b0, 4'b0000, 1'b1, 2'b01}, "Format R-Branch");
        test_case({13'h004, 5'd31, 5'd5, 1'b1, 1'b0, 5'b10010, 2'b10}, "Load (LW)");
        test_case({13'h008, 5'd31, 5'd5, 1'b1, 1'b0, 5'b00010, 2'b10}, "Store (SW)");
        test_case(32'hFFFFFFFF, "Invalid/System (Cat 11)");

        $display("Starting 1000 Randomized Tests...");
        
        for (i = 0; i < 1000; i = i + 1) begin
            rand_cat = $urandom_range(0, 2);
            rand_op = $urandom;
            rand_rd = $urandom;
            rand_rs1 = $urandom;
            rand_rs2 = $urandom;
            rand_imm13 = $urandom;
            rand_off18 = $urandom;
            rand_sa = $urandom;
            rand_st = $urandom;
            rand_fs = $urandom;

            if (rand_cat == 2'b00) begin
                case ($urandom_range(0, 2))
                    0: begin
                        rand_instr = {rand_fs, rand_sa, rand_st, rand_rs2, rand_rs1, rand_rd, 2'b00, rand_op, 2'b00};
                        desc = "Rand Format R";
                    end
                    1: begin
                        rand_instr = {rand_imm13, rand_rs1, rand_rd, 1'b1, 1'b0, rand_op, 2'b00};
                        desc = "Rand Format I-A";
                    end
                    2: begin
                        rand_instr = {rand_rs2, rand_imm13, rand_rd, 1'b0, 1'b1, rand_op, 2'b00};
                        desc = "Rand Format I-B";
                    end
                endcase
            end else if (rand_cat == 2'b01) begin
                if ($urandom_range(0, 1)) begin
                    rand_instr = {rand_off18, rand_rd, 1'b1, 1'b1, rand_op[4:1], 1'b0, 2'b01};
                    desc = "Rand Format J";
                end else begin
                    rand_instr = {18'b0, rand_rs1, 1'b0, 1'b0, rand_op[4:1], 1'b1, 2'b01};
                    desc = "Rand Format R-Branch";
                end
            end else if (rand_cat == 2'b10) begin
                rand_instr = {rand_imm13, rand_rs1, rand_rd, 1'b1, 1'b0, rand_op, 2'b10};
                desc = "Rand Mem (I-A layout)";
            end

            test_case(rand_instr, desc);
        end

        $display("ALL TESTS PASSED SUCCESSFULLY!");
        $finish;
    end
endmodule