// realizes the communication via the nathan system's slow control

#include "common.h" // library includes
#include "idata.h"
#include "sncomm.h"
#include "spikenet.h" //communication and chip classes
#include "sc_sctrl.h"

#include <algorithm>
#include <gsl/gsl_rng.h>
#include <gsl/gsl_randist.h>
#include <cassert>
#include <limits>

static log4cxx::LoggerPtr logger = log4cxx::Logger::getLogger("HAL.Ctr");

using namespace spikey2;

const unsigned int SC_SlowCtrl::adc_start_adr;

// *** BEGIN class SC_SlowCtrl ***

//----------------------------------------------------------------------
SC_SlowCtrl::SC_SlowCtrl(uint time, std::string workstation)
    : SpikenetComm("SC_SlowCtrl"),
      dbg(::Logger::instance()),
      T(gsl_rng_default),
      rng(gsl_rng_alloc(T))
{
	// get workstation from pynn.setup() argument, workstation file, or load first USB device
	if (workstation == "") {
		std::string filenameWorkstation = string(getenv("HOME")) + "/my_stage1_station";
		workstation = getWorkstationFromFile(filenameWorkstation);
	}
#ifndef WITH_FLANSCH
	if (workstation == "") { // workstation file does not exist
		LOG4CXX_INFO(logger, "Opening first available FPGA board");
		io = new Vmoduleusb(usbcomm::note, 0x04b4, 0x1003);
		workstation = getWorkstationFromUsbDevice(io);
		getConfigFromFile(workstation);
	} else { // load station specified in workstation file or pynn.setup()
		getConfigFromFile(workstation);
		LOG4CXX_INFO(logger, "Opening FPGA board with serial " << serial);
		io = new Vmoduleusb(usbcomm::note, 0x04b4, 0x1003, serial);
	}
#else
	io = new Vmodulesim(50023, "vtitan.kip.uni-heidelberg.de");
#endif

	work_station_name = workstation;
	updateHwConst(chipVersion);

	uint muxboardMode;
	switch (boardVersion) {
		case 1:
			muxboardMode = 1;
			break;
		case 2:
			muxboardMode = 3;
			break;
		default:
			string msg = "Invalid spikey board version!";
			LOG4CXX_ERROR(logger, msg);
			throw std::runtime_error(msg);
	}

	for (int i = 0; i < 16; i++)
		ltime[i] = 0;

	// create sp6 class tree
	usb = new Vusbmaster(io);
	// usbmaster knows three clients, must be in this order!!!
	status = new Vusbstatus(usb);
	mem = new Vmemory(usb);
	ocp = new Vocpfifo(usb);
	// ocpfifo clients
	confrom = new Vspiconfrom(ocp);
	gyro = new Vspigyro(ocp);
	wireless = new Vspiwireless(ocp);
	// Spikey stuff - could be consolidated...
	spyctrl = new Vspikeyctrl(ocp);
	spypbm = new Vspikeypbmem(ocp, boardVersion);
	spydc = new Vspikeydelcfg(ocp, boardVersion);
	spy_slowadc = new Vspikeyslowadc(ocp, boardVersion);
	fadc = new Vspikeyfastadc(ocp);
	spydac = new Vspikeydac(ocp, boardVersion);
	muxboard = new Vmux_board(ocp, muxboardMode);

	usedpcktslots = 3;

	// config STDP
	first_proc_corr = 0;
	proc_corr_dist = 20000;
	proc_corr_dist_pre = proc_corr_dist;
	cont_proc_corr = false;
	startrow = 255;
	stoprow = 255;
	tbmax = (1 << hw_const->sc_plut_res()) - 1;

	gsl_rng_env_setup();

	sdrambufvalid = false;
	setState(ok);
	setStime(time);
}

SC_SlowCtrl::~SC_SlowCtrl()
{
	// TP (04.05.2015): supply voltage should ideally be checked in constructor,
	// but this does not work, see issue #1694
	if (boardVersion >= 2) {
		checkSupplyVoltage();
	}

	// free gsl random number generator
	gsl_rng_free(rng);

	delete spyctrl;
	delete spypbm;
	delete spydc;
	delete spy_slowadc;
	delete fadc;
	delete spydac;
	delete muxboard;
	delete confrom;
	delete gyro;
	delete wireless;
	delete status;
	delete mem;
	delete ocp;
	delete usb;
	delete io;
}

//        <-  sci dataout                ->
//  write: data_in  addr(adw-1 downto 0)   0
//  read:  wdata    "                      1
//
//        <-  sci datain                 ->
//  read:  data_out rdata(adw-1 downto 0) nc
//
SpikenetComm::Commstate SC_SlowCtrl::Send(Mode m, IData data, uint del, uint chip, uint syncoffset)
{
	static_cast<void>(syncoffset); // unused parameter
	switch (m) {
		case sync:
			LOG4CXX_ERROR(logger, "ILLEGAL Send Sync in SC_SlowCtrl::Send!");
			return parametererror;
			break;
		case ctrl: // bus control packet
			LOG4CXX_TRACE(logger, "Send Ctrl in SC_SlowCtrl::Send: " << data);
			if (data.isDelay()) { // configure delay lines
				// address of delay line and it's value are contained within the data files of
				// IData.
				// -> first decompose data field.
				uint deladdr = (data.data() >> hw_const->ctl_deladdr_pos()) &
				               mmw(hw_const->ctl_deladdr_width());
				uint delval =
				    (data.data() >> hw_const->ctl_delval_pos()) & mmw(hw_const->ctl_delval_width());
				uint dout =
				    ((chip & mmw(hw_const->chipid_width())) << hw_const->sg_scdelcid_pos()) |
				    ((mmw(hw_const->ctl_delval_width()) & delval) << hw_const->sg_scdelval_pos()) |
				    ((mmw(hw_const->ctl_deladdr_width()) & deladdr)
				     << hw_const->sg_scdeladdr_pos());
				spyctrl->write(hw_const->sg_scdelay(), dout);
			} else if (data.isControl()) {
				uint dout = ((data.reset() << hw_const->sg_scrst_pos()) |
				             (data.cimode() << hw_const->sg_sccim_pos()) |
				             (data.direct_pckt() << hw_const->sg_scdirectout()) |
				             (data.pllreset() << hw_const->sg_scplr_pos()) |
				             (data.vrest_gnd() << hw_const->sg_scvrest_pos()) |
				             (data.vm_gnd() << hw_const->sg_scvm_pos()) |
				             (data.ibtest_meas() << hw_const->sg_scibtest_pos()) |
				             (data.power_enable() << hw_const->sg_scpower_pos()));
				spyctrl->write(hw_const->sg_scmode(), dout);
			} else if (data.isPhsel()) {
				uint dout = ((data.rx_dat0_phase() << hw_const->sg_rxd0_phsel_pos()) |
				             (data.rx_dat1_phase() << hw_const->sg_rxd1_phsel_pos()) |
				             (data.rx_clk0_phase() << hw_const->sg_rxc0_phsel_pos()) |
				             (data.rx_clk1_phase() << hw_const->sg_rxc1_phsel_pos()));
				spyctrl->write(hw_const->sg_scphsel(), dout);
			}
			break;
		case read:
		case write:
			LOG4CXX_ERROR(logger,
			              "Read/Write CI or Event data via SC_SlowCtrl::Send not possible!!!");
			exit(EXIT_FAILURE);
			break;
		default:
			LOG4CXX_ERROR(logger, "Unknown mode in SC_SlowCtrl::Send!");
			return parametererror;
	}
	return ok;
}

