#ifndef HWCONSTANTS
#define HWCONSTANTS

#include "common.h"

class HardwareConstants
{
public:
	// if not commented out, python wrapping fails
	// HardwareConstants();
	virtual ~HardwareConstants() = 0;

	// the chip's revision
	virtual uint revision() const = 0;
	virtual uint revision_width() const = 0;
	// the chip's reset is high active aas this corresponds to the
	// power-up state of the controlling FPGA's output pins
	virtual uint rst_active() const = 0;
	virtual uint rst_inactive() const = 0;
	// Width of system time counter,
	// number of event-in and -out buffers
	virtual uint ev_clkpos_width() const = 0;
	// number of 200MHz cycles it takes from an event_valid signal at the event_buffer_outs´
	// fifos to the fifo being not empty at the pop interface
	virtual uint ev_bufout_lat() const = 0;
	// The event out buffers have internal counters to determine the
	// respective MSBs for the current data packet
	// these bits are selected according to the definition of
	// etimemsb_w, below
	// Number of event buffers
	virtual uint event_ins() const = 0;
	virtual uint event_outs() const = 0;
	// number of network_blocks
	// not used
	// virtual uint netwblks() const = 0;
	/*********************************************************/
	// Delay is configured in the following manner:
	// link0 data lines are for
	// * delay line addressing and setting the delay value
	// link1 data lines are for
	// * setting the chip_id
	// the positions are relative to the wires ht_data*
	// of the resp. link.
	// the delay lines are addressed as follows (ascending):
	// * rx_clk0, rx_clk1
	// * rx_word0[0..8], rx_word1[0..8]
	// * tx_word0[0..8], tx_word1[0..8]
	// Latency of the synthesized rx_clk's.
	// Needed for correct behavioral simulation of delaylines
	// Delayline configuration constants:
	// -- only tx clocks are delayed --
	virtual uint ctl_rx0() const = 0;
	virtual uint ctl_rx1() const = 0;
	virtual uint ctl_tx0() const = 0;
	virtual uint ctl_tx1() const = 0;
	// not used
	// virtual uint ctl_txc0() const = 0;
	virtual uint ctl_deldefaultvalue() const = 0; // reset value for the delay chains
	virtual uint ctl_deladdr_pos() const = 0;     // positions relative to ht_datain0
	virtual uint ctl_deladdr_width() const = 0;
	virtual uint ctl_delval_pos() const = 0;
	virtual uint ctl_delval_width() const = 0;
	virtual uint ctl_delcid_pos() const = 0; // positions relative to ht_datain1
	virtual uint ctl_delcid_width() const = 0;
	virtual uint ctl_delnum() const = 0; // number of delay lines to be configured
	// Some constants the JTAG port
	virtual uint jtag_sentinel_val() const = 0; // see comments in spikenet_top.v
	/*********************************************************/
	// Bit Posistions in data packets:
	// identical for all
	virtual uint chipid_pos() const = 0; // the chip's address - in 64 bit packet
	virtual uint chipid_width() const = 0;
	// Data fields containing commands and data payload:
	virtual uint pa_spare() const = 0;  // spare bit position (to address future chips etc. () const
	                                    // = 0;-))
	virtual uint pa_eventp() const = 0; // packet is event data (1) or command (0) - bit's position
	virtual uint pa_pos() const = 0;    // start bit of all (incl. cmd) data payload
	virtual uint pa_width() const = 0;  // complete data payload, also width of compl. event data
	// The idle bits are the two CTL bits of the link,
	// which both are high when the link is idle.
	virtual uint pa_idle0_pos() const = 0;
	virtual uint pa_idle1_pos() const = 0;
	// The idle bits are the two CTL bits of the link,
	// which both are high when the link is idle.
	virtual uint idle0_pos() const = 0;
	virtual uint idle1_pos() const = 0;
	virtual uint ci_pos() const = 0;   // start bit of pure data payload
	virtual uint ci_width() const = 0; // data field after command decoder
	virtual uint ci_msb() const = 0;
	// Common command bits:
	virtual uint ci_cmdrwb_pos() const = 0; // command is read or write
	virtual uint ci_cmd_pos() const = 0;    // the actual command
	virtual uint ci_cmd_width() const = 0;
	// Commands:
	// The LSB of the four command bits is reserved as
	// error flag!
	virtual uint ci_errori() const = 0;
	virtual uint ci_synci() const = 0;
	virtual uint ci_loopbacki() const = 0;
	virtual uint ci_paramrami() const = 0;
	virtual uint ci_controli() const = 0;
	virtual uint ci_synrami() const = 0;
	virtual uint ci_areadouti() const = 0;
	virtual uint ci_evloopbacki() const = 0;
	virtual uint ci_dummycmdi() const = 0;
	virtual uint ci_readi() const = 0; // to make sure nothing is mixed up () const = 0;-)
	virtual uint ci_writei() const = 0;
	// bit positions in error packet
	// first, event errors - positions relative to pa_pos
	virtual uint er_ev_startpos() const = 0; // start of almost full flag status
	virtual uint er_ev_msb() const = 0;      // msb of fifo almost full flags
	// deframers are hardwired selected for being able to sync or not
	// -> ONLY deframer 0 is allowed to sync.
	virtual uint sy_syncen() const = 0; // enable sync mode
	virtual uint sy_nosync() const = 0; // disable
	// The Sync packet
	// these positions are relative to the vectors rx_word in the deframers,
	// which still contain the ctl bit!!
	virtual uint sy_val_pos() const = 0; // position of sync value relative to rx_word
	                                     // during sync, this is the SECOND 16 Bit word
	                                     // clocked into ht_deframer!
	virtual uint sy_val_width() const = 0;
	// Event related stuff ***********************************************++
	virtual uint ev_perpacket() const = 0;    // events per packet
	virtual uint ev_bufaddrwidth() const = 0; // number of address bits per buffer
	virtual uint ev_ibuf_width() const = 0;
	virtual uint ev_obuf_width() const = 0; // There WAS a "+1" for the early out bit!
	// event packet bit-positions
	// these are absolute positions, as the data is directly multiplexed to
	// the event buffers
	// e means event, the rest is:
	// time:  absolute time, msb for all three and lsb for each single one
	// tb:    time bin relative to time
	// na:    neuron address
	// valid: event data valid
	virtual uint ev_timemsb_pos() const = 0;
	virtual uint ev_timemsb_width() const = 0;
	virtual uint ev_time0lsb_pos() const = 0;
	virtual uint ev_time1lsb_pos() const = 0;
	virtual uint ev_time2lsb_pos() const = 0;
	virtual uint ev_timelsb_width() const = 0;
	virtual uint ev_tb0_pos() const = 0;
	virtual uint ev_tb1_pos() const = 0;
	virtual uint ev_tb2_pos() const = 0;
	virtual uint ev_tb_width() const = 0;
	virtual uint ev_na0_pos() const = 0;
	virtual uint ev_na1_pos() const = 0;
	virtual uint ev_na2_pos() const = 0;
	virtual uint ev_na_width() const = 0;
	virtual uint ev_0valid() const = 0;
	virtual uint ev_1valid() const = 0;
	virtual uint ev_2valid() const = 0;
	// parameter ram access bit positions:
	// these are relative positions to ci_pos
	// (see definition of sc_width, above!)
	virtual uint pr_cmd_pos() const = 0;    // start position of param ram command
	virtual uint pr_cmd_width() const = 0;  // width of param ram command
	virtual uint pr_cmd_ram() const = 0;    // access parameter sram
	virtual uint pr_cmd_period() const = 0; // access voutclkx period register
	virtual uint pr_cmd_lut() const = 0;    // access lookup table
	virtual uint pr_cmd_num() const = 0;    // access address counter stop value (actual number of
	                                        // parameters)
	//$ command data fields
	//$ cmd period
	virtual uint pr_period_pos() const = 0; // position of period value
	virtual uint pr_period_width() const = 0;
	virtual uint pr_num_pos() const = 0; // start position of number of parameters
	//$ cmd lookup
	virtual uint pr_lutadr_depth() const = 0;
	virtual uint pr_luttime_pos() const = 0; // time value to be written into lut
	virtual uint pr_luttime_width() const = 0;
	virtual uint pr_lutboost_pos() const = 0; // boost time exponent to be written into lut
	virtual uint pr_lutboost_width() const = 0;
	//$ additional entries for repeat and step size
	virtual uint pr_lutrepeat_pos() const = 0; //$ time exponent to be written into lut
	virtual uint pr_lutrepeat_width() const = 0;
	virtual uint pr_lutstep_pos() const = 0; //$ boost time value to be written into lut
	virtual uint pr_lutstep_width() const = 0;
	virtual uint pr_lutadr_pos() const = 0; // address of lookup table
	virtual uint pr_lutadr_width() const = 0;
	virtual uint pr_boostmul() const = 0; //$ boost current multiplicator
	//$command parameter sram
	virtual uint pr_ramaddr_pos() const = 0; // address in parameter ram
	virtual uint pr_ramaddr_width() const = 0;
	virtual uint pr_paraddr_pos() const = 0; // address on anablock
	virtual uint pr_paraddr_width() const = 0;
	virtual uint pr_dacval_pos() const = 0; // DAC value
	virtual uint pr_dacval_width() const = 0;
	virtual uint pr_curaddr_width() const = 0; // address lines to analog block
	virtual uint pr_synparams() const = 0;     // number of parameters
	virtual uint pr_neuronparams() const = 0;  // changed! each neuron decodes 2 lsb, not 1
	// only 20 outputs of each curmem_vout_block are used. Each one
	// needs 2 parameters.
	// problem -> test outputs and uncontrolled drifting
	// changed to 128
	virtual uint pr_voutparams() const = 0;
	virtual uint pr_biasparams() const = 0;
	virtual uint pr_outampparams() const = 0;
	// default values for initial behavior: 20 clocks
	virtual uint pr_defboostcount() const = 0; //$boost 50 percent
	virtual uint pr_defcount() const = 0;
	virtual uint pr_defparamvalue() const = 0; //$default parameter value:10%
	                                           // $total number of parameters minus 1
	// for simulation we can set the total value of parameters to a low value
	// not used!
	// virtual uint pr_numparameters() const = 0;
	// start addresses in the parameter ram
	virtual uint pr_adr_syn0start() const = 0;
	virtual uint pr_adr_syn1start() const = 0;
	virtual uint pr_adr_neuron0start() const = 0;
	virtual uint pr_adr_neuron1start() const = 0;
	virtual uint pr_adr_voutlstart() const = 0;
	virtual uint pr_adr_voutrstart() const = 0;
	virtual uint pr_adr_biasstart() const = 0;
	virtual uint pr_adr_outampstart() const = 0;
	// the loopback register
	virtual uint lb_startpos() const = 0; // the full data content of one packet
	virtual uint lb_msb() const = 0;      // can be used for loopback
	// the control register:
	// resetvalue:
	// `cr_eout_depth_resval<<`cr_eout_depth |
	// (event_outs)<<cr_eout_rst | (event_ins)<<cr_einb_rst | (event_ins)<<cr_ein_rst
	// rest has reset value 0.
	virtual uint cr_sel_pos() const = 0;       // start position of Bits that select
	                                           // the status register to readback
	virtual uint cr_sel_width() const = 0;     // 4 registers - 2 Bits :-)
	virtual uint cr_sel_control() const = 0;   // read control register and revision
	virtual uint cr_sel_clkstat() const = 0;   // read clk fifo status and clk_pos
	virtual uint cr_sel_clkbstat() const = 0;  // read clkb fifo status and clkb_pos
	virtual uint cr_sel_clkhistat() const = 0; // read clkhi fifo status, clkhi_pos,
	                                           // hnibble of event packet time and pll_locked
	virtual uint cr_pos() const = 0;   // start position of control register content, relative to
	                                   // ci_pos.
	                                   // on read, status register content also starts from here
	virtual uint cr_width() const = 0; // width of control register
	// the following bit positions are relative to the control register!
	virtual uint cr_err_out_en() const = 0; // high active enable for instantaneous event error
	                                        // packet generation
	virtual uint cr_earlydebug() const = 0; // Flag for EarlyOut debug mode
	virtual uint cr_anaclken() const = 0;   // Analog Block clk200 and clk200b enable
	virtual uint cr_anaclkhien() const = 0; // Analog Block clk400 enable
	virtual uint cr_pram_en() const = 0;    // parameter ram fsm enable bit
	virtual uint cr_ein_rst() const = 0;    // first bit of the evt_clk buffer in resets (reset is
	                                        // ACTIVE HIGH)
	virtual uint cr_einb_rst() const = 0;   // first bit of the evt_clkb buffer in resets (reset is
	                                        // ACTIVE HIGH)
	virtual uint cr_eout_rst() const = 0;   // first bit of the event buffer out resets (reset is
	                                        // ACTIVE HIGH)
	virtual uint cr_eout_depth() const = 0; // spikey4: first bit of almost full value reg for
	                                        // event_buffer_outs
	virtual uint cr_eout_depth_width() const = 0;  // spikey4: width of almost full value reg for
	                                               // event_buffer_outs
	virtual uint cr_eout_depth_resval() const = 0; // spikey4: reset val (125, eq. max-1) for
	                                               // event_buffer_out fifo depth
	// status of the event buffers is assembled in three vectors. One for the
	// evt_clk, evt_clkb and clkhi buffers each.
	// Each status consists of 4Bit which have the following meaning:
	virtual uint cr_fifofbit() const = 0;   // fifo full flag
	virtual uint cr_fifoafbit() const = 0;  // fifo almost full flag
	virtual uint cr_fifohfbit() const = 0;  // fifo half full flag
	virtual uint cr_fifoerrbit() const = 0; // fifo error flag
	// besides the control register, three different "status registers" can be read
	// from the chip. The content is described above in the command definition
	// Following are the widths of the registers and the start posisions of the
	// contained values:
	virtual uint cr_crread_width() const = 0;    // upon read, the revision number is tranmitted
	virtual uint cr_clkstat_width() const = 0;   //;*4Bit status,() const = 0;Bit clk_pos
	virtual uint cr_clkpos_pos() const = 0;      // first the status, clk_pos starts here
	virtual uint cr_clkhistat_width() const = 0; //;*4Bit status,;Bit clkhi_pos,;Bit
	                                             //evpacket_hinibble,() const = 0;Bit pll_locked
	virtual uint cr_clkhipos_pos() const = 0;    // first the status, then clkhi_pos
	virtual uint cr_clkhi_hin_pos() const = 0;   // start pos of value of event_buf_out hinibble
	                                             // counter
	virtual uint cr_clkhi_pll_pls() const = 0;   // position of the pll_locked bit
	// **** synapse control module constants ****
	virtual uint sc_commandwidth() const = 0;
	// status and control
	virtual uint sc_cmd_ctrl() const = 0; // control and status
	virtual uint sc_cmd_time() const = 0; // timing parameters
	// plasticity lookup-table commands
	virtual uint sc_cmd_plut() const = 0; // lookup
	// ram read/write commands
	virtual uint sc_cmd_syn() const = 0; // synapse weigths and row/col control data (col data can
	                                     // only be written)
	virtual uint sc_cmd_close() const = 0; // close open synapse row (above commands leave row open)
	// correlation commands
	virtual uint sc_cmd_cor_read() const = 0; // read correlation senseamp outputs
	virtual uint sc_cmd_pcor() const = 0; // process correlation for one row
	virtual uint sc_cmd_pcorc() const = 0; // process correlation for one row
	// all the following field definitions start by ci_pos+sc_commandwidth
	// **** common definitons for all address/data fields: data|address|cmd|buscmd
	virtual uint sc_aw() const = 0; // max address width
	virtual uint sc_dw() const = 0; // data width 4* #of64blocks
	virtual uint sc_rowconfigbit() const = 0; // selects row config bits, only row address valid ->
	                                          // ~colmodecore
	virtual uint sc_neuronconfigbit() const = 0; // selects neuron config bits, only column address
	                                             // valid -> ~rowmodecore
	virtual uint sc_plut_res() const = 0; // resolution of correlation lookup table in bits
	virtual uint sc_colmsb() const = 0; // number of columns (physical chip parameter)
	virtual uint sc_rowmsb() const = 0; // dito, rows (physical chip parameter)
	virtual uint sc_cprienc() const = 0; // number of correlation priority encoders
	// **** time register ****
	// sc_pcorperiod|sc_pcsecdelay|sc_csensedelay|cmd|buscmd
	virtual uint sc_csensedelaymsb() const = 0; // 8 bit field
	virtual uint sc_pcsecdelaymsb() const = 0;  //
	virtual uint sc_pcorperiodmsb() const = 0;  // (minimum) time interval between auto pcor rows
	virtual uint sc_pcreadlenpos() const = 0;   // if one, read takes 2 cycles instead of 1 in pcor
	virtual uint sc_tregw() const = 0; // number of valid bits in treg register
	// **** control and status register ****
	virtual uint sc_csregmsb() const = 0;
	virtual uint sc_sreg_cmerr() const = 0;   // error in command decoding
	virtual uint sc_sreg_done() const = 0;    // done status
	virtual uint sc_sreg_pcauto() const = 0;  // auto correlation processing running
	virtual uint sc_sreg_pccont() const = 0;  // continuous auto correlation processing
	virtual uint sc_sreg_dllres() const = 0;  // set dll charge pump voltage to Vdllreset
	virtual uint sc_sreg_nresetb() const = 0; // zero holds all neurons in reset state -> prevents
	                                          // firing
	virtual uint sc_sreg_row() const = 0;     // actual row processed
	virtual uint sc_sreg_start() const = 0;   // start row
	virtual uint sc_sreg_stop() const = 0;    // stop row
	virtual uint sc_sreg_rowmsb() const = 0;  // 8 bits field width for rows
	// **** column (neuron) config data ****
	virtual uint sc_ncd_evout() const = 0; // enables event out
	virtual uint sc_ncd_outamp() const = 0; // enables membrane voltage out
	// **** row config data definitons (synapse drive data) ****
	virtual uint sc_sdd_is0() const = 0; // input select (0-3): evin,in<1>,prev,in<0>
	virtual uint sc_sdd_is1() const = 0;
	virtual uint sc_sdd_senx() const = 0;
	virtual uint sc_sdd_seni() const = 0;
	virtual uint sc_sdd_enstdf() const = 0;
	virtual uint sc_sdd_dep() const = 0;
	virtual uint sc_sdd_cap2() const = 0;
	virtual uint sc_sdd_cap4() const = 0;
	virtual uint sc_blockshift() const = 0; // shift for block 1
	// (is1:is0) definitions
	virtual uint sc_sdd_evin() const = 0; // event in from event fifo
	virtual uint sc_sdd_in1() const = 0; // adjacent block feedback
	virtual uint sc_sdd_prev() const = 0; // input of previous row, only used for every second row
	                                      // (e.g. 1 uses same input than 0)
	virtual uint sc_sdd_in0() const = 0; // same block feedback
	// **** analog read out chain control interface ****
	virtual uint ar_numchains() const = 0; // number of implemented chains
	virtual uint ar_voutl() const = 0; // chain definitions
	virtual uint ar_voutr() const = 0;
	virtual uint ar_maxlen() const = 0; // longest chain used
	virtual uint ar_chainnumberwidth() const = 0; // width of chain number field in din
	virtual uint ar_chainlen() const = 0; // width of data field, equals maximum chain length
	virtual uint ar_numvouts() const = 0; // this the real number of vout cells per block
	// ****event loopback module control interface ****
	virtual uint el_depth() const = 0;            // depth of loopback chain
	virtual uint el_enable_pos() const = 0;       // event loopback enable bit
	virtual uint el_hibufsel_clk_pos() const = 0; // if 1, selects input buffers 3 and 7 to output
	                                              // buffers 2 and 5 in clk domain
	// if 0, selects input buffers 2 and 6 to output buffers 2 and 5
	virtual uint el_hibufsel_clkb_pos() const = 0; // same as above for clkb domain
	virtual uint el_earlyout_pos() const = 0; // start position of earlyout bits to apply to the
	                                          // even_out_buffers
	                                          // give `event_outs number of bits!
	// this constant is not derived from verilog headers
	virtual uint el_offset() const = 0;

