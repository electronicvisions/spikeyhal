#include "hardwareConstants.h"
#ifndef HWCONSTANTS_REV3
#define HWCONSTANTS_REV3

class HardwareConstantsRev3 : public HardwareConstants
{
public:
	HardwareConstantsRev3(){};
	~HardwareConstantsRev3(){};

	uint revision() const { return 3; }
	uint revision_width() const { return 4; }


	uint rst_active() const { return 1; }
	uint rst_inactive() const { return 0; }


	uint ev_clkpos_width() const { return 8; }


	uint ev_bufout_lat() const { return 5; }


	uint event_ins() const { return 8; }
	uint event_outs() const { return 6; }


	uint ctl_rx0() const { return 0; }
	uint ctl_rx1() const { return 9; }
	uint ctl_tx0() const { return 18; }
	uint ctl_tx1() const { return 27; }
	uint ctl_deldefaultvalue() const { return 3; }
	uint ctl_deladdr_pos() const { return 0; }
	uint ctl_deladdr_width() const { return 6; }
	uint ctl_delval_pos() const { return 6; }
	uint ctl_delval_width() const { return 3; }
	uint ctl_delcid_pos() const { return 0; }
	uint ctl_delcid_width() const { return 4; }
	uint ctl_delnum() const { return 36; }


	uint jtag_sentinel_val() const { return 0; }


	uint chipid_pos() const { return 2; }
	uint chipid_width() const { return 4; }

	uint pa_spare() const { return 1; }
	uint pa_eventp() const { return 0; }
	uint pa_pos() const { return 6; }
	uint pa_width() const { return (64 - 6); }


	uint pa_idle0_pos() const { return 8; }
	uint pa_idle1_pos() const { return 17; }


	uint idle0_pos() const { return 8; }
	uint idle1_pos() const { return 17; }
	uint ci_pos() const { return 11; }
	uint ci_width() const { return (64 - 11); }
	uint ci_msb() const { return 63; }

	uint ci_cmdrwb_pos() const { return 6; }
	uint ci_cmd_pos() const { return 7; }
	uint ci_cmd_width() const { return 4; }


	uint ci_errori() const { return 1; }
	uint ci_synci() const { return 0; }
	uint ci_loopbacki() const { return 2; }
	uint ci_paramrami() const { return 4; }
	uint ci_controli() const { return 6; }
	uint ci_synrami() const { return 8; }
	uint ci_areadouti() const { return 10; }
	uint ci_evloopbacki() const { return 12; }
	uint ci_dummycmdi() const { return 14; }
	uint ci_readi() const { return 1; }
	uint ci_writei() const { return 0; }


	uint er_ev_startpos() const { return 3; }
	uint er_ev_msb() const { return 24; }


	uint sy_syncen() const { return 1; }
	uint sy_nosync() const { return 0; }


	uint sy_val_pos() const { return 0; }


	uint sy_val_width() const { return 8; }

	uint ev_perpacket() const { return 3; }
	uint ev_bufaddrwidth() const { return 6; }
	uint ev_ibuf_width() const
	{
		return (ev_bufaddrwidth() + (ev_clkpos_width() - 1) + ev_tb_width());
	}
	uint ev_obuf_width() const { return (ev_bufaddrwidth() + ev_clkpos_width() + ev_tb_width()); }


	uint ev_timemsb_pos() const { return 9; }
	uint ev_timemsb_width() const { return 4; }
	uint ev_time0lsb_pos() const { return 17; }
	uint ev_time1lsb_pos() const { return 34; }
	uint ev_time2lsb_pos() const { return 51; }
	uint ev_timelsb_width() const { return 4; }
	uint ev_tb0_pos() const { return 13; }
	uint ev_tb1_pos() const { return 30; }
	uint ev_tb2_pos() const { return 47; }
	uint ev_tb_width() const { return 4; }
	uint ev_na0_pos() const { return 21; }
	uint ev_na1_pos() const { return 38; }
	uint ev_na2_pos() const { return 55; }
	uint ev_na_width() const { return 9; }
	uint ev_0valid() const { return 6; }
	uint ev_1valid() const { return 7; }
	uint ev_2valid() const { return 8; }


	uint pr_cmd_pos() const { return 0; }
	uint pr_cmd_width() const { return 4; }
	uint pr_cmd_ram() const { return 0; }
	uint pr_cmd_period() const { return 1; }
	uint pr_cmd_lut() const { return 2; }
	uint pr_cmd_num() const { return 3; }


