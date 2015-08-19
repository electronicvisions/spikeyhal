// forward declaration of typedef gsl_rng(_type) is ugly - including a header.
#include <gsl/gsl_rng.h>
#include "logger.h"
#include <boost/filesystem.hpp>

// VModule stuff is fully contained in this class:
#include "vmodule.h"
#include "vmsp6.h"
#include "Vspikey.h"

#ifndef WITH_FLANSCH
#include "Vmoduleusb.h"
#else
#include "Vmodulesim.h"
#endif

#include "Vusbmaster.h"
#include "Vusbstatus.h"
#include "Vmemory.h"
#include "Vspiconfrom.h"
#include "Vspigyro.h"
#include "Vspiwireless.h"
#include "Vmodule_adc.h"
#include "Vspidac.h"
#include "Vmux_board.h"

#ifdef CONFIG_H_AVAILABLE
#include "config.h"
#ifdef HAVE_GTEST
#include <gtest/gtest_prod.h>
#else
#define FRIEND_TEST(a, b) typedef int gtestisdisabled
#endif
#else
#define FRIEND_TEST(a, b) typedef int gtestisdisabled
#endif

namespace spikey2
{


// realizes the communication via the nathan system's slow control

class SC_SlowCtrl : public SpikenetComm
{
	FRIEND_TEST(pbEvtTests, checkSomeEventVector);

protected:
	::Logger& dbg;

private:
	int mynathan;

	uint delayFpgaIn = 35;
	std::string serial = "";
	uint boardVersion = 2;
	uint chipVersion = 5;
	std::string work_station_name = "";

	const static uint basedelay = 1; // basic bus delay -> one spikey clock cycle for each IData
	                                 // packet
	const static uint sc_dacaddr = 0x300; // address within cm_dac12_tempsense to generate 32bit
	                                      // accesses
	const static uint sc_adcaddr = 0x600; // address within cm_dac12_tempsense to read from adc

	//! chunk size for writing and reading memory
	static const uint maxchunksize = (1 << 17); // 512KB - event loopback crashes for chunksizes
	                                            // greater than that!

	// TP: TODO: remove maxid
	queue<IData> rcvci[maxid]; // receive ci data
	queue<IData> rcvctrl[maxid]; // receive control data
	bool rcvcidel[maxid]; // received ci data pipeline delay flag

	vector<IData> rcvev[maxid];
	vector<IData> sendev[maxid]; // sent events
	vector<IData> errev[maxid]; // events dropped or modified by pbEvt().

	queue<SBData> adc; // results from readAdc command

	uint _pbradr = 0; //!< playback memory pointer of sdrambuf
	uint _pbwadr = 0; //!< playback memory pointer of sdrambuf

	// old buffer stuff
	// TP: TODO (comment by AG): delete this and directly write to pbsend in next version!!!
	vector<uint64_t> sdrambuf;
	uint sdrambufbase = 0;
	bool sdrambufvalid = false;

	// write to playback software buffer
	SpikenetComm::Commstate writeBuf(uint64_t data, uint addr);

	uint ltime[16]; // resp. buffer's last time of event - TP: TODO: leftover from prefetch bug?

	// stores last system time stamp out of playback memory
	// to keep track of overall event time.
	uint lsystime; // last system time transmitted by system time event
	int levtimeclk; // last event time translated

	int usedpcktslots; //!< how many events per event packet, choose in {1,2,3}; set to 1 to disable
	                   //packing, e.g. for multi Spikey

	uint first_proc_corr;    // time in clock cycles at which first process correlation command is
	                         // inserted
	uint proc_corr_dist;     // distance (clock cycles) between process correlation commands
	                         // (20000=every second; at least tdel+2*(tpcorperiod+4*64))
	uint proc_corr_dist_pre; // distance between process correlation commands at synapse clearance
	                         // before experiment start
	bool cont_proc_corr;     // turn process correlation on/off
	uint startrow;           // synapse array to process
	uint stoprow;
	std::vector<int> lut; // LUT in index notation: acausal [0..15] and causal [16..31]
	uint tbmax; // LUT size