	/**********************
	 * Spikeyglobal stuff
	 **********************/

	// event interface definitions

	virtual uint sg_eventw() const = 0; //  maximum width to fit in 1/3 of a 64 bit field

	virtual uint sg_numoutports() const = 0; //  number of event ports

	virtual uint sg_numevoutfifos() const = 0; //  number of fifos to build event packets from

	virtual uint sg_numinports() const = 0; //  number of event ports

	// #### event port specification ####

	// these values are defined separately to be independent of spikenet_base definitions.

	virtual uint sg_systimewidth() const = 0; //  width of fpga system time counter

	virtual uint sg_eadrwidth() const = 0; //  neuron address

	virtual uint sg_etimewidth() const = 0; //  event time (400MHz counter)

	virtual uint sg_efinewidth() const = 0; //  event time within 400MHz cycle

	virtual uint sg_datawidth() const = 0; //  total width of event data field

	virtual uint sg_evtimeout() const = 0; //  write fpga time to pbmem if no events after this # of
	                                       //  cycles (200MHz cycles)

	//											 value (400MHz):	128 - max difference in event latency when fifos
	//empty.

	//																															max difference is 16 + 2 cycles
	//for sync uncertainty.

	virtual uint sg_evtimeoutw() const = 0; //  width of timeout counter