	uint pr_period_pos() const { return 4; }
	uint pr_period_width() const { return 8; }
	uint pr_num_pos() const { return 4; }

	uint pr_lutadr_depth() const { return 16; }
	uint pr_luttime_pos() const { return 4; }
	uint pr_luttime_width() const { return 4; }
	uint pr_lutboost_pos() const { return 8; }
	uint pr_lutboost_width() const { return 4; }

	uint pr_lutrepeat_pos() const { return 12; }
	uint pr_lutrepeat_width() const { return 8; }
	uint pr_lutstep_pos() const { return 20; }
	uint pr_lutstep_width() const { return 4; }
	uint pr_lutadr_pos() const { return 40; }
	uint pr_lutadr_width() const { return 4; }
	uint pr_boostmul() const { return 10; }

	uint pr_ramaddr_pos() const { return 4; }
	uint pr_ramaddr_width() const { return 12; }
	uint pr_paraddr_pos() const { return 16; }
	uint pr_paraddr_width() const { return 12; }
	uint pr_dacval_pos() const { return 28; }
	uint pr_dacval_width() const { return 10; }
	uint pr_curaddr_width() const { return 10; }
	uint pr_synparams() const { return 2048; }
	uint pr_neuronparams() const { return (2 * 768); }


	uint pr_voutparams() const { return 128; }
	uint pr_biasparams() const { return 16; }
	uint pr_outampparams() const { return 16; }

	uint pr_defboostcount() const { return 10; }
	uint pr_defcount() const { return 10; }
	uint pr_defparamvalue() const { return 100; }


	uint pr_adr_syn0start() const { return 0; }
	uint pr_adr_syn1start() const { return (pr_synparams() / 2); }
	uint pr_adr_neuron0start() const { return (pr_synparams()); }
	uint pr_adr_neuron1start() const { return (pr_synparams() + pr_neuronparams() / 2); }
	uint pr_adr_voutlstart() const { return (pr_synparams() + pr_neuronparams()); }
	uint pr_adr_voutrstart() const
	{
		return (pr_synparams() + pr_neuronparams() + pr_voutparams() / 2);
	}
	uint pr_adr_biasstart() const { return (pr_synparams() + pr_neuronparams() + pr_voutparams()); }
	uint pr_adr_outampstart() const
	{
		return (pr_synparams() + pr_neuronparams() + pr_voutparams() + pr_biasparams());
	}

	uint lb_startpos() const { return 11; }
	uint lb_msb() const { return 53; }


	uint cr_sel_pos() const { return 0; }

	uint cr_sel_width() const { return 4; }
	uint cr_sel_control() const { return 0; }
	uint cr_sel_clkstat() const { return 1; }
	uint cr_sel_clkbstat() const { return 2; }
	uint cr_sel_clkhistat() const { return 3; }

	uint cr_pos() const { return 4; }

	uint cr_width() const { return 32; }

	uint cr_err_out_en() const { return 5; }
	uint cr_earlydebug() const { return 6; }
	uint cr_anaclken() const { return 7; }
	uint cr_anaclkhien() const { return 8; }
	uint cr_pram_en() const { return 9; }
	uint cr_ein_rst() const { return 10; }
	uint cr_einb_rst() const { return 18; }
	uint cr_eout_rst() const { return 26; }
	// the next 3 constants can return 0 because they are not used in Spikey 3
	uint cr_eout_depth() const { return 0; }
	uint cr_eout_depth_width() const { return 0; }
	uint cr_eout_depth_resval() const { return 0; }


	uint cr_fifofbit() const { return 0; }
	uint cr_fifoafbit() const { return 1; }
	uint cr_fifohfbit() const { return 2; }
	uint cr_fifoerrbit() const { return 3; }


	uint cr_crread_width() const { return (cr_width() + revision_width()); }
	uint cr_clkstat_width() const { return 39; }
	uint cr_clkpos_pos() const { return 32; }
	uint cr_clkhistat_width() const { return 37; }
	uint cr_clkhipos_pos() const { return 24; }
	uint cr_clkhi_hin_pos() const { return 32; }
	uint cr_clkhi_pll_pls() const { return 36; }

	uint sc_commandwidth() const { return 4; }