SpikenetComm::Commstate SC_SlowCtrl::Receive(Mode m, IData& data, uint chip)
{
	switch (m) {
		case sync:
			LOG4CXX_ERROR(logger, "Nothing to receive for sync in SC_SlowCtrl::Receive");
			return parametererror;
			break;
		case ctrl: // bus control packet
			// read from rcvctrl[chip]
			if (rcvctrl[chip].empty()) {
				LOG4CXX_ERROR(logger, "Received control data queue empty!");
				return readfailed;
			} else {
				data = rcvctrl[chip].front();
				LOG4CXX_TRACE(logger, "Received ctrl: " << hex << data.busTestPattern());
				rcvctrl[chip].pop();
				if (data.isBusTestPattern()) {
					LOG4CXX_TRACE(logger, "Received pattern: " << hex << data.data());
				} else {
					LOG4CXX_ERROR(logger, "Received unknown data in control queue!");
					return readfailed;
				}
			}
			break;
		case read:
		case write:
			LOG4CXX_ERROR(logger,
			              "Read/Write CI or Event data via SC_SlowCtrl::Receive not possible!!!");
			exit(EXIT_FAILURE);
			break;
		default:
			LOG4CXX_ERROR(logger, "Unknown mode in SC_SlowCtrl::Receive");
			return parametererror;
	}
	return ok;
}

// create sideband data packet in transfers file:
SpikenetComm::Commstate SC_SlowCtrl::SendSBD(Mode m, const SBData& data, uint del, uint chip)
{
	static_cast<void>(chip);
	switch (m) {
		case read:
		case write:
			LOG4CXX_TRACE(logger, "Write SC_SlowCtrl::SendSBD");
			if (data.isDac()) {
				writeDac(data);
			} else if (data.isAdc()) {
				SBData r = data;
				readAdc(r);
				adc.push(r);
			} else
				return parametererror;
			break;
		default:
			LOG4CXX_ERROR(logger, "Unknown mode in SC_SlowCtrl::SendSBD");
			return parametererror;
	}
	return ok;
}

// receive supports only adc vals at the moment
SpikenetComm::Commstate SC_SlowCtrl::RecSBD(Mode m, SBData& data, uint chip)
{
	static_cast<void>(chip);
	switch (m) {
		case read:
			// read from adc
			data.setADvalue(adc.front().ADvalue());
			adc.pop();
			break;
		default:
			LOG4CXX_ERROR(logger, "Unknown mode in SC_SlowCtrl::RecSBD");
			return parametererror;
	}
	LOG4CXX_TRACE(logger, "Received: " << data);
	return ok;
}

// access the spikey Slow Control interface's registers
SpikenetComm::Commstate SC_SlowCtrl::writeSC(uint data, uint addr)
{
	LOG4CXX_TRACE(logger, "SC_SlowCtrl::writeSC address:" << hex << addr << " data:" << hex
	                                                      << data);
	spyctrl->write(addr & 0xfff, data);
	return ok;
}

SpikenetComm::Commstate SC_SlowCtrl::readSC(uint& data, uint addr)
{
	LOG4CXX_TRACE(logger, "SC_SlowCtrl::readSC address:" << hex << addr << " ");
	data = spyctrl->read(addr & 0xfff);
	LOG4CXX_TRACE(logger, " data:0x" << hex << data);
	return ok;
}

// access the spikey Playback Memory control registers
SpikenetComm::Commstate SC_SlowCtrl::writePBC(uint data, uint addr)
{
	LOG4CXX_TRACE(logger, "SC_SlowCtrl::writePBC address:" << hex << addr << " data:" << hex
	                                                       << data);
	spypbm->write(addr & 0xfff, data);
	return ok;
}

SpikenetComm::Commstate SC_SlowCtrl::readPBC(uint& data, uint addr)
{
	LOG4CXX_TRACE(logger, "SC_SlowCtrl::readPBC address:" << hex << addr << " ");
	data = spypbm->read(addr & 0xfff);
	LOG4CXX_TRACE(logger, " data: 0x" << hex << data);
	return ok;
}

// function to display
// playback memory content
void SC_SlowCtrl::printPBMem()
{
	bool wtim0, idle, re, wi, wf; // dummies needed for compatibility
	uint startwaddr, endwaddr;
	uint64_t entry;
	uint entries;
	getWadr(startwaddr);
	getCurWadr(endwaddr, wtim0, idle, re, wi, wf);
	entries =
	    (endwaddr & mmw(hw_const->sg_sc_wadrw())) - (startwaddr & mmw(hw_const->sg_sc_wadrw()));
	LOG4CXX_TRACE(logger, "Playback memory write start address is: " << hex << startwaddr);
	LOG4CXX_TRACE(logger, "Playback memory entry count           : " << hex << entries);
	for (uint i = 0; i < entries; i++) {
		entry = (uint64_t)mem->read(2 * (startwaddr + i));
		entry |= (uint64_t)mem->read(2 * (startwaddr + i) + 1) << 32;
		if (entry & 1ULL) { // entry is event packet
			binout(cout, (entry >> (2 * hw_const->sg_ev_evsize() + hw_const->sg_ev_evbase())),
			       hw_const->sg_ev_evsize());
			cout << " ";
			binout(cout, (entry >> (hw_const->sg_ev_evsize() + hw_const->sg_ev_evbase())),
			       hw_const->sg_ev_evsize());
			cout << " ";
			binout(cout, (entry >> (hw_const->sg_ev_evbase())), hw_const->sg_ev_evsize());
			cout << " | ";
			cout << setw(6) << hex
			     << (mmw(hw_const->sg_ev_evsize()) &
			         (entry >> (2 * hw_const->sg_ev_evsize() + hw_const->sg_ev_evbase()))) << " ";
			cout << setw(6) << hex
			     << (mmw(hw_const->sg_ev_evsize()) &
			         (entry >> (hw_const->sg_ev_evsize() + hw_const->sg_ev_evbase()))) << " ";
			cout << setw(6) << hex
			     << (mmw(hw_const->sg_ev_evsize()) & (entry >> (hw_const->sg_ev_evbase()))) << endl;
		} else
			cout << hex << entry << endl;
	}
}

// setup playback memory addresses and insert a dummy delay packet
// if playback memory contains an odd number of entries
void SC_SlowCtrl::setupSend(uint rstartadr, uint chipid)
{
	LOG4CXX_TRACE(logger, "setupSend: PBM pointer: 0x" << hex << pbradr() << ", PB start addr: 0x"
	                                                   << hex << rstartadr);
	uint numread = pbradr() - rstartadr;
	if (numread % 2) {
		LOG4CXX_TRACE(logger, "SC_SlowCtrl::setupSend: inserting dummy delay command at end to "
		                      "obtain even number of entries");
		pbEvtdel(1); // dummy to accomplish even number of playback memory entries
		numread += 1;
	}

	if (dbg.getLevel() == ::Logger::DEBUG3) {
		LOG4CXX_TRACE(logger, "SC_SlowCtrl::setupSend: sdrambuf content:");
		for (uint i = 0; i < numread; i++) {
			LOG4CXX_TRACE(logger, hex << setfill('0') << right << "A: 0x" << setw(8) << i
			                          << " | D: 0x" << setw(16) << (uint64_t)sdrambuf[i]);
		}
	}

	// send buffer to playback memory
	uint maxchunksize = getMaxChunkSize();
	uint chunks = (numread * 2) / maxchunksize + 1;
	uint chunkstart = rstartadr * 2; // vbuf addresses are 32bit aligned
	uint chunksize = 0;
	for (uint chunk = 0; chunk < chunks; chunk++) {
		// cut addresses to fit in chunks
		uint virtChunksize = numread * 2 - (chunkstart - rstartadr * 2);
		if (virtChunksize > maxchunksize)
			chunksize = maxchunksize;
		else
			chunksize = virtChunksize;

		LOG4CXX_TRACE(logger, hex << "SC_SlowCtrl::setupSend: chunks: " << chunks
		                          << ", chunkstart: " << chunkstart
		                          << ", chunksize: " << chunksize);
		Vbufuint_p pbsend = mem->writeBlock(chunkstart, chunksize);
		// copy sdrambuf to Vmemory buffer...
		for (uint i = 0; i < chunksize / 2; i++) {
			pbsend[2 * i] = sdrambuf[i + chunkstart / 2 - rstartadr] & (uint64_t)0xffffffff;
			pbsend[2 * i + 1] =
			    (sdrambuf[i + chunkstart / 2 - rstartadr] >> 32) & (uint64_t)0xffffffff;
			// dbg(::Logger::DEBUG3) << hex << setfill('0') << right << "A: 0x" << setw(8) << i << "
			// | D: 0x" << setw(16) << (uint64_t)sdrambuf[i+chunkstart/2-rstartadr];
		}
		mem->doWB();
		chunkstart = chunkstart + chunksize;
	}
	sdrambufvalid = false;

	LOG4CXX_TRACE(logger, "SC_SlowCtrl::setupSend: set PBM to read num = 0x" << hex
	                                                                         << (numread >> 1));

	setNumRead(numread >> 1); // read client "counts" in 128bit accesses -> div. number to be read
	                          // by 2!
	setRadr(rstartadr);
	setChipid(chipid);
}