	float supply_voltage_limit = 4.5; // spikey needs at least a supply voltage of 4.5 volts

public:
	SC_SlowCtrl(uint time = 0, std::string workstation = "");
	virtual ~SC_SlowCtrl();

	// Vmodule class tree:
	Vmoduleusb* io;
	Vusbmaster* usb;
	// usbmaster knows three clients, must be in this order!!!
	Vusbstatus* status;
	Vmemory* mem;
	Vocpfifo* ocp;
	// ocpfifo clients
	Vspiconfrom* confrom;
	Vspigyro* gyro;
	Vspiwireless* wireless;
	// Spikey stuff - could be consolidated...
	Vspikeyctrl* spyctrl;
	Vspikeypbmem* spypbm;
	Vspikeydelcfg* spydc;
	Vspikeyslowadc* spy_slowadc;
	// Vspifastadc*  spiadc;
	Vspikeyfastadc* fadc;
	Vspikeydac* spydac;
	Vmux_board* muxboard;

	//! start address for ADC data (relative address from the beginning of second memory module
	//(512MB), 32bit aligned)
	static const unsigned int adc_start_adr = 0x0;
	size_t adc_num_samples; //!< number of samples the ADC should record

	const vector<IData>* rcvd(uint c) { return &(rcvev[c]); };
	const vector<IData>* ev(uint c) { return &(sendev[c]); };
	vector<IData>* eev(uint c) { return &(errev[c]); };
	virtual Commstate Send(Mode mode, IData data = emptydata, uint del = 0, uint chip = 0,
	                       uint syncoffset = 0);
	virtual Commstate Receive(Mode mode, IData& data, uint chip = 0);
	virtual Commstate SendSBD(Mode mode, const SBData& data = emptysbdata, uint del = 0,
	                          uint chip = 0);
	virtual Commstate RecSBD(Mode mode, SBData& data, uint chip = 0);

	//
	enum SCTiming { adcsim = 400 };

	// ***** new members *****

	void checkSupplyVoltage(); // measures supply voltage and throws exception, if too low

	uint getMaxChunkSize() { return maxchunksize; };
	// set parameters for process correlation commands inserted into the spike train
	void setProcCorr(bool active, uint first, uint distance, uint distance_pre, uint rowmin,
	                 uint rowmax)
	{
		cont_proc_corr = active;
		first_proc_corr = first;
		proc_corr_dist = distance;
		proc_corr_dist_pre = distance_pre;
		startrow = rowmin;
		stoprow = rowmax;
	};

	uint nathanNum(void) { return mynathan; }; // return nathan board number
	uint getSyncMax()
	{
		return 0x70;
	}; // returns exact number of bus cycles a sync command needs !!! multiple chips will take
	   // longer... !!!
	   // from counting clock cycles in FPGA code, this should be 0x58.
	// It turns out that only this value gives safe results (not yet fully understood by agruebl...)
	uint getEvtLatency(uint chip = 0)
	{
		return 14 + chip * 12;
	}; // returns pb to event input latency (upper boundary) !!! multiple chips
	uint getMaxChainDelay(void)
	{
		return hw_const->sg_chain_latency() + 50;
	}; // TP: timing is still subject for improvement

	void writeDac(const SBData&); // internal write dac method
	void readAdc(SBData& d, uint channel = 1); // read the voltage of slow ADC, channel 0 is supply
	                                           // voltage
	void setupFastAdc(unsigned int sample_time_us, std::bitset<3> adc_input = 0);
	void triggerAdc(); //!< triggers the ADC manually instead via "experiment start"

	// access the spikey Slow Control interface's registers
	Commstate writeSC(uint data, uint addr);
	Commstate readSC(uint& data, uint addr);

	// access the spikey Playback Memory control registers
	Commstate writePBC(uint data, uint addr);
	Commstate readPBC(uint& data, uint addr);

	// functions to access playback memory pointers
	// Attention: read is read from playback mem, write is write to received mem
	// this is the opposite from send(write) receive(read) in SC_SpikenetComm
	uint pbradr() { return _pbradr; }; // read pointer for data sent to spikey
	uint pbwadr() { return _pbwadr; }; // write pointer for data received from spikey
	void setPbradr(uint adr) { _pbradr = adr; };
	void setPbwadr(uint adr) { _pbwadr = adr; };