	uint sc_cmd_ctrl() const { return 0; }
	uint sc_cmd_time() const { return 1; }

	uint sc_cmd_plut() const { return 2; }

	uint sc_cmd_syn() const { return 3; }
	uint sc_cmd_close() const { return 4; }

	uint sc_cmd_cor_read() const { return 5; }
	uint sc_cmd_pcor() const { return 6; }
	uint sc_cmd_pcorc() const { return 7; }


	uint sc_aw() const { return 24; }
	uint sc_dw() const { return 24; }
	uint sc_rowconfigbit() const { return 16; }
	uint sc_neuronconfigbit() const { return 17; }
	uint sc_plut_res() const { return 4; }
	uint sc_colmsb() const { return 5; }
	uint sc_rowmsb() const { return 7; }
	uint sc_cprienc() const { return 6; }


	uint sc_csensedelaymsb() const { return 7; }
	uint sc_pcsecdelaymsb() const { return 7; }
	uint sc_pcorperiodmsb() const { return 11; }
	uint sc_pcreadlenpos() const { return 28; }
	uint sc_tregw() const { return 29; }

	uint sc_csregmsb() const { return 31; }
	uint sc_sreg_cmerr() const { return 0; }
	uint sc_sreg_done() const { return 1; }
	uint sc_sreg_pcauto() const { return 2; }
	uint sc_sreg_pccont() const { return 3; }
	uint sc_sreg_dllres() const { return 4; }
	uint sc_sreg_nresetb() const { return 5; }
	uint sc_sreg_row() const { return 8; }
	uint sc_sreg_start() const { return 16; }
	uint sc_sreg_stop() const { return 24; }
	uint sc_sreg_rowmsb() const { return 7; }

	uint sc_ncd_evout() const { return 4; }
	uint sc_ncd_outamp() const { return 2; }

	uint sc_sdd_is0() const { return 1; }
	uint sc_sdd_is1() const { return 2; }
	uint sc_sdd_senx() const { return 4; }
	uint sc_sdd_seni() const { return 8; }
	uint sc_sdd_enstdf() const { return 16; }
	uint sc_sdd_dep() const { return 32; }
	uint sc_sdd_cap2() const { return 64; }
	uint sc_sdd_cap4() const { return 128; }
	uint sc_blockshift() const { return 8; }

	uint sc_sdd_evin() const { return 0; }
	uint sc_sdd_in1() const { return 1; }
	uint sc_sdd_prev() const { return 2; }
	uint sc_sdd_in0() const { return 3; }

	uint ar_numchains() const { return 2; }
	uint ar_voutl() const { return 0; }
	uint ar_voutr() const { return 1; }
	uint ar_maxlen() const { return 24; }
	uint ar_chainnumberwidth() const { return 4; }
	uint ar_chainlen() const { return ci_width(); }
	uint ar_numvouts() const { return 24; }

	uint el_depth() const { return 3; }
	uint el_enable_pos() const { return 0; }
	uint el_hibufsel_clk_pos() const { return 1; }

	uint el_hibufsel_clkb_pos() const { return 2; }
	uint el_earlyout_pos() const { return 4; }

	uint el_offset() const { return 4; }

	uint sg_eventw() const { return 21; } //  maximum width to fit in 1/3 of a 64 bit field

	uint sg_numoutports() const { return 3; } //  number of event ports

	uint sg_numevoutfifos() const { return 3; } //  number of fifos to build event packets from

	uint sg_numinports() const { return 3; } //  number of event ports

	// #### event port specification ####

	// these values are defined separately to be independent of spikenet_base definitions.

	uint sg_systimewidth() const { return 32; } //  width of fpga system time counter

	uint sg_eadrwidth() const { return 9; } //  neuron address

	uint sg_etimewidth() const { return 8; } //  event time (400MHz counter)

	uint sg_efinewidth() const { return 4; } //  event time within 400MHz cycle

	uint sg_datawidth() const
	{
		return sg_eadrwidth() + sg_etimewidth() + sg_efinewidth();
	} //  total width of event data field

	uint sg_evtimeout() const
	{
		return 54;
	} //  write fpga time to pbmem if no events after this # of cycles (200MHz cycles)

	//																						 value (400MHz):	128 - max difference in event
	//latency when fifos empty.

