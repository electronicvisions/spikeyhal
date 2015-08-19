// Global testbanch parameters
// the global timescale - recall in each module by ""`tscale"
// timing
#define tim_ctoff 0.25 // clock to output typical internal flipflop
// instead of using library mappings in ModelSim, use these macros
// these are supported by Verilog-XL
#define SPIKENET dir =../ src
#define SPIKEN_DSIM dir = / kip / home31 / schemmel / chip / src / lib / sim
#define CORECELLS                                                                                  \
	dir = / cad / libs / umc_kit1 .2 / VST / UMC18 / CORECELLS / verilog_simulation_models
#define CORECELLS_UDP                                                                              \
	file = / cad / libs / umc_kit1 .2 / VST / UMC18 / CORECELLS / verilog_simulation_models / UDPS.v
#define MACROS dir =../ src / lib / macros
#define BLOCKS dir =../ src / lib / blocks
#define UMCOLD dir = / cad / libs / umc_kit1 .0 / CORECELLS / verilog_simulation_models
#define UMCOLD_UDP                                                                                 \
	file = / cad / libs / umc_kit1 .0 / CORECELLS / verilog_simulation_models / UDPS.v
#define IOSTCELLS                                                                                  \
	dir = / cad / libs / umc_kit1 .2 / VST / UMC18 / IOSTCELLS / verilog_simulation_models
#define IOCELLS dir = / cad / libs / umc_kit1 .2 / VST / UMC18 / IOCELLS / verilog_simulation_models
#define SYNOPSYS dir = / cad / products / synopsys / syn_vV - 2004.06 - SP1 / dw / sim_ver
#define XILINXF                                                                                    \
	file = / cad / products / xilinx / xilinx6 .2i / coregen / ip / xilinx / primary / com /       \
	       xilinx / ip / async_fifo_v5_1 / simulation / ASYNC_FIFO_V5_1.v
#define XILINXD dir = / cad / products / xilinx / xilinx6 .2i / verilog / src / unisims
// this was moved into the verilog_mixed command file
// sdf file must be created from vst_instances.sdf:
// perl patch_sdf.pl vst_instances.sdf vst_instances_xl.sdf
#define sdf_spikenet_top "../../../chip/sim/vst_instances_xl.sdf"
// for backanno
#define sdf_ba_spikenet_chip "./spikenet_chip_final_patch_wc.sdf"
// general global constants
#define logic0 1b0
#define logic1 1b1
// the chip's revision
#define revision 3
#define revision_width 4
// the chip's reset is high active aas this corresponds to the
// power-up state of the controlling FPGA's output pins
#define rst_active 1
#define rst_inactive 0
// Width of system time counter,
// number of event-in and -out buffers
#define ev_clkpos_width 8
// number of 200MHz cycles it takes from an event_valid signal at the event_buffer_outs´
// fifos to the fifo being not empty at the pop interface
#define ev_bufout_lat 5
// The event out buffers have internal counters to determine the
// respective MSBs for the current data packet
// these bits are selected according to the definition of
// etimemsb_w, below
// Number of event buffers
#define event_ins 8
#define event_outs 6
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
#define tim_rx_clk_latency 0.9
// the defines Andy was to lazy to type...
// -- clocks aren't delayed anymore --
#define ctl_rx0 0
#define ctl_rx1 9
#define ctl_tx0 18
#define ctl_tx1 27
#define ctl_deldefaultvalue 3 // reset value for the delay chains
#define ctl_deladdr_pos 0     // positions relative to ht_datain0
#define ctl_deladdr_width 6
#define ctl_delval_pos 6
#define ctl_delval_width 3
#define ctl_delcid_pos 0 // positions relative to ht_datain1
#define ctl_delcid_width 4
#define ctl_delnum 36 // number of delay lines to be configured
// Some constants for the packets...
#define idle_char 16b1010101001010101
// Some constants the JTAG port
#define jtag_sentinel_val 0 // see comments in spikenet_top.v
/*********************************************************/
// Bit Posistions in data packets:
// identical for all
#define chipid_pos 2 // the chip's address - in 64 bit packet
#define chipid_width 4
// Data fields containing commands and data payload:
#define pa_spare 1        // spare bit position (to address future chips etc. ;-))
#define pa_eventp 0       // packet is event data (1) or command (0) - bit's position
#define pa_pos 6          // start bit of all (incl. cmd) data payload
#define pa_width (64 - 6) // complete data payload, also width of compl. event data
// The idle bits are the two CTL bits of the link,
// which both are high when the link is idle.
#define pa_idle0_pos 8
#define pa_idle1_pos 17
#define def_spareval 1b0
// The idle bits are the two CTL bits of the link,
// which both are high when the link is idle.
#define idle0_pos 8
#define idle1_pos 17
#define ci_pos 11          // start bit of pure data payload
#define ci_width (64 - 11) // data field after command decoder
#define ci_msb 63
// Common command bits:
#define ci_cmdrwb_pos 6 // command is read or write
#define ci_cmd_pos 7    // the actual command
#define ci_cmd_width 4
// Commands:
// The LSB of the four command bits is reserved as
// error flag!
#define ci_errori 1
#define ci_synci 0
#define ci_loopbacki 2
#define ci_paramrami 4
#define ci_controli 6
#define ci_synrami 8
#define ci_areadouti 10
#define ci_evloopbacki 12
#define ci_dummycmdi 14
#define ci_readi 1 // to make sure nothing is mixed up ;-)
#define ci_writei 0
// old version, obsolete!!!, do not use any more
#define ci_error 4dci_errori
#define ci_sync 4dci_synci
#define ci_loopback 4dci_loopbacki
#define ci_paramram 4dci_paramrami
#define ci_control 4dci_controli
#define ci_synram 4dci_synrami
#define ci_areadout 4dci_areadouti
#define ci_evloopback 4dci_evloopbacki
#define ci_dummycmd 4dci_dummycmdi
#define ci_read 1b1 // to make sure nothing is mixed up ;-)
#define ci_write 1b0
// reserved chip_id for empty packets
#define ci_emptycid 4hf
// bit positions in error packet
// first, event errors - positions relative to pa_pos
#define er_ev_startpos 3 // start of almost full flag status
#define er_ev_msb 24     // msb of fifo almost full flags
// deframers are hardwired selected for being able to sync or not
// -> ONLY deframer 0 is allowed to sync.
#define sy_syncen 1 // enable sync mode
#define sy_nosync 0 // disable
// The Sync packet
// these positions are relative to the vectors rx_word in the deframers,
// which still contain the ctl bit!!
#define sy_val_pos 0 // position of sync value relative to rx_word
                     // during sync, this is the SECOND 16 Bit word
                     // clocked into ht_deframer!