	// ###################################################################################

	// control interface definitions

	virtual uint sg_latcnt_width() const = 0; //  width of chain latency counter

	virtual uint sg_numchips() const = 0; //  number of chips in chain

	virtual uint sg_fpga_time_msb() const = 0; //  msb of system time counter

	//	 maximum round trip delay in 200MHz cycles for one packet

	virtual uint sg_chain_latency() const = 0; //  for single chip!

	//	 max count of received error packets until timeout

	virtual uint sg_ci_maxerrors() const = 0; //  !!! review value !!!

	virtual uint sg_ci_maxerr_w() const = 0; //
	virtual uint sg_lat2csync() const = 0;   //  #400MHz cycles from controller sync (FPGA starts
	                                         //  counting) to spikey sync (spikey starts counting)

	virtual uint sg_latrd2cpreg() const = 0; //  #400MHz cycles from read command (controller) to
	                                         //  clkhipos_reg valid (spikey)

	virtual uint sg_parwpos() const = 0; // rw bit in pa_ci_packet (ci_width+cmd+rw)

	virtual uint sg_sdram_wtim_max() const = 0; //  max number of cycles the sdram will need to
	                                            //  empty the write fifo.

	//	 besides sync (command: ci_synci) the controller suppoprts these commands that are encoded

	//	 within the command field of the playback memory entries

