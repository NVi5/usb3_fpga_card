module main (
    // Reset and Clocks
    input         CLK50,
    input         RESET_N,

    // LED and Push Button
    output [3:0]  USER_LED,
    input  [3:0]  PB,

    // FX3
    inout  [31:0] DQ,         // data bus
    output [1:0]  ADDR,       // output fifo address
    output        SLRD,       // output read select
    output        SLWR,       // output write select
    input         FLAGA,      // full flag
    input         FLAGB,      // partial full flag
    input         FLAGC,      // empty flag
    input         FLAGD,      // empty partial flag
    output        SLOE,       // output output enable select
    output        SLCS,       // output chip select
    output        PKEND,      // output pkt end
    output        CLK_OUT     // output clk 100 Mhz and 180 phase shift
 );

assign USER_LED[1:0] = PB[1:0];

reg [1:0] oe_delay_cnt;
reg rd_oe_delay_cnt;
wire [31:0] fifo_data_in;

wire [31:0] data_out;
reg  [31:0] DQ_d;

reg SLRD_loopback_d1_;
reg SLRD_loopback_d2_;
reg SLRD_loopback_d3_;
reg SLRD_loopback_d4_;
reg [1:0]fifo_address;
reg [1:0]fifo_address_d;
reg FLAGA_d;
reg FLAGB_d;
reg FLAGC_d;
reg FLAGD_d;
wire [31:0] data_out_loopback;

reg [3:0]current_loop_back_state;
reg [3:0]next_loop_back_state;
reg SLWR_loopback_1d_;

//parameters for LoopBack mode state machine
parameter [3:0] loop_back_idle                    = 4'd0;
parameter [3:0] loop_back_FLAGC_rcvd              = 4'd1;
parameter [3:0] loop_back_wait_FLAGD              = 4'd2;
parameter [3:0] loop_back_read                    = 4'd3;
parameter [3:0] loop_back_read_rd_and_oe_delay    = 4'd4;
parameter [3:0] loop_back_read_oe_delay           = 4'd5;
parameter [3:0] loop_back_wait_FLAGA              = 4'd6;
parameter [3:0] loop_back_wait_FLAGB              = 4'd7;
parameter [3:0] loop_back_write                   = 4'd8;
parameter [3:0] loop_back_write_wr_delay          = 4'd9;
parameter [3:0] loop_back_flush_fifo              = 4'd10;

//output signal assignment
assign SLRD   = SLRD_loopback_;
assign SLWR   = SLWR_loopback_1d_;
assign ADDR  = fifo_address_d;
assign SLOE   = SLOE_loopback_;
assign SLCS   = 1'b0;
assign PKEND = 1'b1;


//assign clk_100 = clk;        //for TB