void SC_SlowCtrl::startPlayback()
{
	LOG4CXX_DEBUG(logger, "Starting playback memory");
	writePBC((1 << hw_const->sg_sb_start_ramsm()), hw_const->sg_sc_status());
}

void SC_SlowCtrl::stopPlayback()
{
	string msg = "Playback memory stop no longer supported!";
	dbg(::Logger::ERROR) << msg << Logger::flush;
	throw std::runtime_error(msg);
}

void SC_SlowCtrl::setAnaMux(std::bitset<3> inputselect)
{
	if (inputselect == 0)
		setAnaMux(Vmux_board::OUTAMP_0);
	else if (inputselect == 1)
		setAnaMux(Vmux_board::OUTAMP_1);
	else if (inputselect == 2)
		setAnaMux(Vmux_board::OUTAMP_2);
	else if (inputselect == 3)
		setAnaMux(Vmux_board::OUTAMP_3);
	else if (inputselect == 4)
		setAnaMux(Vmux_board::OUTAMP_4);
	else if (inputselect == 5)
		setAnaMux(Vmux_board::OUTAMP_5);
	else if (inputselect == 6)
		setAnaMux(Vmux_board::OUTAMP_6);
	else if (inputselect == 7)
		setAnaMux(Vmux_board::OUTAMP_7);
	else if (inputselect == 8)
		setAnaMux(Vmux_board::MUX_GND);
}

void SC_SlowCtrl::setAnaMux(Vmux_board::mux_input inputselect)
{
	muxboard->set_Mux(inputselect);
}

//******** PBmem related methods ********

// writes sync command to time "time" into playback memory
void SC_SlowCtrl::pbSync(uint time, uint del, uint offset)
{
	uint addr = pbradr();
	LOG4CXX_TRACE(logger, "SC_SlowCtrl::pbSync: to time: " << hex << time << ", delay: " << del);
	// sync FPGA to (time*2)+6 as controller needs 3 bus cycles (6 400MHz cycles) to load fpga
	// counter
	writeBuf(((((time << 1) + 6 + offset) << (hw_const->ci_cmd_width() + 1 +
	                                          hw_const->sg_ev_citimew() + hw_const->sg_ev_comw())) |
	          (hw_const->ci_synci()
	           << (1 + hw_const->sg_ev_citimew() + hw_const->sg_ev_comw())) | // sync command
	          0), // sc rw bit = 0 for write
	         addr);
	setPbradr(addr + 1);
	if (del > 1)
		pbEvtdel(del - 1); // the sync command takes one of the bus cycles
	if (del < 1)
		LOG4CXX_WARN(logger, "delay too small for issuing command");
	lsystime = time + (offset >> 1);
	levtimeclk = -2;

	for (uint i = 0; i < 16; i++)
		ltime[i] = 0;
}

// writes delay command (i.e. NOP for "del" cycles)
void SC_SlowCtrl::pbEvtdel(uint del)
{
	if (del) {
		// TP (15.05.2015): AG added this, but breaks test_rate_in.py in pynnhw, see issue #1715
		// while(del > (1<<hw_const->sg_ev_timew())){
		// LOG4CXX_TRACE(logger, "SC_SlowCtrl::pbEvtdel: inserting " << dec << del << " cycles");
		// uint addr=pbradr();
		//// the vhdl code takes one cycle to decode the command, therefore decrement del by 1
		// writeBuf((((((1<<hw_const->sg_ev_timew())-1)&mmw(hw_const->sg_ev_timew()))<<hw_const->sg_ev_time())|
		//(hw_const->sg_ev_rdcom_ec()&mmw(hw_const->sg_ev_comw())))&mmw(hw_const->sg_ev_timew()+hw_const->sg_ev_time()),
		//addr); // empty event command for spikey_sei command decoder
		// setPbradr(addr+1);
		// del -= (1<<hw_const->sg_ev_timew());
		//}

		LOG4CXX_TRACE(logger, "SC_SlowCtrl::pbEvtdel: inserting " << dec << del << " cycles");
		uint addr = pbradr();
		// the vhdl code takes one cycle to decode the command, therefore decrement del by 1
		writeBuf(((((del - 1) & mmw(hw_const->sg_ev_timew())) << hw_const->sg_ev_time()) |
		          (hw_const->sg_ev_rdcom_ec() & mmw(hw_const->sg_ev_comw()))) &
		             mmw(hw_const->sg_ev_timew() + hw_const->sg_ev_time()),
		         addr); // empty event command for spikey_sei command decoder
		setPbradr(addr + 1);
	}
}

// generates spikey command interface access
void SC_SlowCtrl::pbCI(Mode mode, IData& data, uint del)
{
	uint addr = pbradr();
	LOG4CXX_TRACE(logger, "SC_SlowCtrl::pbCI, mode: " << hex << mode << ", command: " << data.cmd()
	                                                  << ", data: " << data.data()
	                                                  << ", delay: " << del);
	writeBuf(((data.data() & mmw(hw_const->sg_ev_cidataw()))
	          << (hw_const->sg_ev_cidata() + hw_const->ci_cmd_width() + 1)) | // the '1' stands for
	                                                                          // the rwb-bit
	             ((data.cmd() & mmw(hw_const->ci_cmd_width())) << (hw_const->sg_ev_cidata() + 1)) |
	             (((mode == write) ? hw_const->ci_writei() : hw_const->ci_readi())
	              << hw_const->sg_ev_cidata()),
	         addr);
	// as the first delay cycle is the command itself we need one less for the pbEvtdel command
	setPbradr(addr + 1);
	if (del > 1)
		pbEvtdel(del - 1);
	if (del < 1)
		LOG4CXX_WARN(logger, "delay too small for issuing command");
}

// writes event command
void SC_SlowCtrl::pbEvtcmd(uint lastevcnt, uint numpackets, uint time)
{
	uint addr = pbradr();
	uint lastevmask = 0;
	for (uint i = 0; i < lastevcnt; i++)
		lastevmask |= 1 << i;
	// the vhdl code takes one cycle to decode the command, therefore decrement time by 1
	uint64_t evcmd =
	    ((lastevmask << hw_const->sg_ev_evmask()) |
	     (((time - 1) & mmw(hw_const->sg_ev_timew())) << hw_const->sg_ev_time()) |
	     ((numpackets & mmw(hw_const->sg_ev_numevw())) << hw_const->sg_ev_numev()) |
	     (hw_const->sg_ev_rdcom_ec() & mmw(hw_const->sg_ev_comw()))); // event command for
	                                                                  // spikey_sei command decoder
	writeBuf(evcmd, addr);
	setPbradr(addr + 1);
}