	//																															max difference is 16 + 2 cycles
	//for sync uncertainty.

	uint sg_evtimeoutw() const { return 6; } //  width of timeout counter

	// ###################################################################################

	// control interface definitions

	uint sg_latcnt_width() const { return 8; } //  width of chain latency counter

	uint sg_numchips() const { return 1; } //  number of chips in chain

	uint sg_fpga_time_msb() const { return 31; } //  msb of system time counter

	//	 maximum round trip delay in 200MHz cycles for one packet

	uint sg_chain_latency() const { return 14; } //  for single chip!

	//	 max count of received error packets until timeout

	uint sg_ci_maxerrors() const { return 25000; } //  !!! review value !!!

	uint sg_ci_maxerr_w() const { return 16; } //
	uint sg_lat2csync() const
	{
		return 19;
	} //  #400MHz cycles from controller sync (FPGA starts counting) to spikey sync (spikey starts
	  //  counting)

	uint sg_latrd2cpreg() const
	{
		return 15;
	} //  #400MHz cycles from read command (controller) to clkhipos_reg valid (spikey)

	uint sg_parwpos() const { return 0; } // rw bit in pa_ci_packet (ci_width+cmd+rw)

	uint sg_sdram_wtim_max() const
	{
		return 20;
	} //  max number of cycles the sdram will need to empty the write fifo.

	//	 besides sync (command: ci_synci) the controller suppoprts these commands that are encoded

	//	 within the command field of the playback memory entries

	uint sg_ci_chipid() const { return 10; } //  set/change the controller's chipid register

	uint sg_ci_chipid_pos() const { return 5; } //  position of new chipid within sci_dataout

	// ###################################################################################

	// Slow Control interface definitions

	uint sg_cm_spikey() const { return 2; } //  code module number, no better place yet

	//	 MSB of Slow Control addresses related to spikey

	uint sg_scaddrmsb() const
	{
		return 4;
	} //  distinguishes between access to spikey_control or spikey_sei

	//	 The following Slow Control addresses are mapped to commands for spikey access:

	uint sg_scmode() const { return 16; } //  set/get spikey's mode/reset pins

	uint sg_scdirect0() const { return 17; } //  read/write lower 18bit of sci_dataxxx

	uint sg_scdirect1() const { return 18; } //
	uint sg_scdirect2() const { return 19; } //
	uint sg_scdirect3() const { return 20; } //
	uint sg_scfire() const { return 21; }    //  pulse spikey's firein lines

	uint sg_scdelay() const { return 22; } //  set spikey's delay lines

	uint sg_scsendidle() const { return 23; } //  idle event packets on/off

	uint sg_scclkphase() const { return 24; } //  rx clock pahse selection

	uint sg_scevtimeout() const { return 25; } //  reset value of event system time timeout counter

	uint sg_scrxevstat() const { return 26; } //  read received event packet "statistics"

	uint sg_scanamux() const { return 27; } //  set select bits for analog multiplexer

	uint sg_scleds() const { return 28; } //  set pins connected to LEDs on recha2 :-)

	//	 constants regarding mode/reset pins

	uint sg_scrst_pos() const
	{
		return 0;
	} //  position of reset pin's value within sc.data_in field

	uint sg_sccim_pos() const { return 4; } //  position of ci_mode

	uint sg_scdirectout() const { return 0xffffffff; } //  position of direct packet out

	uint sg_scbsm_pos() const { return 8; } //  position of bs_mode

	uint sg_scplr_pos() const { return 12; } //  position of pll_reset

	uint sg_scplb_pos() const { return 16; } //  position of pll_bypass

	uint sg_scpll_pos() const
	{
		return 20;
	} //  position of pll_locked signal (for read access only!)

	uint sg_scsye_pos() const { return 24; } //  position of sync error flag

	//	 constants regarding direct link access (relative to sc.data_in):

	uint sg_direct_msb() const
	{
		return 17;
	} //  msb of direct in/out bus. 8bit data plus 1bit ctl x2

	uint sg_direct_pos() const { return 0; } //  position of link direct in/out data within

	//																							 Slow Control data field
	//(t_sc_module.data_xxx)

	//	 constants regarding firein lines access

	uint sg_scfil_msb() const { return 3; } //  fireinleft bus msb

	uint sg_scfil_pos() const
	{
		return 8;
	} //  fireinleft bits start position within sc.data_in field