	virtual uint sg_ci_chipid() const = 0; //  set/change the controller's chipid register

	virtual uint sg_ci_chipid_pos() const = 0; //  position of new chipid within sci_dataout

	// ###################################################################################

	// Slow Control interface definitions

	virtual uint sg_cm_spikey() const = 0; //  code module number, no better place yet

	//	 MSB of Slow Control addresses related to spikey

	virtual uint sg_scaddrmsb() const = 0; //  distinguishes between access to spikey_control or
	                                       //  spikey_sei

	//	 The following Slow Control addresses are mapped to commands for spikey access:

	virtual uint sg_scmode() const = 0; //  set/get spikey's mode/reset pins

	virtual uint sg_scdirect0() const = 0; //  read/write lower 18bit of sci_dataxxx

	virtual uint sg_scdirect1() const = 0; //
	virtual uint sg_scdirect2() const = 0; //
	virtual uint sg_scdirect3() const = 0; //
	virtual uint sg_scfire() const = 0;    //  pulse spikey's firein lines

	virtual uint sg_scdelay() const = 0; //  set spikey's delay lines

	virtual uint sg_scsendidle() const = 0; //  idle event packets on/off

	virtual uint sg_scclkphase() const = 0; //  rx clock pahse selection

	virtual uint sg_scevtimeout() const = 0; //  reset value of event system time timeout counter

