module main (
    // Reset and Clocks
    input CLK50,
    input RESET_N,

    // LED and Push Button
    output reg [3:0] USER_LED,
    input  wire [3:0] PB,

    // FX3
    inout  [31:0] DQ,      // data bus
    output [ 1:0] ADDR,    // output fifo address
    output        SLRD,    // output read select
    output        SLWR,    // output write select
    input         FLAGA,   // full flag
    input         FLAGB,   // partial full flag
    input         FLAGC,   // empty flag
    input         FLAGD,   // empty partial flag
    output        SLOE,    // output output enable select
    output        SLCS,    // output chip select
    output        PKEND,   // output pkt end
    output        CLK_OUT  // output clk 100 Mhz and 180 phase shift
);

    reg  [ 1:0] oe_delay_cnt;
    reg         rd_oe_delay_cnt;
    reg  [31:0] write_delay_cnt;
    reg  [ 7:0] data_gen;
    reg  [31:0] wait_ctr;
    reg         update_ctr_flag;
    reg  [31:0] wait_ctr_gbl;
    reg  [15:0] tx_ctr;
    reg         all_ctr_reset;
    reg  [31:0] packet_index;
    reg         valid_packet;
    reg  [31:0] packets_to_send;
    reg  [31:0] write_ctr;
    wire        read_ready;
    wire [31:0] read_data;

    reg  [ 7:0] ch_src[7:0];
    wire [ 7:0] ch_data;
    wire [ 12:0] data_src;

    wire [31:0] data_out;
    reg  [31:0] DQ_d;

    reg         SLRD_d1_;
    reg         SLRD_d2_;
    reg         SLRD_d3_;
    reg         SLRD_d4_;
    reg  [ 1:0] gpif_address;
    reg  [ 1:0] gpif_address_d;
    reg         FLAGA_d;
    reg         FLAGB_d;
    reg         FLAGC_d;
    reg         FLAGD_d;
    wire [31:0] data_out_loopback;

    reg  [ 3:0] current_sm_state;
    reg  [ 3:0] next_sm_state;
    reg         SLWR_1d_;

    // parameters for LoopBack mode state machine
    parameter [3:0] sm_idle = 4'd0;
    parameter [3:0] sm_flagc_rcvd = 4'd1;
    parameter [3:0] sm_wait_flagd = 4'd2;
    parameter [3:0] sm_read = 4'd3;
    parameter [3:0] sm_read_rd_and_oe_delay = 4'd4;
    parameter [3:0] sm_read_oe_delay = 4'd5;
    parameter [3:0] sm_wait_flaga = 4'd6;
    parameter [3:0] sm_wait_flagb = 4'd7;
    parameter [3:0] sm_write = 4'd8;
    parameter [3:0] sm_write_wr_delay = 4'd9;

    // output signal assignment
    assign SLRD  = SLRD_;
    assign SLWR  = SLWR_1d_;
    assign ADDR  = gpif_address_d;
    assign SLOE  = SLOE_;
    assign SLCS  = 1'b0;
    assign PKEND = 1'b1;

    assign reset_ = lock;

    // clock generation(pll instantiation)
    pll inst_clk_pll (
        .areset(1'b0),
        .inclk0(CLK50),
        .c0    (clk_pll),
        .c1    (clk_pllx4),
        .locked(lock)
    );

    // ddr is used to send out the clk(DDR instantiation)

    ddr inst_ddr_to_send_clk_to_fx3 (
        .datain_h(1'b0),
        .datain_l(1'b1),
        .outclock(clk_pll),
        .dataout (CLK_OUT)
    );

    nios u0 (
        .clk_clk         (clk_pll),
        .reset_reset_n   (reset_),
        .state_in_export (update_ctr_flag),
        .data_in_export  (wait_ctr)
    );

    vio u1 (
        .probe({tx_ctr, wait_ctr_gbl, write_ctr, current_sm_state, ch_src[0], ch_data}),
        .source(all_ctr_reset)
    );

    // flopping the INPUTs flags
    always @(posedge clk_pll, negedge reset_) begin
        if (!reset_) begin
            FLAGA_d <= 1'd0;
            FLAGB_d <= 1'd0;
            FLAGC_d <= 1'd0;
            FLAGD_d <= 1'd0;
        end else begin
            FLAGA_d <= FLAGA;
            FLAGB_d <= FLAGB;
            FLAGC_d <= FLAGC;
            FLAGD_d <= FLAGD;
        end
    end

    // output control signal generation
    assign SLRD_ = ((current_sm_state == sm_read) | (current_sm_state == sm_read_rd_and_oe_delay)) ? 1'b0 : 1'b1;
    assign SLOE_ = ((current_sm_state == sm_read) | (current_sm_state == sm_read_rd_and_oe_delay) | (current_sm_state == sm_read_oe_delay)) ? 1'b0 : 1'b1;
    assign SLWR_ = ((current_sm_state == sm_write)) ? 1'b0 : 1'b1;

    always @(posedge clk_pll, negedge reset_) begin
        if (!reset_) begin
            SLWR_1d_ <= 1'b1;
        end else begin
            SLWR_1d_ <= SLWR_;
        end
    end

    // delay for reading from slave fifo(data will be available after two clk cycle)
    always @(posedge clk_pll, negedge reset_) begin
        if (!reset_) begin
            SLRD_d1_ <= 1'b1;
            SLRD_d2_ <= 1'b1;
            SLRD_d3_ <= 1'b1;
            SLRD_d4_ <= 1'b1;
        end else begin
            SLRD_d1_ <= SLRD_;
            SLRD_d2_ <= SLRD_d1_;
            SLRD_d3_ <= SLRD_d2_;
            SLRD_d4_ <= SLRD_d3_;
        end
    end

    // flopping the input data
    always @(posedge clk_pll, negedge reset_) begin
        if (!reset_) begin
            DQ_d <= 32'd0;
        end else begin
            DQ_d <= DQ;
        end
    end

    // slave gpif address
    always @(*) begin
        if( (current_sm_state == sm_flagc_rcvd) |
            (current_sm_state == sm_wait_flagd) |
            (current_sm_state == sm_read) |
            (current_sm_state == sm_read_rd_and_oe_delay) |
            (current_sm_state == sm_read_oe_delay)) begin
            gpif_address = 2'b11;
        end else begin
            if (write_delay_cnt[12]) begin // Every burst change buffer
                gpif_address = 2'b01;
            end else begin
                gpif_address = 2'b00;
            end
        end
    end

    // flopping the output gpif address
    always @(posedge clk_pll, negedge reset_) begin
        if (!reset_) begin
            gpif_address_d <= 2'd0;
        end else begin
            gpif_address_d <= gpif_address;
        end
    end

    // counter to delay the read and output enable signal
    always @(posedge clk_pll, negedge reset_) begin
        if (!reset_) begin
            rd_oe_delay_cnt <= 1'b0;
        end else if (current_sm_state == sm_read) begin
            rd_oe_delay_cnt <= 1'b1;
        end else if((current_sm_state == sm_read_rd_and_oe_delay) & (rd_oe_delay_cnt > 1'b0)) begin
            rd_oe_delay_cnt <= rd_oe_delay_cnt - 1'b1;
        end
    end

    // Counter to delay the OUTPUT Enable(oe) signal
    always @(posedge clk_pll, negedge reset_) begin
        if (!reset_) begin
            oe_delay_cnt <= 2'd0;
        end else if (current_sm_state == sm_read_rd_and_oe_delay) begin
            oe_delay_cnt <= 2'd2;
        end else if ((current_sm_state == sm_read_oe_delay) & (oe_delay_cnt > 1'b0)) begin
            oe_delay_cnt <= oe_delay_cnt - 1'b1;
        end
    end

    // Counter of transferred packets
    always @(posedge clk_pll, negedge reset_) begin
        if (!reset_) begin
            write_delay_cnt <= 32'd0;
        end else if (current_sm_state == sm_wait_flaga) begin
            write_delay_cnt <= (packets_to_send << 12) - 1; // Multiply by 16384/4
        end else if ((current_sm_state == sm_write) & (write_delay_cnt > 1'b0)) begin
            write_delay_cnt <= write_delay_cnt - 1'b1;
        end
    end

    // Wait counter
    always @(posedge clk_pll, negedge reset_) begin
        if (!reset_) begin
            wait_ctr <= 0;
        end else if (current_sm_state == sm_idle) begin
            wait_ctr <= 0;
        end else if (current_sm_state == sm_wait_flagb || current_sm_state == sm_wait_flaga) begin
            wait_ctr <= wait_ctr + 1;
        end
    end

    // Wait global counter
    always @(posedge clk_pll, negedge reset_) begin
        if (!reset_) begin
            wait_ctr_gbl <= 0;
        end else if (all_ctr_reset) begin
            wait_ctr_gbl <= 0;
        end else if (current_sm_state == sm_wait_flagb || current_sm_state == sm_wait_flaga) begin
            wait_ctr_gbl <= wait_ctr_gbl + 1;
        end
    end

    // Wait counter flag
    always @(posedge clk_pll, negedge reset_) begin
        if (!reset_) begin
            update_ctr_flag <= 0;
        end else if (current_sm_state == sm_wait_flagb || current_sm_state == sm_wait_flaga) begin
            update_ctr_flag <= 0;
        end else begin
            update_ctr_flag <= 1;
        end
    end

    // TX counter
    always @(posedge clk_pll, negedge reset_) begin
        if (!reset_) begin
            tx_ctr <= 0;
        end else if (all_ctr_reset) begin
            tx_ctr <= 0;
        end else if (current_sm_state == sm_write_wr_delay) begin
            tx_ctr <= tx_ctr + 1;
        end
    end

    // Write counter
    always @(posedge clk_pll, negedge reset_) begin
        if (!reset_) begin
            write_ctr <= 0;
        end else if (all_ctr_reset) begin
            write_ctr <= 0;
        end else if (current_sm_state == sm_write) begin
            write_ctr <= write_ctr + 1;
        end
    end

    // LoopBack state machine
    always @(posedge clk_pll, negedge reset_) begin
        if (!reset_) begin
            current_sm_state <= sm_idle;
        end else begin
            current_sm_state <= next_sm_state;
        end
    end

    // LoopBack mode state machine combo
    always @(*) begin
        next_sm_state = current_sm_state;
        case (current_sm_state)
            sm_idle: begin
                if (FLAGC_d == 1'b1) begin
                    next_sm_state = sm_flagc_rcvd;
                end
            end
            sm_flagc_rcvd: begin
                next_sm_state = sm_wait_flagd;
            end
            sm_wait_flagd: begin
                if (FLAGD_d == 1'b1) begin
                    next_sm_state = sm_read;
                end
            end
            sm_read: begin
                if (FLAGD_d == 1'b0) begin
                    next_sm_state = sm_read_rd_and_oe_delay;
                end
            end
            sm_read_rd_and_oe_delay: begin
                if (rd_oe_delay_cnt == 0) begin
                    next_sm_state = sm_read_oe_delay;
                end
            end
            sm_read_oe_delay: begin
                if (oe_delay_cnt == 0) begin
                    if (packets_to_send == 0) begin
                        next_sm_state = sm_idle;
                    end else begin
                        next_sm_state = sm_wait_flaga;
                    end
                end
            end
            sm_wait_flaga: begin
                if (FLAGA_d == 1'b1) begin
                    next_sm_state = sm_wait_flagb;
                end
            end
            sm_wait_flagb: begin
                if (FLAGB_d == 1'b1) begin
                    next_sm_state = sm_write;
                end
            end
            sm_write: begin
                if (write_delay_cnt == 0) begin
                    next_sm_state = sm_write_wr_delay;
                end
            end
            sm_write_wr_delay: begin
                next_sm_state = sm_idle;
            end
        endcase
    end

    assign read_ready = (SLRD_d3_ == 1'b0);
    assign read_data  = (SLRD_d3_ == 1'b0) ? DQ_d : 32'd0;

    // Read packet index
    always @(posedge clk_pll, negedge reset_) begin
        if (!reset_) begin
            packet_index <= 0;
        end else if (read_ready) begin
            packet_index <= packet_index + 1;
        end else begin
            packet_index <= 0;
        end
    end

    // Check header
    always @(posedge clk_pll, negedge reset_) begin
        if (!reset_) begin
            valid_packet <= 0;
        end else if (!read_ready) begin
            valid_packet <= 0;
        end else if (read_ready && packet_index == 0 && read_data == 32'hCAFEB0BA) begin
            valid_packet <= 1;
        end
    end

    // Parse packet
    always @(posedge clk_pll, negedge reset_) begin
        if (!reset_) begin
            packets_to_send <= 32'h0;
            ch_src[0] <= 8'h8;
            ch_src[1] <= 8'h8;
            ch_src[2] <= 8'h8;
            ch_src[3] <= 8'h8;
            ch_src[4] <= 8'h8;
            ch_src[5] <= 8'h8;
            ch_src[6] <= 8'h8;
            ch_src[7] <= 8'h8;
            USER_LED <= 4'b1111;
        end else if (valid_packet) begin
            case (packet_index)
                1: begin
                    packets_to_send <= read_data;
                end
                2: begin
                    ch_src[0] <= read_data[7:0];
                    ch_src[1] <= read_data[15:8];
                    ch_src[2] <= read_data[23:16];
                    ch_src[3] <= read_data[31:24];
                end
                3: begin
                    ch_src[4] <= read_data[7:0];
                    ch_src[5] <= read_data[15:8];
                    ch_src[6] <= read_data[23:16];
                    ch_src[7] <= read_data[31:24];
                end
                4: begin
                    USER_LED <= ~read_data[3:0];
                end
            endcase
        end
    end

    reg [3:0] PB_1d;
    reg [3:0] PB_2d;
    // 2 flip flop synchronizer - input
    always @(posedge clk_pllx4) begin
        if(!reset_)begin
            PB_1d <= 4'd0;
            PB_2d <= 4'd0;
        end else begin
            PB_1d <= PB;
            PB_2d <= PB_1d;
        end
    end

    assign data_src = {PB_2d, 1'b0, data_gen};
    // Invert bits order to match app
    assign ch_data[7] = data_src[ch_src[0]]; // CH0
    assign ch_data[6] = data_src[ch_src[1]]; // CH1
    assign ch_data[5] = data_src[ch_src[2]]; // CH2
    assign ch_data[4] = data_src[ch_src[3]]; // CH3
    assign ch_data[3] = data_src[ch_src[4]]; // CH4
    assign ch_data[2] = data_src[ch_src[5]]; // CH5
    assign ch_data[1] = data_src[ch_src[6]]; // CH6
    assign ch_data[0] = data_src[ch_src[7]]; // CH7


    // data generator counter
    always @(posedge clk_pllx4, negedge reset_)begin
        if(!reset_)begin
            data_gen <= 8'd0;
        end else begin
            data_gen <= data_gen + 1;
        end
    end

    reg [31:0] ch_data_d;
    // channels data shifter
    always @(posedge clk_pllx4, negedge reset_)begin
        if(!reset_)begin
            ch_data_d <= 32'd0;
        end else begin
            ch_data_d <= {ch_data, ch_data_d[31:8]};
        end
    end

    reg [31:0] data_out_1d;
    reg [31:0] data_out_2d;
    // 2 flip flop synchronizer
    always @(posedge clk_pll) begin
        if(!reset_)begin
            data_out_1d <= 32'd0;
            data_out_2d <= 32'd0;
        end else begin
            data_out_1d <= ch_data_d;
            data_out_2d <= data_out_1d;
        end

    end

    assign DQ = (SLWR_1d_) ? 32'dz : data_out_2d;

endmodule