//clock generation(pll instantiation)
pll inst_clk_pll
    (
        .areset(/*reset2pll*/1'b0),
        .inclk0(CLK50),
        .c0(clk_100),
        .locked(lock)
    );


//ddr is used to send out the clk(DDR instantiation)

ddr inst_ddr_to_send_clk_to_fx3
        (
    .datain_h(1'b0),
    .datain_l(1'b1),
    .outclock(clk_100),
    .dataout(CLK_OUT)
    );


assign reset_ = lock;

///flopping the INPUTs flags
always @(posedge clk_100, negedge reset_)begin
    if(!reset_)begin
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
assign SLRD_loopback_      = ((current_loop_back_state == loop_back_read) | (current_loop_back_state == loop_back_read_rd_and_oe_delay)) ? 1'b0 : 1'b1;
assign SLOE_loopback_      = ((current_loop_back_state == loop_back_read) | (current_loop_back_state == loop_back_read_rd_and_oe_delay) | (current_loop_back_state == loop_back_read_oe_delay)) ? 1'b0 : 1'b1;
assign SLWR_loopback_      = ((current_loop_back_state == loop_back_write)) ? 1'b0 : 1'b1;

always @(posedge clk_100, negedge reset_)begin
    if(!reset_)begin
        SLWR_loopback_1d_ <= 1'b1;
    end else begin
        SLWR_loopback_1d_ <=  SLWR_loopback_;
    end
end


//delay for reading from slave fifo(data will be available after two clk cycle)
always @(posedge clk_100, negedge reset_)begin
    if(!reset_)begin
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

//flopping the input data
always @(posedge clk_100, negedge reset_)begin
    if(!reset_)begin
        DQ_d <= 32'd0;
     end else begin
        DQ_d <= DQ;
    end
end


//Control signal of internal fifo for LoopBack mode
assign fifo_push  = (SLRD_loopback_d3_ == 1'b0);
assign fifo_pop   = (current_loop_back_state == loop_back_write);
assign fifo_flush = (current_loop_back_state == loop_back_flush_fifo);

assign fifo_data_in = (SLRD_loopback_d3_ == 1'b0) ? DQ_d : 32'd0;

///slave fifo address
always@(*)begin
    if((current_loop_back_state == loop_back_FLAGC_rcvd) | (current_loop_back_state == loop_back_wait_FLAGD) | (current_loop_back_state == loop_back_read) | (current_loop_back_state == loop_back_read_rd_and_oe_delay) | (current_loop_back_state == loop_back_read_oe_delay))begin
        fifo_address = 2'b11;
    end else
        fifo_address = 2'b00;
end

//flopping the output fifo address
always @(posedge clk_100, negedge reset_)begin
    if(!reset_)begin
        fifo_address_d <= 2'd0;
     end else begin
        fifo_address_d <= fifo_address;
    end
end



//counter to delay the read and output enable signal
always @(posedge clk_100, negedge reset_)begin
    if(!reset_)begin
        rd_oe_delay_cnt <= 1'b0;
    end else if(current_loop_back_state == loop_back_read) begin
        rd_oe_delay_cnt <= 1'b1;
        end else if((current_loop_back_state == loop_back_read_rd_and_oe_delay) & (rd_oe_delay_cnt > 1'b0))begin
        rd_oe_delay_cnt <= rd_oe_delay_cnt - 1'b1;
    end else begin
        rd_oe_delay_cnt <= rd_oe_delay_cnt;
    end
end

//Counter to delay the OUTPUT Enable(oe) signal
always @(posedge clk_100, negedge reset_)begin
    if(!reset_)begin
        oe_delay_cnt <= 2'd0;
    end else if(current_loop_back_state == loop_back_read_rd_and_oe_delay) begin
        oe_delay_cnt <= 2'd2;
        end else if((current_loop_back_state == loop_back_read_oe_delay) & (oe_delay_cnt > 1'b0))begin
        oe_delay_cnt <= oe_delay_cnt - 1'b1;
    end else begin
        oe_delay_cnt <= oe_delay_cnt;
    end
end


//LoopBack state machine
always @(posedge clk_100, negedge reset_)begin
    if(!reset_)begin
        current_loop_back_state <= loop_back_idle;
    end else begin
        current_loop_back_state <= next_loop_back_state;
    end
end

//LoopBack mode state machine combo
always @(*)begin
    next_loop_back_state = current_loop_back_state;
    case(current_loop_back_state)
    loop_back_idle:begin
            if(FLAGC_d == 1'b1)begin
            next_loop_back_state = loop_back_FLAGC_rcvd;
        end else begin
            next_loop_back_state = loop_back_idle;
        end
    end
        loop_back_FLAGC_rcvd:begin
        next_loop_back_state = loop_back_wait_FLAGD;
    end
    loop_back_wait_FLAGD:begin
        if(FLAGD_d == 1'b1)begin
            next_loop_back_state = loop_back_read;
        end else begin
            next_loop_back_state = loop_back_wait_FLAGD;
        end
    end
        loop_back_read :begin
                if(FLAGD_d == 1'b0)begin
            next_loop_back_state = loop_back_read_rd_and_oe_delay;
        end else begin
            next_loop_back_state = loop_back_read;
        end
    end
    loop_back_read_rd_and_oe_delay : begin
        if(rd_oe_delay_cnt == 0)begin
            next_loop_back_state = loop_back_read_oe_delay;
        end else begin
            next_loop_back_state = loop_back_read_rd_and_oe_delay;
        end
    end
        loop_back_read_oe_delay : begin
        if(oe_delay_cnt == 0)begin
            next_loop_back_state = loop_back_wait_FLAGA;
        end else begin
            next_loop_back_state = loop_back_read_oe_delay;
        end
    end
    loop_back_wait_FLAGA :begin
        if (FLAGA_d == 1'b1)begin
            next_loop_back_state = loop_back_wait_FLAGB;
        end else begin
            next_loop_back_state = loop_back_wait_FLAGA;
        end
    end
    loop_back_wait_FLAGB :begin
        if (FLAGB_d == 1'b1)begin
            next_loop_back_state = loop_back_write;
        end else begin
            next_loop_back_state = loop_back_wait_FLAGB;
        end
    end
    loop_back_write:begin
        if(FLAGB_d == 1'b0)begin
            next_loop_back_state = loop_back_write_wr_delay;
        end else begin
             next_loop_back_state = loop_back_write;
        end
    end
        loop_back_write_wr_delay:begin
            next_loop_back_state = loop_back_flush_fifo;
    end
    loop_back_flush_fifo:begin
        next_loop_back_state = loop_back_idle;
    end
    endcase
end




///fifo instantiation for loop back mode
fifo fifo_inst(
    .din(fifo_data_in)
        ,.write_busy(fifo_push)
    ,.fifo_full()
    ,.dout(data_out_loopback)
    ,.read_busy(fifo_pop)
    ,.fifo_empty()
    ,.fifo_clk(clk_100)
    ,.reset_(reset_)
    ,.fifo_flush(fifo_flush)
);

reg [31:0]data_out_loopback_d;
always@(posedge clk_100)begin
    data_out_loopback_d <= data_out_loopback;
end

assign DQ = (SLWR_loopback_1d_) ? 32'dz : data_out_loopback_d;

endmodule