	virtual uint sg_scrxevstat() const = 0; //  read received event packet "statistics"

	virtual uint sg_scanamux() const = 0; //  set select bits for analog multiplexer

	virtual uint sg_scleds() const = 0; //  set pins connected to LEDs on recha2 :-)

	virtual uint sg_scphsel() const = 0; // set phase selection bits of input logic

	//	 constants regarding mode/reset pins

	virtual uint sg_scrst_pos() const = 0; //  position of reset pin's value within sc.data_in field

	virtual uint sg_sccim_pos() const = 0; //  position of ci_mode

	virtual uint sg_scdirectout() const = 0; //  position of direct packet out

	virtual uint sg_scbsm_pos() const = 0; //  position of bs_mode

	virtual uint sg_scplr_pos() const = 0; //  position of pll_reset

	virtual uint sg_scplb_pos() const = 0; //  position of pll_bypass

	virtual uint sg_scpll_pos() const = 0; //  position of pll_locked signal (for read access only!)

	virtual uint sg_scsye_pos() const = 0; //  position of sync error flag

	virtual uint sg_scvrest_pos() const = 0;
	virtual uint sg_scvm_pos() const = 0;
	virtual uint sg_scibtest_pos() const = 0;
	virtual uint sg_scpower_pos() const = 0;
	virtual uint sg_rxd0_phsel_pos() const = 0;
	virtual uint sg_rxd1_phsel_pos() const = 0;
	virtual uint sg_rxc0_phsel_pos() const = 0;
	virtual uint sg_rxc1_phsel_pos() const = 0;

