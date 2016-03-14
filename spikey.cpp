#include "common.h"

// communication
#include "idata.h"
#include "sncomm.h"
#include "sc_sctrl.h"
#include "sc_pbmem.h"
// chip
#include "spikenet.h"
#include "ctrlif.h"

#include "synapse_control.h" //synapse control class
#include "pram_control.h"    //parameter ram etc
#include "spikeyconfig.h"
#include "spikey.h"

static log4cxx::LoggerPtr logger = log4cxx::Logger::getLogger("HAL.Spi");

using namespace spikey2;

//******** Spikey ***********

const float Spikey::voutslope = 1.54 / (1.958 * 150); // Volt per microamp*nanosecs, vout slope

uint64_t Spikey::statusregToUint64(void)
{
	uint64_t statusreg64 = 0;
	uint64_t b;
	for (uint i = 0; i < hw_const->cr_width(); i++) {
		b = (uint64_t)statusreg[i] << i;
		statusreg64 += b;
	}
	return statusreg64;
}

void Spikey::writeCC(uint del)
{
	getCC()->setCtrl(statusregToUint64(), del);
}

void Spikey::setCCBit(uint pos, bool value)
{
	const int controlreg_width = hw_const->cr_width();
	if (pos > (controlreg_width - 1)) {
		throw std::runtime_error("statusreg width is " + to_str<const int>(controlreg_width) +
		                         ", you tried " + to_str<uint>(pos));
	}
	statusreg[pos] = value;
}

Spikey::Spikey(boost::shared_ptr<Spikenet> spnet, float clk, uint chipid)
    : Spikenet(*spnet), // this seems to be using an implicit copy-constructor of Spikenet
      clockper(clk),
      voutperiod(19),
      lutboost(6),
      pram_settled(false)
{
	static_cast<void>(chipid);
	initialize();
}

Spikey::Spikey(boost::shared_ptr<SpikenetComm> comm, float clk, uint chipid)
    : Spikenet(comm, chipid), clockper(clk), voutperiod(19), lutboost(6), pram_settled(false)
{
	initialize();
}

