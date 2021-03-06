module xbar_control (
input ebi_clk, //System clock
input sclk, //XBAR state clock (but not the clock that goes to the XBAR)
input reset,
input enable,
input re,
input wr,
input [15:0] data,
output reg [15:0] data_out,
input [18:0] addr,
//facing the DAC
output reg xbar_clock, //clock going to xbar
output reg pclk, //keep low until we're done, single pulse will set transistors in xbar.
output sin);

//Controller select logic. 
parameter POSITION = 0;
wire cs;
assign cs = (enable & (addr[18:8] == POSITION));
wire busy_fast;
assign busy_fast = (command != 0);


//wire high_addr = (addr[4:0]<<4)+15;
//wire low_addr = (addr[4:0]<<4);

localparam [7:0] 
  PROGRAM  = 8'h04,
  OVERFLOW = 8'h01,
  DIVIDE   = 8'h02,
  SAMPLE   = 8'h03,
  ID_REG   = 8'h09,
  BUSY     = 8'h0A;

//EBI data capture
reg [15:0] ebi_captured_data [0:31];
reg [15:0] command = 0;

integer i;
always @ (posedge ebi_clk) 
begin
  if (reset) begin
    //ebi_captured_data <= 0;
    for (i = 0; i < 32; i = i + 1) begin
      ebi_captured_data[i] <= 0; //offset into ebi big register since we only tx 2 bytes at a time.
    end

  end else begin
    if (cs & wr) begin
      if (addr[7:5] == 3'b001) begin
        command <= data;
      end else begin
        //for (i = 0; i < 32; i = i + 1) begin
        ebi_captured_data[addr[4:0]] <= data; //offset into ebi big register since we only tx 2 bytes at a time.
        //end
      end
    end

    if (cs & re) begin
      if(addr[7:0] == BUSY) 
        data_out <= {14'b0, busy_fast, busy};
      if(addr[7:0] == ID_REG)
        data_out <= 16'h7ba2; //kinda looks like 'XbaR'... no?
    end else begin
      data_out <= 0;
    end

    if (busy) begin
      command <= 0;
    end
  end
end


//Control logic
//------------------------------------------------------------------------------------
reg [8:0] counter = 0;  //clock out 511 pulses.
reg shift_out_enable;
reg load_shift_reg;
reg count_up;
reg count_res;
reg busy;

parameter init =            5'b00001;
parameter load =            5'b00010;
parameter pulse_pclk =      5'b00100;
parameter pulse_xbar_clk =  5'b01000;
parameter load_shift =      5'b10000;

reg[4:0] state;

initial begin
  state <= init;
end

//State transitions.
always @ (posedge sclk) begin
  if (reset) begin
    state <= init;
  end else begin
    case (state) 
      init: begin
        state <= init;
        if (command != 0) begin
          state <= load_shift;
        end
      end

      load: 
        state <= pulse_xbar_clk;

      pulse_xbar_clk: begin
        state <= load;
        if (counter == 511) 
          state <= pulse_pclk;
        else if (counter[3:0] == 15)
          state <= load_shift;
      end

      load_shift:
        state <= load;

      pulse_pclk:
        state <= init;

      default:
        state <= init;

    endcase
  end //reset if end
end //always end

//Output functions.
always @ (*) begin
/*
if (reset) begin
      shift_out_enable <= 1'b0;
      count_up <= 1'b0;
      count_res <= 1'b1;
      xbar_clock <= 1'b0;
      pclk <= 1'b1;
end else begin
  */
  //State machine start.
  case (state)
    //Idle / init, waiting for something to do.
    init: begin
      shift_out_enable <= 1'b0;
      count_up <= 1'b0;
      count_res <= 1'b1;
      xbar_clock <= 1'b0;
      pclk <= 1'b1;
      load_shift_reg <= 1'b0;
      busy <= 1'b0;
    end

    load: begin
      shift_out_enable <= 1'b0;
      count_up <= 1'b0;
      count_res <= 1'b0;
      xbar_clock <= 1'b0;
      pclk <= 1'b1;
      load_shift_reg <= 1'b0;
      busy <= 1'b1;
    end

    load_shift: begin
      shift_out_enable <= 1'b0;
      count_up <= 1'b0;
      count_res <= 1'b0;
      xbar_clock <= 1'b0;
      pclk <= 1'b1;
      load_shift_reg <= 1'b1;
      busy <= 1'b1;
    end

    pulse_pclk: begin
      shift_out_enable <= 1'b0;
      count_up <= 1'b0;
      count_res <= 1'b1;
      xbar_clock <= 1'b0;
      pclk <= 1'b0; //give a pulse.
      load_shift_reg <= 1'b0;
      busy <= 1'b1;
    end

    pulse_xbar_clk: begin
      shift_out_enable <= 1'b1;  //shift one bit on next flank
      count_up <= 1'b1; //count up on next flank.
      count_res <= 1'b0;
      xbar_clock <= 1'b1;  //output, give xbar a flank now, [15] of shift reg is stable.
      pclk <= 1'b1;
      load_shift_reg <= 1'b0;
      busy <= 1'b1;
    end

    default: begin
      shift_out_enable <= 1'b0;
      count_up <= 1'b0;
      count_res <= 1'b0;
      xbar_clock <= 1'b0;
      load_shift_reg <= 1'b0;
      pclk <= 1'b1;
      busy <= 1'b1;
      end

    endcase
//  end
end


//Data path
//------------------------------------------------------------------------------------
reg [15:0] shift_out_register = 0;
always @ (posedge sclk) begin
  if (reset) 
    shift_out_register <= 0;
  else begin
    //Reset counter if asked to.
    if (count_res)
      counter <= 0;
    else 
      if (count_up)
        counter <= counter + 1;

    if (load_shift_reg)
      shift_out_register <= ebi_captured_data[counter[8:4]];
    else
      if (shift_out_enable)
        shift_out_register <= {shift_out_register[14:0], 1'b0};
  end
end

assign sin = shift_out_register[15];

endmodule
