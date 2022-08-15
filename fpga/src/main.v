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
    wire [31:0] fifo_data_in;
    reg  [ 7:0] data_gen_0;
    reg  [ 7:0] data_gen_1;
    reg  [ 7:0] data_gen_2;
    reg  [ 7:0] data_gen_3;
    reg  [31:0] wait_ctr;
    reg         update_ctr_flag;
    reg  [31:0] wait_ctr_gbl;
    reg  [15:0] tx_ctr;
    reg         all_ctr_reset;
    reg  [31:0] packet_index;
    reg         valid_packet;
    reg  [31:0] packets_to_send;
    wire        read_ready;
    wire [31:0] read_data;

    wire [31:0] data_out;
    reg  [31:0] DQ_d;

    reg         SLRD_loopback_d1_;
    reg         SLRD_loopback_d2_;
    reg         SLRD_loopback_d3_;
    reg         SLRD_loopback_d4_;
    reg  [ 1:0] fifo_address;
    reg  [ 1:0] fifo_address_d;
    reg         FLAGA_d;
    reg         FLAGB_d;
    reg         FLAGC_d;
    reg         FLAGD_d;
    wire [31:0] data_out_loopback;

    reg  [ 3:0] current_sm_state;
    reg  [ 3:0] next_sm_state;
    reg         SLWR_loopback_1d_;

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
    parameter [3:0] sm_flush_fifo = 4'd10;

    // output signal assignment
    assign SLRD  = SLRD_loopback_;
    assign SLWR  = SLWR_loopback_1d_;
    assign ADDR  = fifo_address_d;
    assign SLOE  = SLOE_loopback_;
    assign SLCS  = 1'b0;
    assign PKEND = 1'b1;

    assign reset_ = lock;

    // clock generation(pll instantiation)
    pll inst_clk_pll (
        .areset(1'b0),
        .inclk0(CLK50),
        .c0    (clk_pll),
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
        .probe({tx_ctr, wait_ctr_gbl, packets_to_send, current_sm_state}),
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
    assign SLRD_loopback_ = ((current_sm_state == sm_read) | (current_sm_state == sm_read_rd_and_oe_delay)) ? 1'b0 : 1'b1;
    assign SLOE_loopback_ = ((current_sm_state == sm_read) | (current_sm_state == sm_read_rd_and_oe_delay) | (current_sm_state == sm_read_oe_delay)) ? 1'b0 : 1'b1;
    assign SLWR_loopback_ = ((current_sm_state == sm_write)) ? 1'b0 : 1'b1;

    always @(posedge clk_pll, negedge reset_) begin
        if (!reset_) begin
            SLWR_loopback_1d_ <= 1'b1;
        end else begin
            SLWR_loopback_1d_ <= SLWR_loopback_;
        end
    end

    // delay for reading from slave fifo(data will be available after two clk cycle)
    always @(posedge clk_pll, negedge reset_) begin
        if (!reset_) begin
            SLRD_loopback_d1_ <= 1'b1;
            SLRD_loopback_d2_ <= 1'b1;
            SLRD_loopback_d3_ <= 1'b1;
            SLRD_loopback_d4_ <= 1'b1;
        end else begin
            SLRD_loopback_d1_ <= SLRD_loopback_;
            SLRD_loopback_d2_ <= SLRD_loopback_d1_;
            SLRD_loopback_d3_ <= SLRD_loopback_d2_;
            SLRD_loopback_d4_ <= SLRD_loopback_d3_;
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

    // Control signal of internal fifo for LoopBack mode
    assign fifo_push    = (SLRD_loopback_d3_ == 1'b0);
    assign fifo_pop     = (current_sm_state == sm_write);
    assign fifo_flush   = (current_sm_state == sm_flush_fifo);

    assign fifo_data_in = (SLRD_loopback_d3_ == 1'b0) ? DQ_d : 32'd0;

    // slave fifo address
    always @(*) begin
        if( (current_sm_state == sm_flagc_rcvd) |
            (current_sm_state == sm_wait_flagd) |
            (current_sm_state == sm_read) |
            (current_sm_state == sm_read_rd_and_oe_delay) |
            (current_sm_state == sm_read_oe_delay)) begin
            fifo_address = 2'b11;
        end else begin
            fifo_address = 2'b00;
        end
    end

    // flopping the output fifo address
    always @(posedge clk_pll, negedge reset_) begin
        if (!reset_) begin
            fifo_address_d <= 2'd0;
        end else begin
            fifo_address_d <= fifo_address;
        end
    end

    reg [31:0] packets_number;
    always @(posedge clk_pll, negedge reset_) begin
        if (!reset_) begin
            packets_number <= 0;
        end else begin
            if (packets_number > 0 && current_sm_state == sm_write_wr_delay) begin
                packets_number <= packets_number - 1;
            end else if (current_sm_state == sm_read_oe_delay) begin
                packets_number <= packets_to_send;
            end
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
        end else begin
            oe_delay_cnt <= oe_delay_cnt;
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
                end else if (packets_number > 0) begin
                    next_sm_state = sm_wait_flaga;
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
                    next_sm_state = sm_wait_flaga;
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
                if (FLAGB_d == 1'b0) begin
                    next_sm_state = sm_write_wr_delay;
                end
            end
            sm_write_wr_delay: begin
                next_sm_state = sm_flush_fifo;
            end
            sm_flush_fifo: begin
                next_sm_state = sm_idle;
            end
        endcase
    end

    assign read_ready = (SLRD_loopback_d3_ == 1'b0);
    assign read_data  = (SLRD_loopback_d3_ == 1'b0) ? DQ_d : 32'd0;

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
        end else if (valid_packet) begin
            case (packet_index)
                1: begin
                    packets_to_send <= read_data;
                end
            endcase
        end
    end

    // // fifo instantiation for loop back mode
    // fifo fifo_inst(
    //     .din(fifo_data_in),
    //     .write_busy(fifo_push),
    //     .fifo_full(),
    //     .dout(data_out_loopback),
    //     .read_busy(fifo_pop),
    //     .fifo_empty(),
    //     .fifo_clk(clk_pll),
    //     .reset_(reset_),
    //     .fifo_flush(fifo_flush)
    // );

    reg [31:0] data_out_loopback_d;
    always @(posedge clk_pll) begin
        // data_out_loopback_d <= {28'h0, ~USER_LED[3:0]};
        data_out_loopback_d <= {data_gen_0, data_gen_1, data_gen_2, data_gen_3};
    end

    // data generator counter
    always @(posedge clk_pll, negedge reset_)begin
        if(!reset_)begin
            data_gen_0 <= 8'd0;
            data_gen_1 <= 8'd1;
            data_gen_2 <= 8'd2;
            data_gen_3 <= 8'd3;
        end else begin
            data_gen_0 <= data_gen_0 + 4;
            data_gen_1 <= data_gen_1 + 4;
            data_gen_2 <= data_gen_2 + 4;
            data_gen_3 <= data_gen_3 + 4;
        end
    end

    assign DQ = (SLWR_loopback_1d_) ? 32'dz : data_out_loopback_d;

endmodule