#define sy_val_width 8
// Event related stuff ***********************************************++
#define ev_perpacket 3    // events per packet
#define ev_bufaddrwidth 6 // number of address bits per buffer
#define ev_ibuf_width (ev_bufaddrwidth + (ev_clkpos_width - 1) + ev_tb_width)
#define ev_obuf_width                                                                              \
	(ev_bufaddrwidth + ev_clkpos_width + ev_tb_width) // There WAS a "+1" for the early out bit!
// event packet bit-positions
// these are absolute positions, as the data is directly multiplexed to
// the event buffers
// e means event, the rest is:
// time:  absolute time, msb for all three and lsb for each single one
// tb:    time bin relative to time
// na:    neuron address
// valid: event data valid
#define ev_timemsb_pos 9
#define ev_timemsb_width 4
#define ev_time0lsb_pos 17
#define ev_time1lsb_pos 34
#define ev_time2lsb_pos 51
#define ev_timelsb_width 4
#define ev_tb0_pos 13
#define ev_tb1_pos 30
#define ev_tb2_pos 47
#define ev_tb_width 4
#define ev_na0_pos 21
#define ev_na1_pos 38
#define ev_na2_pos 55
#define ev_na_width 9
#define ev_0valid 6
#define ev_1valid 7
#define ev_2valid 8
// parameter ram access bit positions:
// these are relative positions to ci_pos
// (see definition of sc_width, above!)
#define pr_cmd_pos 0    // start position of param ram command
#define pr_cmd_width 4  // width of param ram command
#define pr_cmd_ram 0    // access parameter sram
#define pr_cmd_period 1 // access voutclkx period register
#define pr_cmd_lut 2    // access lookup table
#define pr_cmd_num 3    // access address counter stop value (actual number of parameters)
//$ command data fields
//$ cmd period
#define pr_period_pos 4 // position of period value
#define pr_period_width 8
#define pr_num_pos 4 // start position of number of parameters
//$ cmd lookup
#define pr_lutadr_depth 16
#define pr_luttime_pos 4 // time value to be written into lut
#define pr_luttime_width 4
#define pr_lutboost_pos 8 // boost time exponent to be written into lut
#define pr_lutboost_width 4
//$ additional entries for repeat and step size
#define pr_lutrepeat_pos 12 //$ time exponent to be written into lut
#define pr_lutrepeat_width 8
#define pr_lutstep_pos 20 //$ boost time value to be written into lut
#define pr_lutstep_width 4
#define pr_lutadr_pos 40 // address of lookup table
#define pr_lutadr_width 4
#define pr_boostmul 10 //$ boost current multiplicator
//$command parameter sram
#define pr_ramaddr_pos 4 // address in parameter ram
#define pr_ramaddr_width 12
#define pr_paraddr_pos 16 // address on anablock
#define pr_paraddr_width 12
#define pr_dacval_pos 28 // DAC value
#define pr_dacval_width 10
#define pr_curaddr_width 10       // address lines to analog block
#define pr_synparams 2048         // number of parameters
#define pr_neuronparams (2 * 768) // changed! each neuron decodes 2 lsb, not 1
                                  // only 20 outputs of each curmem_vout_block are used. Each one
                                  // needs 2 parameters.