void Spikey::initialize()
{
	// set statusreg size
	for (int i = 0; i < hw_const->cr_width(); i++)
		statusreg.push_back(0);
	// rconv100=(1039)*9.5 *100 * 1e-6;//corner min
	rconv100 = 10000 * 100 * 1e-6;

	// standard delays
	synramdel = 2;
	synfstdel = ControlInterface::synramdelay; // minimum delay for tsense==0

	/*	//scinit testmode is integrated here
	    IData d(false, false, false);
	    d.setPowerenable(true);
	    this->Send(SpikenetComm::ctrl,d);*/

	// initialize event output FIFO depth for Spikey4
	if (hw_const->revision() >= 4)
		for (uint i = 0; i < hw_const->cr_eout_depth_width(); i++)
			statusreg[i + hw_const->cr_eout_depth()] =
			    (bool)(((uint)hw_const->cr_eout_depth_resval() >> i) & 0x1);

	setCCBit(hw_const->cr_anaclken(), true);
	setCCBit(hw_const->cr_anaclkhien(), true);

	/* reset all event in & out buffers */
	for (uint i = 0; i < hw_const->event_outs(); i++)
		setCCBit(hw_const->cr_eout_rst() + i, true);
	for (uint i = 0; i < hw_const->event_ins(); i++)
		setCCBit(hw_const->cr_einb_rst() + i, true);
	for (uint i = 0; i < hw_const->event_ins(); i++)
		setCCBit(hw_const->cr_ein_rst() + i, true);
	writeCC();

	for (uint i = 0; i < hw_const->event_outs(); i++)
		setCCBit(hw_const->cr_eout_rst() + i, false);
	for (uint i = 0; i < hw_const->event_ins(); i++)
		setCCBit(hw_const->cr_einb_rst() + i, false);
	for (uint i = 0; i < hw_const->event_ins(); i++)
		setCCBit(hw_const->cr_ein_rst() + i, false);
	writeCC();

	// initialization values for cispikenet control register
	dllreset = false;
	neuronreset = false;
	pccont = false;
	writeSCtl();

	// voltage LUTs
	getPC()->write_lut(0, 3, 0, 0, 0);
	getPC()->write_lut(1, 5, 0, 0, 0);
	getPC()->write_lut(2, 7, 0, 0, 0);
	getPC()->write_lut(3, 9, 0, 0, 0);
	getPC()->write_lut(4, 11, 0, 0, 0);
	getPC()->write_lut(5, 13, 0, 0, 0);
	getPC()->write_lut(6, 15, 0, 0, 0);
	// current LUTs
	getPC()->write_lut(7, 0, 3, 0, 0);
	getPC()->write_lut(8, 0, 5, 0, 0);
	getPC()->write_lut(9, 0, 7, 0, 0);
	getPC()->write_lut(10, 0, 9, 0, 0);
	getPC()->write_lut(11, 0, 11, 0, 0);
	getPC()->write_lut(12, 0, 13, 0, 0);
	getPC()->write_lut(13, 0, 15, 0, 0);
	// fill the remaining LUTs
	getPC()->write_lut(14, 1, 0, 0, 0);
	getPC()->write_lut(15, 1, 0, 0, 0);

	// because this does not work:
	// lut = getPC()->get_lut();
	// we'll do this:
	// save copies in lut vector
	lut.push_back(LutData(3, 0, 0, 0));
	lut.push_back(LutData(5, 0, 0, 0));
	lut.push_back(LutData(7, 0, 0, 0));
	lut.push_back(LutData(9, 0, 0, 0));
	lut.push_back(LutData(11, 0, 0, 0));
	lut.push_back(LutData(13, 0, 0, 0));
	lut.push_back(LutData(15, 0, 0, 0));
	lut.push_back(LutData(0, 3, 0, 0));
	lut.push_back(LutData(0, 5, 0, 0));
	lut.push_back(LutData(0, 7, 0, 0));
	lut.push_back(LutData(0, 9, 0, 0));
	lut.push_back(LutData(0, 11, 0, 0));
	lut.push_back(LutData(0, 13, 0, 0));
	lut.push_back(LutData(0, 15, 0, 0));
	lut.push_back(LutData(1, 0, 0, 0));
	lut.push_back(LutData(1, 0, 0, 0));

	// ask Spikey how old he is
	// unfortunately, the revision register cannot be read out, even with Spikey5... :-(
	uint64_t rdata = 0;
	uint64_t evout_depth_resval = 0;
	uint64_t ar_shift_value = 2ULL << hw_const->ar_chainnumberwidth();
	uint64_t chain_shift_result = 0;

	getAR()->read_cmd(ar_shift_value, 100); // shift through 1 for test
	getAR()->read_cmd(0ULL, 100);           // shift through all 0's for correct init

	getCC()->getCtrl();

	Flush();
	Run();
	waitPbFinished();

	getAR()->rcv_data(rdata); // dummy for chain readout in Spikey
	getAR()->rcv_data(rdata);
	// AG made the following calculation indepenent from currently loaded hw_const using UGLY
	// constant values:
	// 24 is the minimum chain length implemeneted in Spikey<3,4,5> and is therefore used for
	// masking
	chain_shift_result = rdata & mmw(hw_const->ar_chainnumberwidth() + 24);
	LOG4CXX_TRACE(logger, "analog readout chain readback value: 0x" << hex << chain_shift_result
	                                                                << ".");

	getCC()->rcv_data(rdata);
	// AG made the following calculation indepenent from currently loaded hw_const using UGLY
	// constant values:
	//   39 is the bit-width of the control register in Spikey4 and 5. Spikey3 has 32 Bit
	//    7 is the bit-width of the event out buffer depth reset value (only present in Spikey4 and
	//    5)
	// 0x7d is the reset value of the event out buffer depth (only present in Spikey4 and 5)
	evout_depth_resval = (rdata >> (hw_const->cr_pos() + (39 - 7))) & mmw(7);
	LOG4CXX_TRACE(logger, "event buffer out depth reset value: 0x" << hex << evout_depth_resval
	                                                               << ".");

	uint spikey_ver = 0;
	if (evout_depth_resval == 0x7d) {
		if (chain_shift_result == ar_shift_value)
			spikey_ver = 5;
		// the right shift is a result of ar_maxlen being too small by 1 in Spikey4
		else if (chain_shift_result == ar_shift_value >> 1)
			spikey_ver = 4;
		else
			throw std::runtime_error(
			    "Analog readout chain pattern doesn't match any known Spikey revision!");
	} else {
		spikey_ver = 3;
	}

	LOG4CXX_INFO(logger, "Found Spikey " << spikey_ver << ".");

	if (spikey_ver != hw_const->revision()) {
		throw std::runtime_error("Hardware revision deviates from revision in config file!");
	}
	// per default, the global trigger signal is NOT connected to the backplane's trigger line
	setGlobalTrigger(false);
}

