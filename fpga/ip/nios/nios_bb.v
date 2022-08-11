
module nios (
	clk_clk,
	data_in_export,
	reset_reset_n,
	state_in_export);	

	input		clk_clk;
	input	[31:0]	data_in_export;
	input		reset_reset_n;
	input	[3:0]	state_in_export;
endmodule