// writes event "packet" into playback memory
void SC_SlowCtrl::pbEvtpct(uint evt1time, uint evt1addr, uint evt2time, uint evt2addr,
                           uint evt3time, uint evt3addr)
{
	uint addr = pbradr();
	writeBuf(
	    (((evt3addr & mmw(hw_const->sg_eadrwidth()))
	      << (3 * hw_const->sg_datawidth() - hw_const->sg_eadrwidth() + hw_const->sg_ev_evbase())) |
	     ((evt3time & mmw(hw_const->sg_etimewidth() + hw_const->sg_efinewidth()))
	      << (2 * hw_const->sg_datawidth() + hw_const->sg_ev_evbase())) |
	     ((evt2addr & mmw(hw_const->sg_eadrwidth()))
	      << (2 * hw_const->sg_datawidth() - hw_const->sg_eadrwidth() + hw_const->sg_ev_evbase())) |
	     ((evt2time & mmw(hw_const->sg_etimewidth() + hw_const->sg_efinewidth()))
	      << (1 * hw_const->sg_datawidth() + hw_const->sg_ev_evbase())) |
	     ((evt1addr & mmw(hw_const->sg_eadrwidth()))
	      << (1 * hw_const->sg_datawidth() - hw_const->sg_eadrwidth() + hw_const->sg_ev_evbase())) |
	     ((evt1time & mmw(hw_const->sg_etimewidth() + hw_const->sg_efinewidth()))
	      << hw_const->sg_ev_evbase()) |
	     1),
	    addr); // direct enconding of 1 for spikey_sei command decoder
	setPbradr(addr + 1);
}


//************ translation functions IData <-> Pbmem ****************

// converts received ram format to IData
// evmask is a three bit mask coding for the three event fields in the ram data
// if a mask bit and the according event are both valid it is returned in IData and the mask bit is
// cleared
// if no more events are valid, the function returns a zero mask

void SC_SlowCtrl::translate(const uint64_t& r, IData& d, uint& evmask)
{
	uint evtime;
	// (ag): Idea to catch the case where an LSB bit-flip might exist:
	// When a time stamp has been received, set lsystime to the received time minus half of
	// max(spikey_time).
	// As the time stamp is generated if min. half of max(spikey_time) has elapsed since the last
	// event,
	// time can be determined in the way below...

	// check for event
	if (r & 1ULL) {
		// event loop
		for (uint i = 0; i < 3; ++i) {
			if (levtimeclk >= -1) { // start processing events after first time stamp has been
			                        // received
				if (evmask & (1 << i)) { // mask bit set?
					evmask &= ~(1 << i); // clear mask bit
					d.setNeuronAdr() =
					    (r >> (hw_const->sg_datawidth() - hw_const->sg_eadrwidth() +
					           i * hw_const->sg_ev_evsize() + hw_const->sg_ev_evbase())) &
					    mmw(hw_const->sg_eadrwidth());
					if ((d.neuronAdr() & mmw(hw_const->sg_eadrwidth() - 1)) <
					    3 * 64) { // valid neuron? - (ag): msb (block select bit) not evaluated!
						d.setEvent();
						evtime = (r >> (i * hw_const->sg_ev_evsize() + hw_const->sg_ev_evbase())) &
						         mmw(hw_const->sg_etimewidth() + hw_const->sg_efinewidth());
						uint evtimeclk = evtime >> hw_const->sg_efinewidth();

						if (levtimeclk >= 0) {
							if ((levtimeclk & 0xf0) >
							    (int)(evtimeclk & 0xf0)) { // ignore lower nibble of eventclk, might
							                               // not be in ascending order
								lsystime += (1 << hw_const->sg_etimewidth());
								LOG4CXX_TRACE(logger, "SC_SlowCtrl::translate: Systime incremented:"
								                          << hex << lsystime);
							} else {
								// A false wrap around at 0xf0->0x00 has occured, if the dist.
								// between two events
								// equals 0xf0 and no time stamp has been received (is the case in
								// this condition).
								if (((evtimeclk & 0xf0) - (levtimeclk & 0xf0)) & 0xf0 == 0xf0) {
									lsystime -= (1 << hw_const->sg_etimewidth());
									LOG4CXX_WARN(logger, "SC_SlowCtrl::translate: Systime "
									                     "decremented as no timestamp received! "
									                     "Systime: "
									                         << dec << lsystime);
								}
							}
						} else {
							if ((lsystime & mmw(hw_const->sg_etimewidth())) <= evtimeclk)
								lsystime -= 1 << hw_const->sg_etimewidth(); // subtract systime
								                                            // overflow
							LOG4CXX_TRACE(logger, "Systime from systime event: " << hex << lsystime
							                                                     << " evtime:"
							                                                     << evtimeclk);
						}
						levtimeclk = evtimeclk;

						d.setTime() = evtime | ((lsystime & ~mmw(hw_const->sg_etimewidth()))
						                        << hw_const->sg_efinewidth());
						break;
					}
				}
			}
			d.clear(); // set type to empty
		}
	} else {
		if (((r >> (hw_const->sg_ev_cidata() + 1)) & mmw(hw_const->ci_cmd_width())) ==
		    hw_const->ci_synci()) { // process stored time stamp
			d.clear();
			lsystime = r >> (hw_const->sg_ev_cidata() + hw_const->ci_cmd_width() + 1) &
			           mmw(hw_const->sg_systimewidth());
			levtimeclk = -1; // systime event packet is kind of resync, like at the beginning of the
			                 // playback cycle
			LOG4CXX_TRACE(logger, "System time event: " << hex << lsystime << " ");
			return;
		} else { // regular CI packet
			d.setCI();
			d.setData() = (r >> (hw_const->sg_ev_cidata() + hw_const->ci_cmd_width() + 1)) &
			              mmw(hw_const->sg_ev_cidataw());
			d.setCmd() = (r >> (hw_const->sg_ev_cidata() + 1)) & mmw(hw_const->ci_cmd_width());
		}
	}
	LOG4CXX_TRACE(logger, "translate " << hex << r << " " << d);
}


// TP: this function is copied from SynapseControl::set_LUT
// set LUT values
void SC_SlowCtrl::set_LUT(std::vector<int> _lut)
{
	assert(_lut.size() == 2 * (tbmax + 1));
	for (int i = 0; i < 2 * (tbmax + 1); i++) {
		assert(_lut[i] <= tbmax);
		assert(_lut[i] >= 0);
	}
	lut.resize(2 * (tbmax + 1));
	lut = _lut;
}

// get LUT values
std::vector<int> SC_SlowCtrl::get_LUT()
{
	return lut;
}

// TP: this function is copied from SynapseControl::gen_plut_data
// generate LUT commands
// for each LUT entry weight has to be repeated "no of correlation priority encoders"
uint SC_SlowCtrl::gen_plut_data(float nval)
{
	uint out = 0;
	for (uint j = 0; j < hw_const->sc_cprienc(); j++) {
		out |= ((uint)nval) << (j * hw_const->sc_plut_res());
	}
	return out;
}