	// additional functions for event generation in Playback memory
	void pbSync(uint time = 0, uint del = 1, uint offset = 0); // time to sync fpga and chip to,del
	                                                           // to next command
	void pbEvtdel(uint del = 0);                               // delay for time cycles
	void set_LUT(std::vector<int> _lut); // see "lut" for format
	std::vector<int> get_LUT();
	uint gen_plut_data(float nval); // generate LUT commands
	void fill_plut(uint delay, bool identity); // write look-up table
	void pbEvt(vector<IData>& evt, uint& newstime, uint stime = 0,
	           uint chip = 0); // stime: give current time so pbEvt is able to track event times.
	void pbCI(Mode mode, IData& data, uint del = 2); // mode is read/write from Mode enum in base
	                                                 // class, del=2:minimum fpga cycles == spikey
	                                                 // cycles (but not mandatory)

	// internal pbmem setup commands
	void pbEvtcmd(uint lastevcnt = 0, uint numevents = 0, uint time = 0); // event count in last
	                                                                      // pbram word, total
	                                                                      // number of events, time
	                                                                      // for whole cycle
	void pbEvtpct(uint evt1time = 0, uint evt1addr = 0,  // three events per pbram word
	              uint evt2time = 0, uint evt2addr = 0,  // -> may contain dummies if event channels
	                                                     // are
	              uint evt3time = 0, uint evt3addr = 0); // 		not connected
	// interpret SDRAM received content
	void translate(const uint64_t& d, IData&, uint& evmask);

	// functions to access registers...all addresses are 64-bit word aligned (lsb selects even/odd
	// 64 bit word)
	void setContIdleOn() { writeSC((1 << hw_const->sg_scevidle_pos()), hw_const->sg_scsendidle()); }
	void setContIdleOff() { writeSC(0, hw_const->sg_scsendidle()); }
	void sendOneIdle() { writeSC((1 << hw_const->sg_sc1evidle_pos()), hw_const->sg_scsendidle()); }
	void setupPlayback(uint wstartadr, uint rstartadr, uint chipid)
	{
		setupSend(rstartadr, chipid);
		setWadr(wstartadr);
	};
	void setupSend(uint rstartadr, uint chipid);
	void startPlayback();
	//! set all addresses to startup values (stored in configuration registers)
	//! and RAM-FSM to IDLE
	//! JS: should be changed! TODO: why?
	void resetPlayback()
	{
		writePBC((1 << hw_const->sg_sb_reset_ramsm()), hw_const->sg_sc_status()); // hardware reset
		                                                                          // for playback
		                                                                          // memory logic
		sdrambufvalid = false; // mark potentially stored data as false
	}
	void stopPlayback();
	void setNumRead(uint nr)
	{
		writePBC(nr, hw_const->sg_sc_numread());
	} // set to number of 64bit words/2 !!!
	void setRadr(uint ra) { writePBC(ra, hw_const->sg_sc_radr()); }
	//! set address (in configuration register) to which record data should be written
	//! this address is written to RAM pointer, if resetPlayback called
	void setWadr(uint wa) { writePBC(wa, hw_const->sg_sc_wadr()); }
	void setChipid(uint ci) { writePBC(ci, hw_const->sg_sc_chipid()); } // TODO: obsolete?
	void setAnaMux(Vmux_board::mux_input inputselect);
	void setAnaMux(std::bitset<3> inputselect);

	// enable/disable global trigger output
	void setGlobalTrigOut(bool enable) { writePBC((uint)enable, hw_const->sg_sc_cten()); }