	//	 constants regarding direct link access (relative to sc.data_in):

	virtual uint sg_direct_msb() const = 0; //  msb of direct in/out bus. 8bit data plus 1bit ctl x2

	virtual uint sg_direct_pos() const = 0; //  position of link direct in/out data within

	//								 Slow Control data field (t_sc_module.data_xxx)

	//	 constants regarding firein lines access

	virtual uint sg_scfil_msb() const = 0; //  fireinleft bus msb

	virtual uint sg_scfil_pos() const = 0; //  fireinleft bits start position within sc.data_in
	                                       //  field

	virtual uint sg_scfir_msb() const = 0; //  fireinright bus msb

	virtual uint sg_scfir_pos() const = 0; //  fireinright bits start position within sc.data_in
	                                       //  field

	virtual uint sg_scfil_late_pos() const = 0; //  fireins with late-bit will be sent to spikey

	virtual uint sg_scfir_late_pos() const = 0; //  after firein late delay

	virtual uint sg_scfidel_pos() const = 0; //  delay for firein late signals -> distance between
	                                         //  rising edges in sysclock/2 cycles

	virtual uint sg_scfidel_msb() const = 0; //  delay 4 means two cycles from early to late pulse

	//	 constants regarding delay line configuration (relative to sc.data_in):