void Spikey::setGlobalTrigger(bool value)
{
	boost::shared_ptr<SC_Mem> mem(boost::dynamic_pointer_cast<SC_Mem>(bus));
	if (mem != NULL) { // check if correct subclass used as bus
		mem->getSCTRL()->setGlobalTrigOut(value);
	} else
		LOG4CXX_WARN(logger, "Spikey::setGlobalTrigger couldn't get access to slow control");
}


// config spikey depending on valid flags in spikeyconfig
// order is:
// dac -> param -> chip -> rowconfig -> colconfig -> synapses

MemObj Spikey::config(boost::shared_ptr<SpikeyConfig> cfg)
{

	// remember last config
	actcfg = cfg;

	if (cfg->valid & SpikeyConfig::ud_dac) {
		LOG4CXX_DEBUG(logger, "Updating DAC");
		setIrefdac(cfg->irefdac);
		// dacimax=2.5; //microamps, adjust dacimax for param setup
		dacimax = cfg->irefdac / 10.0 * (1023.0 / 1024.0); // microamps, adjust dacimax for param
		                                                   // setup
		// irefdac has to be set initially. Therefore, dacimax
		// wil be valid for the lifetime of this SpikeyConfig object
		// and the Spikey class.

		setVcasdac(cfg->vcasdac);
		setVm(cfg->vm);
		setVstart(cfg->vstart);
		setVrest(cfg->vrest);
	}

	if (cfg->valid & SpikeyConfig::ud_param) {
		LOG4CXX_DEBUG(logger, "Updating parameter RAM");
		vector<PramData> pd;
		loadParam(cfg, pd);

		// optimize order of pram entires
		getPC()->sort_pd_triangle(pd);
		getPC()->check_pd_timing(pd, lut);
		getPC()->write_pd(pd, 6, lut); // transfer to chip and config param update
		getPC()->write_period(voutperiod - 1);
		getPC()->write_parnum(pd.size());

		setCCBit(hw_const->cr_pram_en(), true); // write statusreg to chip and wait...
		if (pram_settled) {
			writeCC(getPC()->get_pram_upd_cycles()); // wait one refresh cycle (specified in clock
			                                         // cycles)
		} else {
			LOG4CXX_DEBUG(logger, "waiting for " << (10 * getPC()->get_pram_upd_cycles())
			                                     << " cycles to settle analog parameters...");
			writeCC(10 * getPC()->get_pram_upd_cycles()); // wait 10 more refresh cycles until
			                                              // params (incl Vdllreset) are safely
			                                              // settled..
			pram_settled = true;

			// reset DLL (this is required at init of chip only)
			dllreset = true;
			writeSCtl(); // reset may be short
			dllreset = false;
			neuronreset = false;
			writeSCtl();
		}

		// calibParam();
	}
	if (cfg->valid & SpikeyConfig::ud_chip) {
		LOG4CXX_DEBUG(logger, "Updating chip configuration");
		getSC()->write_time((uint)round(cfg->tsense / clockper),
		                    (uint)round(cfg->tpcsec / clockper),
		                    (uint)round(cfg->tpcorperiod / clockper)); // time register
		synfstdel = ControlInterface::synramdelay + (uint)round(cfg->tsense / clockper);
	}


	// mapping d15-8: b1, d7-0, b0
	if (cfg->valid & SpikeyConfig::ud_rowconfig) {
		LOG4CXX_DEBUG(logger, "Updating row config");
		for (uint c = 0; c < SpikeyConfig::num_presyns; ++c) {
			uint data = ((cfg->synapse[c + SpikeyConfig::num_presyns].config.to_ulong())
			             << hw_const->sc_blockshift()) |
			            (cfg->synapse[c].config.to_ulong());
			getSC()->write_sram(c, 1 << hw_const->sc_rowconfigbit(), data,
			                    synfstdel); // since rowconfig is only a single write per row, delay
			                                // must always set to synfstdel
			//			if(data)dbg(Logger::DEBUG0)<<hex<<" d:"<<data;
			getSC()->close();
		}
	}
	// mapping d23-20: b0,n2; d19-16: b0,n1, d15-d12: b0, n0
	//				d11-08: b1,n2; d07-04: b1,n1, d03-d00: b1, n0
	if (cfg->valid & SpikeyConfig::ud_colconfig) {
		LOG4CXX_DEBUG(logger, "Updating column config");
		for (uint c = 0; c < SpikeyConfig::num_perprienc; ++c) {
			uint d = 0;
			for (uint b = 0; b < SpikeyConfig::num_blocks; ++b)
				for (uint n = 0; n < SpikeyConfig::num_prienc; ++n) {
					uint nidx = b * SpikeyConfig::num_neurons + c + n * SpikeyConfig::num_perprienc;
					d |= shiftRamData(nidx, cfg->neuron[nidx].config.to_ulong());
				}
			getSC()->write_sram(0, (1 << hw_const->sc_neuronconfigbit()) | c, d,
			                    c == 0 ? synfstdel : synramdel);
		}
		getSC()->close();
	}

	if (cfg->valid & SpikeyConfig::ud_weight) {
		LOG4CXX_DEBUG(logger, "Updating weights");
		for (uint r = 0; r < SpikeyConfig::num_presyns; ++r) { // over all synapse drivers
			valarray<ubyte> r0 = cfg->row(0, r), r1 = cfg->row(1, r);
			for (uint c = 0; c < SpikeyConfig::num_perprienc;
			     ++c) { // over number of priority encoder
				uint d = 0;
				for (uint n = 0; n < SpikeyConfig::num_prienc;
				     ++n) { // over neurons per priority encoder //block 0
					uint nidx = c + n * SpikeyConfig::num_perprienc;
					d |= shiftRamData(nidx, r0[nidx]);
					d |= shiftRamData(nidx + SpikeyConfig::num_neurons, r1[nidx]);
				}
				getSC()->write_sram(r, c, d, c == 0 ? synfstdel : synramdel);
			}
			getSC()->close();
		}
	}

	Flush();
	Run();
	waitPbFinished();

	/* optional debug functionality:
	// read back voltage parameters:
	if(cfg->valid & SpikeyConfig::ud_param){
	    getAR()->clear(0);
	    getAR()->clear(1);

	    for(uint chain=0;chain<2;chain++){
	        if(chain==1) getAR()->clear(0);
	        for(uint addr=0;addr<21;addr++) {

	            getAR()->set((chain==0?getAR()->voutl:getAR()->voutr),addr,true,150);//set with
	verify

	            Flush();
	            Run();

	            if (getAR()->check_last(addr,false) == 1)
	                ;//cout<<"Areadout check: positive"<<endl;
	            else
	                cout<<"Areadout check: negative"<<endl;

	            vector<float> v;
	            v.clear();
	            for(int i=0;i<2;++i)v.push_back(readAdc(SBData::ibtest));

	            double sum = std::accumulate(v.begin(), v.end(), 0.0);
	            double mean = sum / v.size();

	            double sq_sum = std::inner_product(v.begin(), v.end(), v.begin(), 0.0);
	            double stdev = std::sqrt(sq_sum / v.size() - mean * mean);

	            cout<<"chain " << chain << ", addr " << addr << ": ADC read: "<<mean<< ", stddev: "
	<< stdev << endl;
	        }
	    }

	    getAR()->clear(0);
	    getAR()->clear(1);
	    Flush();
	    Run();
	    waitPbFinished();
	}*/

	return MemObj(MemObj::ok);
}