// TP: this function is copied from SynapseControl::fill_plut
// creates and writes look-up table, identity of custom LUT
void SC_SlowCtrl::fill_plut(uint delay, bool identity)
{
	float nval;
	int i;
	std::vector<uint> plut(2 * (tbmax + 1)); // acausal and causal in one vector

	if (identity) {
		for (i = 0; i < (2 * (tbmax + 1)); i++) {
			nval = i & mmw(hw_const->sc_plut_res()); // hardy modulo
			plut[i] = gen_plut_data(nval);
			LOG4CXX_TRACE(logger, "plut i=" << i << " nval=" << nval << " out=" << plut[i]);
		}
	} else {
		assert(lut.size() == 2 * (tbmax + 1));
		for (i = 0; i < (2 * (tbmax + 1)); i++) {
			plut[i] = gen_plut_data(lut[i]);
			LOG4CXX_TRACE(logger, "plut i=" << i << " nval=" << lut[i] << " out=" << plut[i]);
		}
	}
	// transfer to chip
	for (i = 0; i < (2 * (tbmax + 1)); i++) {
		IData write_plut(hw_const->ci_synrami(),
		                 hw_const->sc_cmd_plut() | ((i | ((uint64_t)plut[i] << hw_const->sc_aw()))
		                                            << hw_const->sc_commandwidth()));
		pbCI(SpikenetComm::write, write_plut, delay);
	}
}


/* 	generates sequences of event commands and according packets out of an IData-Vector containing
events.
(ag):	Current Functionality:
        * Low nibble of time information (timebin) is not processed here but just passed.
        * To catch the case where only sparse events occur, an event command is issued in every
case, where
            the event to be generated is more than 256-getEvtLatency() clock cycles in the future of
the start
            of the previous event command.
        *	The VHDL code only processes one event stream out of the Playback Memory (max is 3).
Therefore the pbEvt command
            will generate at least one event packet (and eventually an event command, before) for
each event.
        *	Time values contained within the IDatas have to be 32bit wide. This will probably only
be possible
            for software-generated spike-trains but for now it makes time processing easier.
        *	The pbEvt command stores the last time value for each of spikey's event in fifos.
            Strategy to prevent 256-cycle-deadlocks within these fifos:
            If an event is to be transmitted that has the same time as the last one, then it's time
will be
            increased, otherwise an adequate delay will be introduced.
        *	This requires two events in the vector to have identical or increasing values!
        *	To make everything work correctly, take care, that the event times start at values that
are
            larger than the time you synced to plus getEvtLatency(chip) */