	virtual uint sg_scdeladdr_pos() const = 0; //  address of delay line to be written

	virtual uint sg_scdelval_pos() const = 0; //  value to be written

	virtual uint sg_scdelcid_pos() const = 0; //  chip id

	//	 constants regarding config and status of Slow Control access (spikey_control)

	virtual uint sg_scevidle_pos() const = 0; //  position of idle packet type control bit

	virtual uint sg_sc1evidle_pos() const = 0; //  position for "send one idle event packet" bit

	virtual uint sg_scsyncerr_pos() const = 0; //  position of sync error flag

	virtual uint sg_scckph0_pos() const = 0; //  position of phase sel. bit within sc data

	virtual uint sg_scckph1_pos() const = 0; //
	//	 constants regarding event packet stats

	virtual uint sg_rxeverr_width() const = 0; //  number of received event error packets

	virtual uint sg_rxeverr_pos() const = 0; //
	//	 number of clk-cycles to wait after C_DELAY or delay line address/value has been changed

	virtual uint sg_dcapplycnt() const = 0; //
	//	 ### begin obsolete (these constants are only there for compatibility with software

	//	 and will be deleted in coming revisions!) ###

	virtual uint sg_scdin0() const = 0; //  get packet data from link 0

	virtual uint sg_scdin1() const = 0; //  get packet data from link 1

	virtual uint sg_scramdebug() const = 0; //  playback ram debug mode on/off

	virtual uint sg_scscevents() const = 0; //  read number of events scheduled by spikey_sei

	virtual uint sg_sctxevents() const = 0; //  read number of events transmitted by spikey_control

	virtual uint sg_sceventcw() const = 0; //  width of transmitted event counter

	virtual uint sg_scramdebug_pos() const = 0; //  playback ram debug flag.

	virtual uint sg_rxevnum_width() const = 0; //  number of received event packets

	virtual uint sg_rxevnum_pos() const = 0; //
	//	 ### end obsolete ###

	// ###################################################################################

	// cycles to wait for ramclient after pb mem start. -> Makes sure there's nor refresh at the
	// beginning of pbmem cycle.

	virtual uint sg_pfwait() const = 0; //
	// ram command definitions

	virtual uint sg_ev_comw() const = 0; //  number of command bits

	virtual uint sg_ev_rdcom_ci() const = 0; //  ci rw command

	virtual uint sg_ev_rdcom_ec() const = 0; //  event command

	virtual uint sg_ev_evsize() const = 0; //  event fields

	virtual uint sg_ev_evbase() const = 0; //  lsb of first event

	virtual uint sg_ev_citimew() const = 0; //  ci command time field

	virtual uint sg_ev_citime() const = 0;  //
	virtual uint sg_ev_cidataw() const = 0; //  ci command data filed + command +rw

	virtual uint sg_ev_cidata() const = 0; //
	virtual uint sg_ev_timew() const = 0;  //  ev command time field

	virtual uint sg_ev_time() const = 0;   //
	virtual uint sg_ev_numevw() const = 0; //  ev command number of event words

	virtual uint sg_ev_numev() const = 0;   //
	virtual uint sg_ev_evmaskw() const = 0; //  ev command mask for last event word