// tries to calibrate vout by reading back membrane voltage and
// rewriting parametrer to set voltage to mean of error distri
// measured distibution has three (ore four) distinct maxima
// therefore simple calculation of mean is not sufficient
// now calibParam uses a histogram approach
//

vector<float> Spikey::calibParam(float prec, uint nadc, uint nmes, uint ndist, uint maxnmes)
{
	boost::shared_ptr<SpikeyConfig> c(new SpikeyConfig(hw_const, SpikeyConfig::ud_colconfig));
	boost::shared_ptr<SpikeyConfig> oldcfg;
	if (actcfg)
		oldcfg = actcfg;
	else
		oldcfg.reset(); // this will be valid throughout calib
	SpikeyConfig::NeuronConf nc;
	nc.config = hw_const->sc_ncd_outamp();
	c->neuron = nc;
	bool oldnr = neuronreset;
	neuronreset = true;
	writeSCtl(); // set neuronreset->no spikes, stable vout

	config(c); // recursive call ok if param not valid
	// uint nadc=5,nmes=200,ndist=100,maxnmes=2000; //number of measurements
	vector<float> m0, m1;

	// now start calibration loop:measure error distribution
	for (uint i = 0; i < nmes; i++) {
		// statusreg[cr_pram_en]=false;
		// writeCC();
		// statusreg[cr_pram_en]=true;
		// writeCC();
		for (uint d = 0; d < ndist; d++)
			getPC()->write_pram(3071, 0, 0, 0); // disturb param update
		Flush(); // necessary before sideband traffic
		vector<float> v0, v1;
		for (uint i = 0; i < nadc; ++i) {
			v0.push_back(readAdc(SBData::vout0));
			v1.push_back(readAdc(SBData::vout4));
		}
		m0.push_back(mean(v0));
		m1.push_back(mean(v1));
	};

	// generate histogram
	uint vmax = 1300; // mV
	float binsize = 1; // mV
	uint sgm = 4; // sigma of filter in bins
	vector<float> histo0((uint)(vmax / binsize), 0), histo1((uint)(vmax / binsize), 0);
	for (uint i = 0; i < nmes; i++) {
		++histo0[(uint)round(m0[i] * 1000 / binsize)];
		++histo1[(uint)round(m1[i] * 1000 / binsize)];
	}
	// GAUSS filter histogram
	// float gauss[]={2,8,16,8,2};
	// vector<float> coeff(gauss,gauss+5);//initializes coeff with gauss (gauss is iterator.begin())
	uint numcoeff = 6 * sgm + 1; // width of 3 sigma in each direction
	vector<float> coeff(numcoeff);
	for (uint i = 0; i <= numcoeff / 2; i++) {
		float cc = exp(-((float)i * (float)i) / (2 * (float)sgm * (float)sgm));
		coeff[numcoeff / 2 + i] = cc;
		coeff[numcoeff / 2 - i] = cc;
	}
	vector<float> f, res0(histo0.size(), 0), res1(histo1.size(), 0);
	fir(res0, histo0, coeff);
	fir(res1, histo1, coeff);
	f.push_back(((max_element(res0.begin(), res0.end()) - res0.begin())) * 0.001 * binsize);
	f.push_back(((max_element(res1.begin(), res1.end()) - res1.begin())) * 0.001 * binsize);
	LOG4CXX_DEBUG(logger, "Histo max after " << nmes << " (0,1):" << f[0] << " " << f[1]);

	// now set to histo max
	uint i;
	float res = 0;
	for (i = 0; i < maxnmes; i++) {
		for (uint d = 0; d < ndist; d++)
			getPC()->write_pram(3071, 0, 0, 0); // disturb param update
		Flush();
		vector<float> v0, v1;
		for (uint i = 0; i < nadc; ++i) {
			v0.push_back(readAdc(SBData::vout0));
			v1.push_back(readAdc(SBData::vout4));
		}
		// see if inside precision
		bool inprec = abs(mean(v0) - (f[0])) < prec && abs(mean(v1) - (f[1])) < prec;
		if (inprec) {
			uint t;
			for (t = 0; t < 5; t++) { // check if it stays 5 times inside precision
				vector<float> t0, t1;
				//				usleep(1000);
				for (uint i = 0; i < nadc; ++i) {
					t0.push_back(readAdc(SBData::vout0));
					t1.push_back(readAdc(SBData::vout4));
				}
				inprec = abs(mean(t0) - (f[0])) < prec && abs(mean(t1) - (f[1])) < prec;
				if (!inprec)
					break;
			}
			if (t < 5)
				continue;
			else {
				res = sqrt((pow(mean(v0) - f[0], 2) + pow(mean(v1) - f[1], 2)) * .5);
				break;
			}
		}
	}
	LOG4CXX_INFO(logger, "Result of calib: " << res);
	f.push_back(res);
	if (i == maxnmes)
		f.push_back(-1); // calibration failed
	else
		f.push_back(i);
	// restore chip
	if (oldcfg) {
		SpikeyConfig::SCupdate oldvalid = oldcfg->valid;
		oldcfg->valid = SpikeyConfig::ud_colconfig; // restore colconfig
		config(oldcfg); // restores old config in actcfg pointer
		oldcfg->valid = oldvalid;
	}

	neuronreset = oldnr;
	writeSCtl();
	Flush();
	return f;
}

