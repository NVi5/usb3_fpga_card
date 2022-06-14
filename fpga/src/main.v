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

endmodule