//
// Chronology:
//   1. define parameters
//   for each event do
//     2. insert correlation processing if possible
//     3. insert event in existing packet, if full, pack it and open new one in next loop
//
// TP (03.05.2011): Note that clock is running with 100MHz/200MHz instead of 200MHz/400MHz
// 20000 200MHz clock cycles = 1s in biology at speedup 10^4
void SC_SlowCtrl::pbEvt(vector<IData>& evt, uint& newstime, uint stime, uint chip)
{
	//***** 1. DEFINE PARAMETERS *****//

	uint systime = (stime + 1) << 1; // System time in 400MHz cycles (one cycle for evtcmd
	                                 // included).
	                                 // Needed to estimate state of spikey's event fifos.
	uint cstart = stime << 1;  // current event command's start time (first will start immediately)
	uint buf;                  // spikey's event in fifo addressed by the current event
	uint lbuf1 = 0, lbuf2 = 0; // stores buffer address of resp. events
	uint ptim = 0;             // high nibble time of currently processed packet
	uint packed = 0;           // monitors number of compressible events
	bool gencmd = true;        // if true, generate event command, because no more packing possible
	// initialized "true" to enable neuron reset insertion before first command.
	bool is2early = false;

	LOG4CXX_DEBUG(logger, "Encoding spikes");

	// process (+ record) and reset the correlation flags within the spike train
	if (cont_proc_corr) {
		LOG4CXX_DEBUG(logger, "SC_SlowCtrl::pbEvt: proc_corr rows: " << startrow << "->"
		                                                             << stoprow);
	}

	IData p_corr(hw_const->ci_synrami(),
	             (hw_const->sc_cmd_pcorc() | ((startrow | ((uint64_t)stoprow << hw_const->sc_aw()))
	                                          << hw_const->sc_commandwidth())));

	uint next_corr_proc = first_proc_corr - getEvtLatency() + 5; // +5 (AGs guess): control command
	                                                             // is faster than event command;
	// last process correlation after second last spike (last spike just marks end of simulation)
	// otherwise weights may drift
	int last_spike_time = 0;
	if (evt.size() >= 2) {
		last_spike_time = (evt[evt.size() - 2].time() >> hw_const->ev_tb_width());
	}
	int proc_success = 0;
	int proc_dropped = 0;

	//***** EVENT PROCESSING *****//

	uint nexti;
	uint ecdel;
	vector<IData> cv;
	IData current;

	// clear BEFORE event processing! >> otherwise mem leak!
	sendev[chip].clear();

	// process correlation before experiment start with LUTs configured to identity
	// this clears STDP capacitors "drifting" over time
	if (cont_proc_corr) {
		// time for writing LUT
		uint delay_LUT_entry = 3;
		uint delay_LUT = ((tbmax + 1) * 2) * (delay_LUT_entry << 1);
		// experiment start - time for processing the selected array of synapses - 2 times writing
		// LUT
		assert(next_corr_proc > proc_corr_dist_pre + 2 * delay_LUT + systime +
		                            (1 << hw_const->ev_clkpos_width())); // enough time before
		                                                                 // spikes?
		uint delay_exp = next_corr_proc - proc_corr_dist_pre - 2 * delay_LUT - systime -
		                 (1 << hw_const->ev_clkpos_width());
		delay_exp &= 0xfffffffe; // ensure even delay values!
		// time for processing synapses
		uint proc_time = proc_corr_dist_pre - (basedelay << 1);
		// fill with delays up to experiment start minus time for STDP preparation
		if (delay_exp > 0) {
			pbEvtdel(delay_exp >> 1);
			systime += delay_exp;
		};
		// write identity LUT
		fill_plut(delay_LUT_entry, true);
		systime += delay_LUT;
		// process synapses
		pbCI(SpikenetComm::write, p_corr, basedelay);
		systime += (basedelay << 1);
		// time for synapse processing
		pbEvtdel(proc_time >> 1);
		systime += proc_time;
		// write custom LUT
		fill_plut(delay_LUT_entry, false);
		systime += delay_LUT;
		cstart = systime - (basedelay << 1);
		next_corr_proc += proc_corr_dist; // leave out process correlation at beginning of
		                                  // experiment
	}

	LOG4CXX_TRACE(logger, "SC_SlowCtrl::pbEvt: Started generation of "
	                          << dec << evt.size() << " events at systime 0x" << hex << systime);
	for (uint i = 0; i < evt.size(); i++) {
		current = evt[i];
		uint cnormedtime = (current.time() >> hw_const->ev_tb_width()) -
		                   getEvtLatency(); // current event's time stamp normed on systime
		                                    // (diversification of 4 bit excluded)

		// check if systime becomes late relative to normed event time
		if (systime > cnormedtime) {
			LOG4CXX_TRACE(logger, "Event dropped (event rate exceeds link capacity) -> Systime: "
			                          << dec << systime << ", Normed time: " << cnormedtime
			                          << ", Event no. " << dec << i << ": " << current
			                          << ". - dropping event.");
			errev[chip].push_back(current);
			continue;
		}

		LOG4CXX_TRACE(logger, "SC_SlowCtrl::pbEvt: Event for address " << hex << current.neuronAdr()
		                                                               << ", time " << hex
		                                                               << current.time());
		while (true) { // try to insert event

			// Current buffer is selected by three addr msb times 2 plus lsb of event time.
			buf = ((current.neuronAdr() >> hw_const->ev_bufaddrwidth()) << 1) |
			      ((current.time() >> hw_const->ev_tb_width()) & 1);

			if (ltime[buf] < (current.time() >>
			                  hw_const->ev_tb_width())) { // no simultaneous events, no problem,
			                                              // otherwise increase event time
				if (current.time() != evt[i].time()) {
					LOG4CXX_TRACE(logger, "SC_SlowCtrl::pbEvt: overall increased time for synapse #"
					                          << hex << current.neuronAdr());
					LOG4CXX_TRACE(logger, " by " << hex << ((current.time() - evt[i].time()) >>
					                                        hw_const->ev_tb_width()) << " !");
				}
				gencmd = false;

				//***** 2. INSERT CORRELATION PROCESSING *****//

				// Systime and cnormedtime are decremented by 2 event clock cycles to ignore the
				// already included time for initial event command.
				// A neuron reset may only be inserted between completed event commands.
				// A previously finished command is flagged by cv.size()==0.
				// cstart=systime - 2
				LOG4CXX_TRACE(logger, "systime: " << systime
				                                  << "; next proc corr: " << next_corr_proc
				                                  << "; next spike: " << cnormedtime);

				// is time for process correlation and last packet finished
				if (cnormedtime > (systime + (basedelay << 1)) && cv.size() == 0 &&
				    cont_proc_corr) {
					// drop correlation processings that are too late
					while (next_corr_proc < cstart &&
					       next_corr_proc - proc_corr_dist < last_spike_time) {
						LOG4CXX_WARN(
						    logger, "SC_SlowCtrl::pbEvt: dropped process correlation, should be at "
						                << next_corr_proc);
						proc_dropped++;
						next_corr_proc += proc_corr_dist;
					}

					while ((next_corr_proc >= cstart && cont_proc_corr) // not too late
					       && (next_corr_proc + (basedelay << 1) <
					           cnormedtime - 2)) { // enough time until next spike

						// insert appropriate delay
						// avoid unneccessary insertion in case of one 400MHz cycle difference by
						// addition of 1
						if (cstart < next_corr_proc + 1) {
							pbEvtdel((next_corr_proc - cstart) >> 1);
							LOG4CXX_TRACE(
							    logger,
							    "SC_SlowCtrl::pbEvt: process correlation: Delay inserted: 0x"
							        << hex << ((next_corr_proc - (systime - 2)) >> 1)
							        << ". Systime now: 0x" << hex << systime);
						}

						// process correlation
						pbCI(SpikenetComm::write, p_corr, basedelay);

						// correct system time and next command's start time
						cstart = next_corr_proc + (basedelay << 1);
						systime = cstart + 2; // incr systime to account for event command delay
						is2early =
						    (cnormedtime > (cstart + (1 << hw_const->ev_clkpos_width()) -
						                    (1 << hw_const->ev_timelsb_width()))); // first term
						                                                           // accounts for
						                                                           // clock wrap
						LOG4CXX_TRACE(logger, "SC_SlowCtrl::pbEvt: inserted process correlation at "
						                          << next_corr_proc);
						proc_success++;

						if (next_corr_proc >= last_spike_time) {
							cont_proc_corr = false; // deactivate, last spike was sent
							LOG4CXX_TRACE(logger, "SC_SlowCtrl::pbEvt: last process correlation at "
							                          << next_corr_proc);
						}

						// set next neuron reset:
						next_corr_proc += proc_corr_dist;
					}
				}

				//***** 3. HERE DOES THE EVENT PACKING START *****//

				// Insert delay, if distance to next event is too large. Only even delay numbers in
				// terms of 400MHz cycles
				// can be generated. Therefore, the max. distance is reduced by 1 to cover odd
				// distances, which are
				// rounded off to even values within the loop.
				while ((cnormedtime > (cstart + (1 << hw_const->ev_clkpos_width()) -
				                       (1 << hw_const->ev_timelsb_width()) - 1) ||
				        is2early) &&
				       cv.size() == 0) {
					uint d;
					// getEvtLatency is included to get maximum margin. As the actual latency might
					// be up to four cycles smaller
					// depending on phase between FPGA and Spikey, I consider this by decrementing
					// by 4.
					if (cnormedtime > cstart + (1 << hw_const->sg_ev_timew()))
						d = (1 << hw_const->sg_ev_timew()) - 1; // add command with max delay

					// add command that ends with half of max delay before next event
					// -> half of max to avoid unneccessary subsequent delay commands and to have
					// reasonable margin
					else
						d = cnormedtime - (1 << hw_const->ev_clkpos_width()) / 2 - cstart;
					d &= 0xfffffffe;  // ensure even delay values!
					pbEvtdel(d >> 1); // shift right as VHDL code counts in 200MHz cycles
					cstart += d;
					systime = cstart + 2; // + 2 cycles for event command
					is2early = false;     // not early any more...
					LOG4CXX_TRACE(logger, "SC_SlowCtrl::pbEvt: Delay inserted: 0x"
					                          << hex << ((d + 1) >> 1) << ". Systime now: 0x" << hex
					                          << systime);
				}

				nexti = i < evt.size() - 1 ? i + 1 : i; // index of next event to be processed (used
				                                        // if can not be packed now)

				// first check, if event would be too early and prevent further packing if true.
				// it is too early, if the high nibble of the time stamp is identical
				// to the previous event's one, but the actual time is one Spikey counter-wraparound
				// later.
				is2early = (cnormedtime > (cstart + (1 << hw_const->ev_clkpos_width()) -
				                           (1 << hw_const->ev_timelsb_width())));

				// then check, if the current event may be packed together with the previous ones.
				LOG4CXX_TRACE(logger, "SC_SlowCtrl::pbEvt: Event " << current << " will be... ");
				// LOG4CXX_TRACE(logger, "lbuf1 "<<lbuf1<<" lbuf2 "<<lbuf2<<" buf
				// "<<((current.neuronAdr()&0xfc0)+(current.time()&0x10))<<" nibble "<<ptim<<" time
				// "<<(current.time()&0xf00));
				switch (packed) {
					case 0:
						if (!is2early) {
							systime += 2; // add 2 cycles for the current event packet
							cv.push_back(current);
							ltime[buf] = current.time() >> hw_const->ev_tb_width();
							lbuf1 = (current.neuronAdr() & 0xfc0) + (current.time() & 0x10);
							ptim = (current.time() & 0xf00);
							packed = 1;

							LOG4CXX_TRACE(logger, " 1st in packet");
							break;
						}
					case 1:
						if (!is2early && (usedpcktslots >= 2)) {
							if (lbuf1 !=
							        ((current.neuronAdr() & 0xfc0) + (current.time() & 0x10)) &&
							    (ptim == (current.time() & 0xf00))) {
								cv.push_back(current);
								ltime[buf] = current.time() >> hw_const->ev_tb_width();
								lbuf2 = (current.neuronAdr() & 0xfc0) + (current.time() & 0x10);
								packed = 2;

								LOG4CXX_TRACE(logger, " 2nd in packet");

								break;
							}
						}
					case 2:
						if (!is2early && (usedpcktslots >= 3)) {
							if (lbuf1 !=
							        ((current.neuronAdr() & 0xfc0) + (current.time() & 0x10)) &&
							    lbuf2 !=
							        ((current.neuronAdr() & 0xfc0) + (current.time() & 0x10)) &&
							    (ptim == (current.time() & 0xf00))) {
								cv.push_back(current);
								ltime[buf] = current.time() >> hw_const->ev_tb_width();
								packed = 0;

								LOG4CXX_TRACE(logger, " 3rd in packet");
								break;
							}
						}

						// if this point is reached, the current event does not fit into the packet.
						// no further packing is possible and an event command has to be generated.
						nexti = i;  // current event won't be included in current command -> will be
						            // in the next one!
						packed = 0; // restart counting after command generation
						gencmd = true;
						LOG4CXX_TRACE(logger, " packed later (is early: " << is2early << ")");
						break;

					default:
						LOG4CXX_ERROR(logger, "Internal error in SC_SlowCtrl::pbEvt.\n");
						exit(-1); // this will never occur!
				}

				// finish current event command, if time to next event too large; max. number of
				// events
				// for command has been reached; gencmd is true or no more events are available
				if (evt[nexti].time() >> hw_const->ev_tb_width() >
				        cstart + (1 << hw_const->sg_ev_timew()) + getEvtLatency() ||
				    !(cv.size() < (1 << hw_const->sg_ev_numevw()) - 1) || i == evt.size() - 1 ||
				    gencmd) {

					// evt command with appropriate size and delay
					ecdel = (systime - cstart) >> 1;
					uint ecsize = (cv.size() % 3) ? (cv.size() / 3 + 1) : cv.size() / 3;
					uint evmask = (cv.size() % 3) ? cv.size() % 3 : 3;
					LOG4CXX_TRACE(logger, "SC_SlowCtrl::pbEvt: Ev. cmd at systime 0x"
					                          << hex << cstart << ": " << dec << cv.size()
					                          << " events. Time: 0x" << hex << ecdel);
					pbEvtcmd(evmask, ecsize, ecdel);

					// generate playback memory "packets" containing one event each
					for (uint j = 0; j < cv.size(); j += usedpcktslots) {
						bool useSlot1, useSlot2;

						useSlot1 = (j + 1 < cv.size()) && (usedpcktslots >= 2);
						useSlot2 = (j + 2 < cv.size()) && (usedpcktslots >= 3);

						pbEvtpct(cv[j].time(), cv[j].neuronAdr(), // event 1 data
						         useSlot1 ? cv[j + 1].time() : 0,
						         useSlot1 ? cv[j + 1].neuronAdr() : 0, // event 2 data
						         useSlot2 ? cv[j + 2].time() : 0,
						         useSlot2 ? cv[j + 2].neuronAdr() : 0); // event 3 data

						// use this vector for event checking only if playback mem is used!!!
						uint s;
						if (j + usedpcktslots < cv.size())
							s = usedpcktslots % 3;
						else
							s = cv.size() % 3; // usedpcktslots;

						// generate sent events vector; missing BREAK is intentional!
						switch (s) {
							case 0:
								LOG4CXX_TRACE(logger, "SC_SlowCtrl::pbEvt: 3rd event of packet: "
								                          << cv[j + 2]);
								cv[j + 2].setTime() =
								    cv[j + 2].time() +
								    ((2 * hw_const->el_depth() + hw_const->el_offset()) << 4);
								sendev[chip].push_back(cv[j + 2]);

							case 2:
								LOG4CXX_TRACE(logger, "SC_SlowCtrl::pbEvt: 2nd event of packet: "
								                          << cv[j + 1]);
								cv[j + 1].setTime() =
								    cv[j + 1].time() +
								    ((2 * hw_const->el_depth() + hw_const->el_offset()) << 4);
								sendev[chip].push_back(cv[j + 1]);

							case 1:
								LOG4CXX_TRACE(logger,
								              "SC_SlowCtrl::pbEvt: 1st event of packet: " << cv[j]);
								cv[j].setTime() =
								    cv[j].time() +
								    ((2 * hw_const->el_depth() + hw_const->el_offset()) << 4);
								sendev[chip].push_back(cv[j]);
						}
					}
					cstart = systime; // next cmd starts at current systime
					systime += 2;     // incr systime to include the next event command
					cv.clear();
				}
				if (gencmd)
					continue; // current event couldn't be packed into command - pack it in next
					          // command!
				break;
			}
			// simultaneous event? increase event's time and try again
			current.setTime() = current.time() + (1 << hw_const->ev_tb_width());
		}
		newstime = systime >> 1;
	}

	if (errev[chip].size() > 0) {
		LOG4CXX_WARN(logger, "Number of lost input spikes due to limited input bandwidth: "
		                         << errev[chip].size() << " of " << evt.size() << " ("
		                         << 100.0 * errev[chip].size() / evt.size() << "%)");
	}

	// insert delay command to let potentially filled event out buffers run empty.
	// max depth is 128; use 128 cycles.
	uint empty_cycles = 128;
	pbEvtdel(empty_cycles);
	newstime += empty_cycles << 1;

	if (proc_success > 0) {
		LOG4CXX_INFO(logger, "Inserted " << proc_success + proc_dropped
		                                 << " correlation processings into spike train");
	}

	if (proc_dropped > 0) {
		LOG4CXX_WARN(logger, "SC_SlowCtrl::pbEvt: " << proc_dropped << " of "
		                                            << proc_success + proc_dropped
		                                            << " correlation processings dropped");
	}
}

