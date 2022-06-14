module  hello_world (

    // Reset and Clocks
    input              SYS_CLK50M,
    input              RESET_EXPN,

    // LED and Push Button
    output    [3:0]    USER_LED,
    input     [3:0]    PB
    
    );

    assign USER_LED[1:0] = PB[1:0];

endmodule