	uint sg_scfir_msb() const { return 5; } //  fireinright bus msb

	uint sg_scfir_pos() const
	{
		return 0;
	} //  fireinright bits start position within sc.data_in field

	uint sg_scfil_late_pos() const { return 20; } //  fireins with late-bit will be sent to spikey

	uint sg_scfir_late_pos() const { return 12; } //  after firein late delay

	uint sg_scfidel_pos() const
	{
		return 24;
	} //  delay for firein late signals -> distance between rising edges in sysclock/2 cycles

	uint sg_scfidel_msb() const { return 7; } //  delay 4 means two cycles from early to late pulse

	//	 constants regarding delay line configuration (relative to sc.data_in):

	uint sg_scdeladdr_pos() const { return 0; } //  address of delay line to be written

	uint sg_scdelval_pos() const { return 8; } //  value to be written

	uint sg_scdelcid_pos() const { return 12; } //  chip id

	//	 constants regarding config and status of Slow Control access (spikey_control)

	uint sg_scevidle_pos() const { return 0; } //  position of idle packet type control bit

	uint sg_sc1evidle_pos() const { return 4; } //  position for "send one idle event packet" bit

	uint sg_scsyncerr_pos() const { return 8; } //  position of sync error flag

	uint sg_scckph0_pos() const { return 12; } //  position of phase sel. bit within sc data

	uint sg_scckph1_pos() const { return 16; } //
	//	 constants regarding event packet stats

	uint sg_rxeverr_width() const { return 8; } //  number of received event error packets

	uint sg_rxeverr_pos() const { return 0; } //
	//	 number of clk-cycles to wait after C_DELAY or delay line address/value has been changed

	uint sg_dcapplycnt() const { return 7; } //
	//	 ### begin obsolete (these constants are only there for compatibility with software

	//	 and will be deleted in coming revisions!) ###

	uint sg_scdin0() const { return 19; } //  get packet data from link 0

	uint sg_scdin1() const { return 20; } //  get packet data from link 1

	uint sg_scramdebug() const { return 25; } //  playback ram debug mode on/off

	uint sg_scscevents() const { return 27; } //  read number of events scheduled by spikey_sei

	uint sg_sctxevents() const
	{
		return 28;
	} //  read number of events transmitted by spikey_control

	uint sg_sceventcw() const { return 8; } //  width of transmitted event counter

	uint sg_scramdebug_pos() const { return 20; } //  playback ram debug flag.

	uint sg_rxevnum_width() const { return 8; } //  number of received event packets

	uint sg_rxevnum_pos() const { return 0; } //
	//	 ### end obsolete ###

	// ###################################################################################

	// cycles to wait for ramclient after pb mem start. -> Makes sure there's nor refresh at the
	// beginning of pbmem cycle.

	uint sg_pfwait() const { return 1024; } //
	// ram command definitions

	uint sg_ev_comw() const { return 2; } //  number of command bits

	uint sg_ev_rdcom_ci() const { return 0; } //  ci rw command

	uint sg_ev_rdcom_ec() const { return 2; } //  event command

	uint sg_ev_evsize() const { return 21; } //  event fields

	uint sg_ev_evbase() const { return 1; } //  lsb of first event

	uint sg_ev_citimew() const { return 4; } //  ci command time field

	uint sg_ev_citime() const { return sg_ev_comw(); } //
	uint sg_ev_cidataw() const { return pa_width(); }  //  ci command data filed + command +rw

	uint sg_ev_cidata() const { return sg_ev_citime() + sg_ev_citimew(); } //
	uint sg_ev_timew() const { return 16; }                                //  ev command time field

	uint sg_ev_time() const { return 4; }   //
	uint sg_ev_numevw() const { return 8; } //  ev command number of event words

	uint sg_ev_numev() const { return sg_ev_time() + sg_ev_timew(); } //
	uint sg_ev_evmaskw() const { return 3; } //  ev command mask for last event word

	uint sg_ev_evmask() const { return sg_ev_numev() + sg_ev_numevw(); } //
	// field widths slow control configuration

	uint sg_adw() const { return 31; } // address bits used as data for ci acces

	uint sg_sc_numreadw() const { return 24; } //
	uint sg_sc_radrw() const { return 27; }    //  max ram

	uint sg_sc_wadrw() const
	{
		return 27;
	} // (ag): DO NOT increase over 28 because otherwise the below status bits will conflict!