void SC_SlowCtrl::readAdc(SBData& d, uint channel)
{
	// This function reads the slow ADC on the flyspi-spikey board
	// and uses an interesting method to communicate the result!
	// It sets the result field in the parameter d of type SBData.
	// This datum will be pushed to an SBData vector by SendSBD.
	float voltage;
	if (boardVersion == 1) {
		voltage = spy_slowadc->readVoltage();
	} else if (boardVersion == 2) {
		voltage = spy_slowadc->readVoltage(channel);
	}
	d.setADvalue(voltage);
	LOG4CXX_TRACE(logger, "slow ADC channel " << channel << ": " << dec << voltage << "V");
}

// ****** end additional functions for event generation in Playback memory
// (JS) to clean up the whole class hirarchy, these methods have been moved to sc_sctrl

void SC_SlowCtrl::checkSupplyVoltage()
{
	SBData data;
	readAdc(data, 0);
	float supply_voltage = data.ADvalue() * 2.0; // measured value is half of real value, because of
	                                             // voltage divider
	if (supply_voltage < (supply_voltage_limit)) {
		std::stringstream msg_stream;
		msg_stream << "Supply voltage too low (is: " << supply_voltage
		           << "V should at least: " << supply_voltage_limit << "V)";
		string msg = msg_stream.str();
		LOG4CXX_ERROR(logger, msg);
		throw std::runtime_error(msg);
	}
}

// write dac packet to slow control
void SC_SlowCtrl::writeDac(const SBData& d)
{
	spydac->enableReference();

	switch (d.getADtype()) {
		case SBData::irefdac:
			LOG4CXX_TRACE(logger, "Setting IREFDAC " << d.ADvalue());
			spydac->setCurrent_uA(Vspikeydac::IREFDAC, d.ADvalue());
			break;

		case SBData::vcasdac:
			LOG4CXX_TRACE(logger, "Setting VCASDAC " << d.ADvalue());
			spydac->setVoltage(Vspikeydac::VCASDAC, d.ADvalue());
			break;

		case SBData::vm:
			LOG4CXX_TRACE(logger, "Setting VM " << d.ADvalue());
			spydac->setVoltage(Vspikeydac::VM, d.ADvalue());
			break;

		case SBData::vrest:
			LOG4CXX_TRACE(logger, "Setting VREST " << d.ADvalue());
			spydac->setVoltage(Vspikeydac::VREST, d.ADvalue());
			break;

		case SBData::vstart:
			LOG4CXX_TRACE(logger, "Setting VSTART " << d.ADvalue());
			spydac->setVoltage(Vspikeydac::VSTART, d.ADvalue());
			break;
		default:
			assert(false);
	}
}

/**
 * Generates a list of times uniformly distributed
 *
 * @param   start      start time
 * @param   duration   duration
 * @param   frequency  (mean of poisson process) frequency of insertions
 * @return  vector of times
 */