// transmit spiketrain 'st' and allocate nec. memory
MemObj Spikey::sendSpikeTrain(const SpikeTrain& st, SpikeTrain* et, bool dropmod)
{
	static_cast<void>(dropmod);

	boost::shared_ptr<SC_Mem> mem(boost::dynamic_pointer_cast<SC_Mem>(bus));
	// check temperature
	assert(mem != NULL);
	float temperature = mem->getSCTRL()->getTemp();
	if (temperature > tempMax) {
		LOG4CXX_WARN(logger,
		             "System temperature too high, communication links may become unstable.")
	}

	//***** sync
	// reset fifos and priority encoder
	neuronreset = true;
	writeSCtl(); // write control registers

	// reset event out buffers
	for (uint i = 0; i < hw_const->event_outs(); i++)
		setCCBit(hw_const->cr_eout_rst() + i, true);
	writeCC(hw_const->fifo_reset_delay());

	//	//enable internal event loopback
	//	getCC()->setEvLb((1<<hw_const->el_enable_pos()), hw_const->fifo_reset_delay()); //turn on
	//event loopback TODO: AG said that it should be zero? does result in buffer full conditions!
	//are fifos reset here?

	for (uint i = 0; i < hw_const->event_outs(); i++)
		setCCBit(hw_const->cr_eout_rst() + i, false);
	writeCC(hw_const->fifo_reset_delay());

	// adjust system time to spike train time
	bus->setStime(0);

	Send(SpikenetComm::sync);

	neuronreset = false;
	writeSCtl();

	//***** events
	for (uint i = 0; i < st.d.size(); ++i)
		Send(SpikenetComm::write, st.d[i]);

	/* mute all outputs (after dummy spike) => fixes status data reads/spike race condition */
	for (uint i = 0; i < hw_const->event_outs(); i++)
		setCCBit(hw_const->cr_eout_rst() + i, true);
	writeCC(100); // TP: TODO: should be mem->getMaxChainDelay()

	// get buffer stats directly after execution.
	// -> during readback, execute recspiketrain in the same order as sendSpikeTrain!
	getCC()->read_clkstat(3); // TP: TODO: use variable for delays of this kind
	getCC()->read_clkbstat(3);
	getCC()->read_clkhistat(3);

	// get dropped/modified events:
	if (mem != NULL) { // check if correct subclass used as bus
		if (et != NULL) {
			et->d = *(mem->eev(chip())); // copy error events
			mem->eev(chip())->clear();   // clear error events
		}
	}

	LOG4CXX_DEBUG(logger, "spike train sent"); // read remaining received events into buffers
	return MemObj(MemObj::ok);
}