// problem -> test outputs and uncontrolled drifting
// changed to 128
#define pr_voutparams 128
#define pr_biasparams 16
#define pr_outampparams 16
// default values for initial behavior: 20 clocks
#define pr_defboostcount 10 //$boost 50 percent
#define pr_defcount 10
#define pr_defparamvalue 100 //$default parameter value:10%
// $vout charging period
// 200MHz clock cycles
#define pr_perioddefvalue 8d15
// $total number of parameters minus 1
// for simulation we can set the total value of parameters to a low value
#define pr_numparameters (pr_synparams
// start addresses in the parameter ram
#define pr_adr_syn0start 0
#define pr_adr_syn1start (pr_synparams / 2)
#define pr_adr_neuron0start (pr_synparams)
#define pr_adr_neuron1start (pr_synparams + pr_neuronparams / 2)
#define pr_adr_voutlstart (pr_synparams + pr_neuronparams)
#define pr_adr_voutrstart (pr_synparams + pr_neuronparams + pr_voutparams / 2)
#define pr_adr_biasstart (pr_synparams + pr_neuronparams + pr_voutparams)
#define pr_adr_outampstart (pr_synparams + pr_neuronparams + pr_voutparams + pr_biasparams)
// the loopback register
#define lb_startpos 11 // the full data content of one packet
#define lb_msb 53      // can be used for loopback
// the control register:
// resetvalue:
// (event_outs)<<cr_eout_rst | (event_ins)<<cr_einb_rst | (event_ins)<<cr_ein_rst
// rest has reset value 0.
#define cr_rstval 32b111111_11111111_11111111_00_00000000
#define cr_sel_pos 0       // start position of Bits that select
                           // the status register to readback
#define cr_sel_width 4     // 4 registers - 2 Bits :-)
#define cr_sel_control 0   // read control register and revision
#define cr_sel_clkstat 1   // read clk fifo status and clk_pos
#define cr_sel_clkbstat 2  // read clkb fifo status and clkb_pos
#define cr_sel_clkhistat 3 // read clkhi fifo status, clkhi_pos,
                           // hnibble of event packet time and pll_locked
#define cr_pos 4           // start position of control register content, relative to ci_pos.
                           // on read, status register content also starts from here
#define cr_width 32        // width of control register
// the following bit positions are relative to the control register!
#define cr_err_out_en 5 // high active enable for instantaneous event error packet generation
#define cr_earlydebug 6 // Flag for EarlyOut debug mode
#define cr_anaclken 7   // Analog Block clk200 and clk200b enable
#define cr_anaclkhien 8 // Analog Block clk400 enable
#define cr_pram_en 9    // parameter ram fsm enable bit
#define cr_ein_rst 10   // first bit of the evt_clk buffer in resets (reset is ACTIVE HIGH)
#define cr_einb_rst 18  // first bit of the evt_clkb buffer in resets (reset is ACTIVE HIGH)
#define cr_eout_rst 26  // first bit of the event buffer out resets (reset is ACTIVE HIGH)
// status of the event buffers is assembled in three vectors. One for the
// evt_clk, evt_clkb and clkhi buffers each.
// Each status consists of 4Bit which have the following meaning:
#define cr_fifofbit 0   // fifo full flag
#define cr_fifoafbit 1  // fifo almost full flag
#define cr_fifohfbit 2  // fifo half full flag
#define cr_fifoerrbit 3 // fifo error flag
// besides the control register, three different "status registers" can be read
// from the chip. The content is described above in the command definition
// Following are the widths of the registers and the start posisions of the
// contained values:
#define cr_crread_width (cr_width + revision_width) // upon read, the revision number is tranmitted
#define cr_clkstat_width 39                         // 8*4Bit status, 7Bit clk_pos
#define cr_clkpos_pos 32                            // first the status, clk_pos starts here
#define cr_clkhistat_width                                                                         \
	37                     // 6*4Bit status, 7Bit clkhi_pos, 4Bit evpacket_hinibble, 1Bit pll_locked