	uint sg_sc_wtimw() const { return 9; } // (ag): width of wait time counter

	uint sg_sc_syncsw() const { return 8; } // width of number-of-syncs counter

	uint sg_sc_wtim() const { return 28; } // (ag): pos of wtimis0 bit in sc_dout vector

	uint sg_sc_isidle() const { return 29; } // (ag): pos of pbmem fsm idle status bit

	uint sg_sc_read_empty() const
	{
		return 30;
	} // (ag): pos of bit indicating that read fifo empty occured

	uint sg_sc_write_inh() const
	{
		return 31;
	} // (ag): pos of bit indicating that write fifo inhibit occured

	uint sg_sc_write_full() const { return 0xffffffff; } // (ag): not valid in this version

	uint sg_sc_chipidw() const { return 4; } //
	uint sg_sc_statusw() const { return 4; } //
	uint sg_sc_wdataw() const
	{
		return sg_ev_cidataw() - sg_adw() - 1;
	} //  sc read remaining ci write data width

	uint sg_sb_start_ramsm() const { return 0; } //  bit number for start_ramsm

	uint sg_sb_reset_ramsm() const { return 1; } // (ag): bit number for reset_ramsm

	uint sg_sb_read_empty() const { return 2; } // (ag): indicates that read fifo empty occured

	uint sg_sb_write_inh() const { return 3; } // (ag): indicates that write fifo inhibit occured

	uint sg_sb_write_full() const { return 0xffffffff; } // (ag): not valid in this version

	// slow control addresses

	uint sg_sc_addrw() const { return 5; } //  number of address bits

	uint sg_sc_status() const { return 0; } //  status register

	uint sg_sc_numread() const { return 1; } //  number of ram words to read

	uint sg_sc_radr() const { return 2; } //  ram start adr

	uint sg_sc_chipid() const { return 3; } //  chip id to use

	uint sg_sc_rdata() const { return 8; } //  sc read remaining ci read data

	uint sg_sc_wdata() const { return 9; } //  sc read remaining ci write data

	uint sg_sc_wadr() const { return 4; } //  sc address for ram write port

	uint sg_sc_rradr() const { return 5; } //  read back current read address

	uint sg_sc_rwadr() const { return 6; } //  read back current write address

	uint sg_sc_prefcnt() const { return 7; } //  read/write prefetch counter load value

	uint sg_sc_cten() const { return 10; } //  read/write common trigger enable

	uint sg_sc_syncs() const
	{
		return 12;
	} // read back number of executed sync commands (ag, added on 2014/05/05)

	// constants regarding analog multiplexer and leds on recha2

	uint sg_scibtestpos() const { return 0; } //  bit position of ibtest enable

	uint sg_scanamuxenpos() const { return 1; } //  bit 0 of analog multiplexer select input

	uint sg_scanamuxenw() const { return 3; } //  width of analog multiplexer select input

	uint sg_scaddled1pos() const
	{
		return 0;
	} //  bit position of add. led 1 on recha (all leds low active)

	uint sg_scaddled2pos() const
	{
		return 1;
	} //  bit position of add. led 2 on recha (all leds low active)

	uint sg_scplexiledpos() const { return 2; } //  led on plexi cover edge

	uint sg_scchipledpos() const { return 3; } //  led near spikey chip


	uint fifo_reset_delay() const { return 5; }

	uint sg_sc_muxw() const { return 2; }
	uint sg_sc_mux10m_pos() const { return 0; }
	uint sg_sc_mux432_pos() const { return 4; }
	uint sg_sc_mux765_pos() const { return 8; }

	// new, not used in Rev3!

	uint sg_scvrest_pos() const { return 0xffffffff; }
	uint sg_scvm_pos() const { return 0xffffffff; }
	uint sg_scibtest_pos() const { return 0xffffffff; }
	uint sg_scpower_pos() const { return 0xffffffff; }
	uint sg_rxd0_phsel_pos() const { return 0xffffffff; }
	uint sg_rxd1_phsel_pos() const { return 0xffffffff; }
	uint sg_rxc0_phsel_pos() const { return 0xffffffff; }
	uint sg_rxc1_phsel_pos() const { return 0xffffffff; }
	uint sg_scphsel() const { return 0xffffffff; }
};

#endif