	void getCurRadr(uint& res)
	{
		readPBC(res, hw_const->sg_sc_radr());
		res &= mmw(hw_const->sg_sc_radrw());
	}
	void getRadr(uint& res)
	{
		readPBC(res, hw_const->sg_sc_rradr());
		res &= mmw(hw_const->sg_sc_radrw());
	}
	void getNumRead(uint& res)
	{
		readPBC(res, hw_const->sg_sc_numread());
		res &= mmw(hw_const->sg_sc_numreadw());
	}
	//! reads address written with setWadr
	void getWadr(uint& res)
	{ // TODO: merge with getCurWadr(...)
		readPBC(res, hw_const->sg_sc_wadr());
		res &= mmw(hw_const->sg_sc_wadrw());
	}
	//! return write address of FPGA, see getCurWadr for playback memory control registers
	uint curwadr(void)
	{
		uint res;
		bool wtim0, idle, re, wi, wf; // dummies needed for compatibility
		getCurWadr(res, wtim0, idle, re, wi, wf);
		return res;
	}
	void getStatus(uint& res)
	{
		readPBC(res, hw_const->sg_sc_status());
		res &= mmw(hw_const->sg_sc_statusw());
	}
	//! read back current write address of FPGA (up to where results have been written)
	//! and evaluate playback memory control registers
	//! address is used to get to know where to receive data up to
	void getCurWadr(uint& res, bool& wtim0, bool& idle, bool& readempty, bool& writeinh,
	                bool& writefull)
	{
		readPBC(res, hw_const->sg_sc_rwadr());
		wtim0 = res & (1 << hw_const->sg_sc_wtim());
		idle = res & (1 << hw_const->sg_sc_isidle());
		readempty = res & (1 << hw_const->sg_sc_read_empty());
		writeinh = res & (1 << hw_const->sg_sc_write_inh());
		writefull = res & (1 << hw_const->sg_sc_write_full());
		res &= mmw(hw_const->sg_sc_wadrw());
		res &= 0xfffffffeUL; // mask readout and clear lsb (128 bit issue)
	}

	bool getSyncErr()
	{
		uint res;
		readSC(res, hw_const->sg_scsendidle());
		return res & (1 << hw_const->sg_scsyncerr_pos());
	}

	// function to display playback memory content
	void printPBMem();

	// functions to read/write directly from/to the 72bit link registers
	void setDout0(uint pat) { writeSC(pat, hw_const->sg_scdirect0()); }
	void setDout1(uint pat) { writeSC(pat, hw_const->sg_scdirect1()); }
	void setDout2(uint pat) { writeSC(pat, hw_const->sg_scdirect2()); }
	void setDout3(uint pat) { writeSC(pat, hw_const->sg_scdirect3()); }

	void getDin0(uint& pat)
	{
		readSC(pat, hw_const->sg_scdirect0());
		pat &= mmm(hw_const->sg_direct_msb());
	}
	void getDin1(uint& pat)
	{
		readSC(pat, hw_const->sg_scdirect1());
		pat &= mmm(hw_const->sg_direct_msb());
	}
	void getDin2(uint& pat)
	{
		readSC(pat, hw_const->sg_scdirect2());
		pat &= mmm(hw_const->sg_direct_msb());
	}
	void getDin3(uint& pat)
	{
		readSC(pat, hw_const->sg_scdirect3());
		pat &= mmm(hw_const->sg_direct_msb());
	}

	const gsl_rng_type* T;
	gsl_rng* rng;
	/// generates a list of times (uniform distributed)
	void poisson(uint start, uint duration, uint cmddur, double freq, vector<uint>* p);

	void getNumOfSyncs(uint& res)
	{
		readPBC(res, hw_const->sg_sc_syncs());
		res &= mmw(hw_const->sg_sc_syncsw());
	}

	bool getSyncError(void)
	{
		uint res(0);
		readSC(res, hw_const->sg_scsendidle());
		return res & (1 << hw_const->sg_scsyncerr_pos());
	}

	//! get temperature from sensor between Flyspi and Spikey board
	float getTemp();
	//! read workstation from file (needed for SpikeyHAL tests)
	std::string getWorkstationFromFile(std::string filenameWorkstation);
	//! get workstation using serial from USB device and *.cfg files
	std::string getWorkstationFromUsbDevice(Vmoduleusb* device);
	//! get config from file for workstation
	void getConfigFromFile(std::string filenameConfig);
	//! get number of delay lines for FPGA
	uint getChipVersion() { return chipVersion; }
	uint getDelayFpgaIn() { return delayFpgaIn; }
	std::string getWorkStationName() { return work_station_name; }
};

uint intval(float v, uint res, float max);

} // end of namespace spikey2