#define cr_clkhipos_pos 24 // first the status, then clkhi_pos
#define cr_clkhi_hin_pos 32 // start pos of value of event_buf_out hinibble counter
#define cr_clkhi_pll_pls 36 // position of the pll_locked bit
// **** synapse control module constants ****
#define sc_commandwidth 4
// status and control
#define sc_cmd_ctrl 0 // control and status
#define sc_cmd_time 1 // timing parameters
// plasticity lookup-table commands
#define sc_cmd_plut 2 // lookup
// ram read/write commands
#define sc_cmd_syn 3 // synapse weigths and row/col control data (col data can only be written)
#define sc_cmd_close 4 // close open synapse row (above commands leave row open)
// correlation commands
#define sc_cmd_cor_read 5 // read correlation senseamp outputs
#define sc_cmd_pcor 6 // process correlation for one row
#define sc_cmd_pcorc 7 // process correlation for one row
// all the following field definitions start by ci_pos+sc_commandwidth
// **** common definitons for all address/data fields: data|address|cmd|buscmd
#define sc_aw 24 // max address width
#define sc_dw 24 // data width 4* #of64blocks
#define sc_rowconfigbit 16 // selects row config bits, only row address valid -> ~colmodecore
#define sc_neuronconfigbit                                                                         \
	17 // selects neuron config bits, only column address valid -> ~rowmodecore
#define sc_plut_res 4 // resolution of correlation lookup table in bits
#define sc_colmsb 5 // number of columns (physical chip parameter)
#define sc_rowmsb 7 // dito, rows (physical chip parameter)
#define sc_cprienc 6 // number of correlation priority encoders
// **** time register ****
// sc_pcorperiod|sc_pcsecdelay|sc_csensedelay|cmd|buscmd
#define sc_csensedelaymsb 7 // 8 bit field
#define sc_pcsecdelaymsb 7  //
#define sc_pcorperiodmsb 11 // (minimum) time interval between auto pcor rows
#define sc_pcreadlenpos 28  // if one, read takes 2 cycles instead of 1 in pcor
#define sc_tregw 29 // number of valid bits in treg register
// **** control and status register ****
#define sc_csregmsb 31
#define sc_sreg_cmerr 0   // error in command decoding
#define sc_sreg_done 1    // done status
#define sc_sreg_pcauto 2  // auto correlation processing running
#define sc_sreg_pccont 3  // continuous auto correlation processing
#define sc_sreg_dllres 4  // set dll charge pump voltage to Vdllreset
#define sc_sreg_nresetb 5 // zero holds all neurons in reset state -> prevents firing
#define sc_sreg_row 8     // actual row processed
#define sc_sreg_start 16  // start row
#define sc_sreg_stop 24   // stop row
#define sc_sreg_rowmsb 7  // 8 bits field width for rows
// **** column (neuron) config data ****
#define sc_ncd_evout 4 // enables event out
#define sc_ncd_outamp 2 // enables membrane voltage out
// **** row config data definitons (synapse drive data) ****
#define sc_sdd_is0 1 // input select (0-3): evin,in<1>,prev,in<0>
#define sc_sdd_is1 2
#define sc_sdd_senx 4
#define sc_sdd_seni 8
#define sc_sdd_enstdf 16
#define sc_sdd_dep 32
#define sc_sdd_cap2 64
#define sc_sdd_cap4 128
#define sc_blockshift 8 // shift for block 1
// (is1:is0) definitions
#define sc_sdd_evin 0 // event in from event fifo
#define sc_sdd_in1 1 // adjacent block feedback
#define sc_sdd_prev                                                                                \
	2 // input of previous row, only used for every second row (e.g. 1 uses same input than 0)
#define sc_sdd_in0 3 // same block feedback
// **** analog read out chain control interface ****
#define ar_numchains 2 // number of implemented chains
#define ar_voutl 0 // chain definitions
#define ar_voutr 1
#define ar_maxlen 24 // longest chain used
#define ar_chainnumberwidth 4 // width of chain number field in din
#define ar_chainlen ci_width // width of data field, equals maximum chain length
// ****event loopback module control interface ****
#define el_depth 3      // depth of loopback chain
#define el_enable_pos 0 // event loopback enable bit
#define el_hibufsel_clk_pos                                                                        \
	1 // if 1, selects input buffers 3 and 7 to output buffers 2 and 5 in clk domain
      // if 0, selects input buffers 2 and 6 to output buffers 2 and 5
#define el_hibufsel_clkb_pos 2 // same as above for clkb domain
#define el_earlyout_pos 4      // start position of earlyout bits to apply to the even_out_buffers
                               // give `event_outs number of bits!