// hack!
void Spikey::replayPB()
{
	// get playback memory access pointer
	boost::shared_ptr<SC_Mem> mem(boost::dynamic_pointer_cast<SC_Mem>(bus));
	if (mem != NULL)
		mem->reFlush();
}
//---------------------------------------------------------------------------
void Spikey::waitPbFinished()
{
	LOG4CXX_DEBUG(logger, "Spikey::waitPbFinished: wait for hardware to become idle");

	boost::shared_ptr<SC_Mem> mem(boost::dynamic_pointer_cast<SC_Mem>(bus));
	if (mem) {
		mem->waitIdle();
	} else
		assert(false);
}
//---------------------------------------------------------------------------


// the last transmitted spiketrain (st.state==invalid) or (st.adr) is sent and the received data
// collected in 'st'
MemObj Spikey::recSpikeTrain(SpikeTrain& st, bool nonblocking)
{
	/* optional debug functionality:
	// read back number of executed sync commands and the sync_error flag:
	uint syncs(0);
	mem->getSCTRL()->getNumOfSyncs(syncs);
	cout << "*** Spikey::recSpikeTrain: Read number of syncs: " << dec << syncs << ", sync_error
	flag: " << mem->getSCTRL()->getSyncError() << endl; */


	/* this is not executed, since no read answers are expected during the spiketrain.
	   events are read back in the course of teh following rcv_data commands, automatically
	    IData d=IData::CI();

	    SpikenetComm::Mode flags;
	    if (nonblocking)
	        flags = (SpikenetComm::Mode) (SpikenetComm::read|SpikenetComm::nonblocking);
	    else
	        flags = (SpikenetComm::Mode) SpikenetComm::read;

	    //read remaining received events into buffers
	    uint dropCount = 0;
	    while(SpikenetComm::ok==Receive(flags,d))
	        ++dropCount;

	    dbg(Logger::DEBUG0)<<"recSpikeTrain dropped "<< dropCount << " cmd(s).";
	*/

	// perform read back operations only when using message queues (real hardware)
	// read back status registers and check for fifo problems on Spikey.
	uint64_t spstatus;

	// ****************************************************************************
	// to ensure the following works correctly, execute recSpikeTrain in the
	// same order as sendSpikeTrain!
	// ****************************************************************************

	LOG4CXX_DEBUG(logger, "Spikey::recSpikeTrain: receiving spikes");

	getCC()->rcv_data(spstatus);
	LOG4CXX_TRACE(logger, "recSpikeTrain checking event in buffers...");
	for (uint i = 0; i < hw_const->event_ins(); i++) {
		for (uint j = 0; j < 4; j++) {
			if ((spstatus >> (hw_const->cr_pos() + 4 * i + j)) & 0x1) {
				switch (j) {
					case 0:
						LOG4CXX_WARN(logger, "Spikey's event in buffer "
						                         << (hw_const->event_ins() - i - 1)
						                         << " had full condition!");
						break;
					case 1:
						LOG4CXX_WARN(logger, "Spikey's event in buffer "
						                         << (hw_const->event_ins() - i - 1)
						                         << " had almost full condition!");
						break;
					case 2:
						LOG4CXX_WARN(logger, "Spikey's event in buffer "
						                         << (hw_const->event_ins() - i - 1)
						                         << " had half full condition!");
						break;
					case 3:
						LOG4CXX_WARN(logger, "Spikey's event in buffer "
						                         << (hw_const->event_ins() - i - 1)
						                         << " had error condition!");
						break;
					default:
						break;
				}
			}
		}
	}

	getCC()->rcv_data(spstatus);
	LOG4CXX_TRACE(logger, "recSpikeTrain checking event inb buffers...");
	for (uint i = 0; i < hw_const->event_ins(); i++) {
		for (uint j = 0; j < 4; j++) {
			if ((spstatus >> (hw_const->cr_pos() + 4 * i + j)) & 0x1) {
				switch (j) {
					case 0:
						LOG4CXX_WARN(logger, "Spikey's event inb buffer "
						                         << (hw_const->event_ins() - i - 1)
						                         << " had full condition!");
						break;
					case 1:
						LOG4CXX_WARN(logger, "Spikey's event inb buffer "
						                         << (hw_const->event_ins() - i - 1)
						                         << " had almost full condition!");
						break;
					case 2:
						LOG4CXX_WARN(logger, "Spikey's event inb buffer "
						                         << (hw_const->event_ins() - i - 1)
						                         << " had half full condition!");
						break;
					case 3:
						LOG4CXX_WARN(logger, "Spikey's event inb buffer "
						                         << (hw_const->event_ins() - i - 1)
						                         << " had error condition!");
						break;
					default:
						break;
				}
			}
		}
	}

	getCC()->rcv_data(spstatus);
	LOG4CXX_TRACE(logger, "recSpikeTrain checking event out buffers...");
	for (uint i = 0; i < hw_const->event_outs(); i++) {
		for (uint j = 0; j < 4; j++) {
			if ((spstatus >> (hw_const->cr_pos() + 4 * i + j)) & 0x1) {
				switch (j) {
					case 0:
						LOG4CXX_WARN(logger, "Spikey's event out buffer "
						                         << (hw_const->event_outs() - i - 1)
						                         << " had full condition!");
						break;
					case 1:
						LOG4CXX_WARN(logger, "Spikey's event out buffer "
						                         << (hw_const->event_outs() - i - 1)
						                         << " had almost full condition!");
						break;
					case 2:
						LOG4CXX_WARN(logger, "Spikey's event out buffer "
						                         << (hw_const->event_outs() - i - 1)
						                         << " had half full condition!");
						break;
					case 3:
						LOG4CXX_WARN(logger, "Spikey's event out buffer "
						                         << (hw_const->event_outs() - i - 1)
						                         << " had error condition!");
						break;
					default:
						break;
				}
			}
		}
	}

	boost::shared_ptr<SC_Mem> mem(boost::dynamic_pointer_cast<SC_Mem>(bus));
	if (mem != NULL) { // check if correct subclass used as bus
		st.d = *(mem->rcvd(chip())); // better use move from c++11, or swap
		mem->rcvd(chip())->clear();
	} else
		return MemObj(MemObj::invalid);
	return MemObj(MemObj::ok);
}