void SC_SlowCtrl::poisson(uint start, uint duration, uint cmddur, double freq, vector<uint>* p)
{
	// ported pyNN.hardware.poisson (python) code to C++ using GSL

	assert(duration > 0);
	// random number from poission distribution with mean "duration/1000*freq"
	uint N = gsl_ran_poisson(rng, duration * freq);

	p->clear();
	for (uint i = 0; i < N; ++i) {
		// number from uniform distribution from start to start+duration
		uint value = static_cast<uint>(round(gsl_ran_flat(rng, start, start + duration)));
		value /= cmddur;
		value *= cmddur;
		p->push_back(value);
	}
	sort(p->begin(), p->end()); // sort spike-times ascending

	double mean_dist = 0.0;
	uint event, prev_event, min_dist = numeric_limits<uint>::max(), max_dist = 0;
	for (uint i = 0; i < p->size(); ++i) {
		event = p->at(i);
		// std::cout << "2del: " << event << std::endl;
		if (i > 0) {
			mean_dist += event - prev_event;
			if (min_dist > event - prev_event)
				min_dist = event - prev_event;
			if (max_dist < event - prev_event)
				max_dist = event - prev_event;
		}
		prev_event = event;
		// dbg(::Logger::ERROR) << dec << fixed << p->at(i);
	}
	mean_dist /= p->size() - 1;
	// dbg(::Logger::ERROR) << "SC_SlowCtrl::poisson: mean distance of generated neuron resets is "
	//       << fixed << mean_dist << " in " << N
	//       << " resets (duration is " << duration << ").";
}
//----------------------------------------------------------------------


void SC_SlowCtrl::setupFastAdc(unsigned int sample_time_us, std::bitset<3> adc_input)
{
	// input patterns are:
	// 000 Normal operation - <D13:D0> = ADC output
	// 001 All zeros - <D13:D0> = 0x0000
	// 010 All ones - <D13:D0> = 0x3FFF
	// 011 Toggle pattern - <D13:D0> toggles between 0x2AAA and 0x1555
	// 100 Digital ramp - <D13:D0> increments from 0x0000 to 0x3FFF by one code every cycle
	// 101 Custom pattern - <D13:D0> = contents of CUSTOM PATTERN registers
	// 110 Unused
	// 111 Unused
	fadc->configure(adc_input); // configures ADC itself

	adc_num_samples = (size_t)(floor(float(sample_time_us) * 96.0 / 2.0 + 0.5) + 1); // ADC samples
	                                                                                 // with 96MHz
	LOG4CXX_DEBUG(logger, "fast ADC records " << sample_time_us
	                                          << " micro seconds (number of double words: "
	                                          << adc_num_samples << ")");

	unsigned int endaddr = adc_start_adr + adc_num_samples;

	// configures controller in FPGA
	fadc->setup_controller(adc_start_adr, endaddr);
}

void SC_SlowCtrl::triggerAdc()
{
	sp6data* buf = ocp->writeBlock(0, 4);
	buf[0] = 0x80003009;
	buf[1] = 0x1; // start
	buf[2] = 0x80003009;
	buf[3] = 0x0; // start off
	ocp->doWB();
}

SpikenetComm::Commstate SC_SlowCtrl::writeBuf(uint64_t data, uint addr)
{
	// handle sdrambuf
	if (!sdrambufvalid) {
		sdrambufbase = addr;
		sdrambuf.resize(10000);
		sdrambufvalid = true;
	}
	// (ag): cast to int to avoid negative resizing!
	if ((int)sdrambuf.size() <= (int)(addr - sdrambufbase))
		sdrambuf.resize((addr - sdrambufbase) * 2);
	sdrambuf[addr - sdrambufbase] = data;
	LOG4CXX_TRACE(logger, "SlowCtrl::writeBuf idx:" << hex << (addr - sdrambufbase) << " a:" << hex
	                                                << addr << " d:" << data
	                                                << " b:" << sdrambufbase);
	return ok;
}

float SC_SlowCtrl::getTemp()
{
	return gyro->read_temperature();
}

std::string SC_SlowCtrl::getWorkstationFromFile(std::string filenameWorkstation)
{
	ifstream workstationFile(filenameWorkstation.c_str());

	if (!workstationFile) {
		LOG4CXX_DEBUG(logger,
		              "Could not open file containing workstation: " << filenameWorkstation);
		return "";
	}

	std::string workstation = "";
	while (workstationFile >> workstation) {
		LOG4CXX_INFO(logger, "Loaded workstation " << workstation
		                                           << " from file: " << filenameWorkstation);
		return workstation;
	}

	std::string msg =
	    "File containing workstation exists (" + filenameWorkstation + "), but could not be loaded";
	LOG4CXX_ERROR(logger, msg);
	throw std::runtime_error(msg);
	return "";
}

std::string SC_SlowCtrl::getWorkstationFromUsbDevice(Vmoduleusb* device)
{
	serial = device->getSerial();
	char* spikeyhalpath = getenv("SPIKEYHALPATH");
	if (spikeyhalpath == NULL) {
		std::string msg = "Could not find env variable SPIKEYHALPATH";
		LOG4CXX_ERROR(logger, msg);
		throw std::runtime_error(msg);
	}
	boost::filesystem::path folder(spikeyhalpath);
	if (boost::filesystem::exists(folder)) {
		boost::filesystem::directory_iterator end_itr;
		for (boost::filesystem::directory_iterator itr(folder); itr != end_itr; itr++) {
			if (!boost::filesystem::is_regular_file(itr->status()))
				continue;
			if (itr->path().extension().string().compare(".cfg") != 0)
				continue;
			ifstream configFile(itr->path().string().c_str());
			assert(configFile);
			std::string serialTmp = "";
			std::getline(configFile, serialTmp);
			configFile.close();
			if (serialTmp == serial) {
				return itr->path().stem().string();
			}
		}
	} else {
		std::string msg = "Could not open folder specified in env variable SPIKEYHALPATH";
		LOG4CXX_ERROR(logger, msg);
		throw std::runtime_error(msg);
	}
	std::string msg = "Could not find config file for USB device with serial " + serial;
	LOG4CXX_ERROR(logger, msg);
	throw std::runtime_error(msg);
}

void SC_SlowCtrl::getConfigFromFile(string filenameConfig)
{
	char* folder = getenv("SPIKEYHALPATH");
	if (folder == NULL) {
		std::string msg = "Could not find env variable SPIKEYHALPATH";
		LOG4CXX_ERROR(logger, msg);
		throw std::runtime_error(msg);
	}
	std::string filenameConfigWithExt = string(folder) + "/" + filenameConfig + ".cfg";
	ifstream configFile(filenameConfigWithExt.c_str());

	if (!configFile) {
		LOG4CXX_WARN(logger, "Could not open config file: " << filenameConfigWithExt);
		return;
	}

	uint i = 0;
	std::string oneLine = "";
	std::string msg = "Config file " + filenameConfigWithExt + " is corrupt";
	while (std::getline(configFile, oneLine)) {
		switch (i) {
			case 0:
				serial = oneLine;
				break;
			case 1:
				boardVersion = atoi(oneLine.c_str());
				break;
			case 2:
				chipVersion = atoi(oneLine.c_str());
				break;
			case 3:
				delayFpgaIn = atoi(oneLine.c_str());
				break;
			default:
				LOG4CXX_ERROR(logger, msg);
				throw std::runtime_error(msg);
		}
		i++;
	}
	configFile.close();

	if (i != 4) {
		LOG4CXX_ERROR(logger, msg);
		throw std::runtime_error(msg);
	}
	LOG4CXX_INFO(logger, "Config successfully loaded from file " << filenameConfigWithExt);
	LOG4CXX_DEBUG(logger, "  serial: " << serial << "; boardVersion: " << boardVersion
	                                   << "; chipVersion: " << chipVersion
	                                   << "; delayFpgaIn: " << delayFpgaIn);
}