	virtual uint sg_ev_evmask() const = 0; //
	// field widths slow control configuration

	virtual uint sg_adw() const = 0; // address bits used as data for ci acces

	virtual uint sg_sc_numreadw() const = 0; //
	virtual uint sg_sc_radrw() const = 0;    //  max ram

	virtual uint sg_sc_wadrw() const = 0; // (ag): DO NOT increase over 28 because otherwise the
	                                      // below status bits will conflict!

	virtual uint sg_sc_wtimw() const = 0; // (ag): width of wait time counter

	virtual uint sg_sc_syncsw() const = 0; // width of number-of-syncs counter

	virtual uint sg_sc_wtim() const = 0; // (ag): pos of wtimis0 bit in sc_dout vector

	virtual uint sg_sc_isidle() const = 0; // (ag): pos of pbmem fsm idle status bit

	virtual uint sg_sc_read_empty() const = 0; // (ag): pos of bit indicating that read fifo empty
	                                           // occured

	virtual uint sg_sc_write_inh() const = 0; // (ag): pos of bit indicating that write fifo inhibit
	                                          // occured

	virtual uint sg_sc_write_full() const = 0; // (ag): pos of bit indicating that write memory ran
	                                           // full

	virtual uint sg_sc_chipidw() const = 0; //
	virtual uint sg_sc_statusw() const = 0; //
	virtual uint sg_sc_wdataw() const = 0;  //  sc read remaining ci write data width

	virtual uint sg_sb_start_ramsm() const = 0; //  bit number for start_ramsm

	virtual uint sg_sb_reset_ramsm() const = 0; // (ag): bit number for reset_ramsm

	virtual uint sg_sb_read_empty() const = 0; // (ag): indicates that read fifo empty occured

	virtual uint sg_sb_write_inh() const = 0; // (ag): indicates that write fifo inhibit occured

	virtual uint sg_sb_write_full() const = 0; // (ag): indicates that write memory ran full

	// slow control addresses

	virtual uint sg_sc_addrw() const = 0; //  number of address bits

	virtual uint sg_sc_status() const = 0; //  status register

	virtual uint sg_sc_numread() const = 0; //  number of ram words to read

	virtual uint sg_sc_radr() const = 0; //  ram start adr

	virtual uint sg_sc_chipid() const = 0; //  chip id to use

	virtual uint sg_sc_rdata() const = 0; //  sc read remaining ci read data

	virtual uint sg_sc_wdata() const = 0; //  sc read remaining ci write data

	virtual uint sg_sc_wadr() const = 0; //  sc address for ram write port

	virtual uint sg_sc_rradr() const = 0; //  read back current read address

	virtual uint sg_sc_rwadr() const = 0; //  read back current write address

	virtual uint sg_sc_prefcnt() const = 0; //  read/write prefetch counter load value

	virtual uint sg_sc_cten() const = 0; //  read/write common trigger enable

	virtual uint sg_sc_syncs() const = 0; // read back number of executed sync commands (ag, added
	                                      // on 2014/05/05)

	// constants regarding analog multiplexer and leds on recha2

	virtual uint sg_scibtestpos() const = 0; //  bit position of ibtest enable

	virtual uint sg_scanamuxenpos() const = 0; //  bit 0 of analog multiplexer select input

	virtual uint sg_scanamuxenw() const = 0; //  width of analog multiplexer select input

	virtual uint sg_scaddled1pos() const = 0; //  bit position of add. led 1 on recha (all leds low
	                                          //  active)

	virtual uint sg_scaddled2pos() const = 0; //  bit position of add. led 2 on recha (all leds low
	                                          //  active)

	virtual uint sg_scplexiledpos() const = 0; //  led on plexi cover edge

	virtual uint sg_scchipledpos() const = 0; //  led near spikey chip

	// constants regarding analog multiplexer on flyspi-spikey-v2

	virtual uint sg_sc_muxw() const = 0; // width of each of the mux'es inputs

	virtual uint sg_sc_mux10m_pos() const = 0; // select signals for muxing outamp<1:0> and calib
	                                           // input

	virtual uint sg_sc_mux432_pos() const = 0; // etc.

	virtual uint sg_sc_mux765_pos() const = 0;

	// not really constant, but measured values

	virtual uint fifo_reset_delay() const = 0; //  clock cycles to wait until FIFOs are reset
	                                           //  (because analog reset, not aligned to clock)
};

#endif