// convert parameters to spikey format
void Spikey::loadParam(boost::shared_ptr<SpikeyConfig> c, vector<PramData>& pd)
{
	LOG4CXX_WARN(logger, "using spikey without calibration");
	pd.clear();
	pd.reserve(3072);

	// for spikey4 chips, configure only right block synapses and neurons!
	// synapses
	uint bstart, jsynstart;
	if (hw_const->revision() == 4) {
		bstart = 1;
		jsynstart = hw_const->pr_adr_syn1start();
	} else {
		bstart = 0;
		jsynstart = hw_const->pr_adr_syn0start();
	}
	uint j = jsynstart;
	for (uint b = bstart; b < SpikeyConfig::num_blocks; ++b)
		for (uint n = 0; n < SpikeyConfig::num_presyns; ++n) {
			pd.push_back(PramData(
			    j++, convCurDac(c->synapse[b * SpikeyConfig::num_presyns + n].drviout), 9));
			pd.push_back(
			    PramData(j++, convCurDac(c->synapse[b * SpikeyConfig::num_presyns + n].adjdel), 9));
			pd.push_back(PramData(
			    j++, convCurDac(dacimax - c->synapse[b * SpikeyConfig::num_presyns + n].drvirise),
			    9));
			pd.push_back(PramData(
			    j++, convCurDac(c->synapse[b * SpikeyConfig::num_presyns + n].drvifall), 9));
		}
	// neurons
	if (hw_const->revision() == 4) {
		j = hw_const->pr_adr_neuron1start();
		for (uint n = 0; n < SpikeyConfig::num_neurons; ++n) {
			pd.push_back(PramData(
			    j++, convCurDac(dacimax - c->neuron[SpikeyConfig::num_neurons + n].ileak), 9));
			pd.push_back(
			    PramData(j++, convCurDac(c->neuron[SpikeyConfig::num_neurons + n].icb), 9));
			j += 2;
		}
	} else {
		j = hw_const->pr_adr_neuron0start();
		for (uint n = 0; n < SpikeyConfig::num_neurons; ++n) {
			pd.push_back(PramData(j++, convCurDac(dacimax - c->neuron[+n].ileak), 9));
			pd.push_back(PramData(j++, convCurDac(c->neuron[+n].icb), 9));
			j += 2;
		}
		j = hw_const->pr_adr_neuron1start();
		for (uint n = 0; n < SpikeyConfig::num_neurons; ++n) {
			pd.push_back(PramData(
			    j++, convCurDac(dacimax - c->neuron[SpikeyConfig::num_neurons + n].ileak), 9));
			pd.push_back(
			    PramData(j++, convCurDac(c->neuron[SpikeyConfig::num_neurons + n].icb), 9));
			j += 2;
		}
	}
	// bias block
	j = hw_const->pr_adr_biasstart();
	for (uint n = 0; n < PramControl::num_biasb; ++n)
		pd.push_back(PramData(j++, convCurDac(dacimax - c->biasb[n]), 9));
	j = hw_const->pr_adr_outampstart();
	for (uint n = 0; n < PramControl::num_outamp; ++n)
		pd.push_back(PramData(j++, convCurDac(c->outamp[n]), 9));

	// re-swapped writing order for blocks because changed for spikey2 -- DB
	j = hw_const->pr_adr_voutrstart();
	for (uint n = 0; n < hw_const->ar_numvouts(); ++n) {
		pd.push_back(PramData(j++, convVoltDac(c->vout[hw_const->ar_numvouts() + n]), 2));
		pd.push_back(PramData(j++, convCurDac(c->voutbias[hw_const->ar_numvouts() + n]), 9));
	}
	j = hw_const->pr_adr_voutlstart();
	for (uint n = 0; n < hw_const->ar_numvouts(); ++n) {
		pd.push_back(PramData(j++, convVoltDac(c->vout[n]), 2));
		pd.push_back(PramData(j++, convCurDac(c->voutbias[n]), 9));
	}
}

void Spikey::setFifoDepth(int depth, int delay)
{
	for (uint i = 0; i < hw_const->cr_eout_depth_width(); i++) {
		bool value = (bool)((depth >> i) & 0x1);
		int pos = i + hw_const->cr_eout_depth();
		setCCBit(pos, value);
	}
	writeCC(delay);
}

void Spikey::setEvOut(bool value, int delay)
{
	for (uint i = 0; i < hw_const->event_outs(); i++) {
		setCCBit((hw_const->cr_eout_rst() + i), value);
	}
	writeCC(delay);
}

void Spikey::setEvIn(bool value, int delay)
{
	for (uint i = 0; i < hw_const->event_ins(); i++) {
		setCCBit((hw_const->cr_ein_rst() + i), value);
	}
	writeCC(delay);
}

void Spikey::setEvInb(bool value, int delay)
{
	for (uint i = 0; i < hw_const->event_ins(); i++) {
		setCCBit((hw_const->cr_einb_rst() + i), value);
	}
	writeCC(delay);
}

uint Spikey::convVoltDac(double voltage)
{
	double microamps = voltage / rconv100;
	uint dacval = convCurDac(microamps);
	LOG4CXX_TRACE(logger, "Vout Voltage [V]: " << voltage << " Current [uA]: " << microamps
	                                           << " DAC value:" << dacval);
	return dacval;
}

uint Spikey::convCurDac(double current)
{
	current = (current < 0.0) ? 0.0 : current; // catch negative values
	uint out = static_cast<unsigned int>((current / dacimax) * 1023.0);
	LOG4CXX_TRACE(logger, "Current [uA]: " << current << " DAC value: " << out);
	return (out > 1023) ? 1023 : out; // catch too big values
}
