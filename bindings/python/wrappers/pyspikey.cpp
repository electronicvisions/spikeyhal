#define PY_ARRAY_UNIQUE_SYMBOL hal
#define NO_IMPORT_ARRAY

#include "spikey2_lowlevel_includes.h"

#include "pysc_mem.h"
#include "pyspiketrain.h"
#include "pyspikeyconfig.h"
#include "pyspikey.h"
#include <cmath>

#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include <numpy/arrayobject.h>

static log4cxx::LoggerPtr logger = log4cxx::Logger::getLogger("HAL.PyS");

using namespace spikey2;
using namespace std;


PySpikey::PySpikey(boost::shared_ptr<PySC_Mem> comm, float clockper, uint chipid, uint spikeyNr,
                   string calibfile)
    : SpikeyCalibratable(comm, clockper, chipid, spikeyNr, calibfile), _lastRunWasSTDP(false)
{
	// this weight array contains just zeros and is used to temporarily set all synaptic weights to
	// zero
	zero_weight_array.resize(
	    SpikeyConfig::num_blocks * SpikeyConfig::num_neurons * SpikeyConfig::num_presyns, 0);

	_correlationInformation.resize(256);
	_synapseWeights.resize(256);
	_synapseWeightsOld.resize(256);
	_synapseWeightsPreLUT.resize(256);

	for (uint i = 0; i < 256; ++i) {
		_correlationInformation[i].resize(384);
		_synapseWeights[i].resize(384);
		_synapseWeightsOld[i].resize(384);
		_synapseWeightsPreLUT[i].resize(384);

		for (uint j = 0; j < 384; ++j) {
			_correlationInformation[i][j] = 0;
			_synapseWeights[i][j] = 0;
			_synapseWeightsOld[i][j] = 0;
			_synapseWeightsPreLUT[i][j] = 0;
		}
	}

	// get the synapse control object
	synapse_control = getSC();

	// for nicer stdout messages
	_printIndentation = "      ";
}


// clear
void PySpikey::clearPlaybackMem()
{
	LOG4CXX_DEBUG(logger, "Clearing playback memory.");
	Spikenet::Clear();
}

// transmit spiketrain 'st' and allocate nec. memory
void PySpikey::sendSpikeTrain(const PySpikeTrain& st, PySpikeTrain* et)
{
	if (et != NULL)
		SpikeyCalibratable::sendSpikeTrain(st, et);
	else
		SpikeyCalibratable::sendSpikeTrain(st);
}

// replay spiketrain
void PySpikey::resendSpikeTrain()
{
	SpikeyCalibratable::replayPB();
	// cout << "re-sending..." << endl;
}

// transmit spiketrain 'st' and allocate nec. memory
void PySpikey::recSpikeTrain(PySpikeTrain& st)
{
	if (_lastRunWasSTDP) {
		SpikeyCalibratable::recSpikeTrain(st, false); // blocking access
		_lastRunWasSTDP = false;
	} else
		SpikeyCalibratable::recSpikeTrain(st, false);
}

// config spikey depending on valid flags in spikeyconfig
// order is:
// dac -> param -> chip -> rowconfig -> colconfig -> synapses
void PySpikey::config(boost::shared_ptr<PySpikeyConfig> cfg, bool updateChip, bool updateDAC,
                      bool updateParam, bool updateRowConf, bool updateColConf, bool updateWeight)
{
	// cout << _printIndentation << "in pyspikey::config" << endl;

	// According to A.Gruebl, one vout calibration at the instantiation of a Spikey object is
	// sufficient
	// Thus, the optional vout calibration at every config() call was commented out (DB)
	/*
	if(doVoutCalib)
	{
	    string calibfile = string(getenv("SPIKEYHALPATH")) + "/spikeycalib.xml";
	    cout << _printIndentation << "Trying to open " << calibfile << " for Vout calibration..." <<
	endl;
	    if(SpikeyCalibratable::clb->readCalib(calibfile)) cout << _printIndentation << "Vout
	calibration successfully loaded and applied!" << endl;
	}*/

	if (updateChip)
		cfg->valid = (SpikeyConfig::SCupdate)(cfg->valid | SpikeyConfig::ud_chip);
	else
		cfg->valid = (SpikeyConfig::SCupdate)(cfg->valid & ~SpikeyConfig::ud_chip);
	if (updateDAC)
		cfg->valid = (SpikeyConfig::SCupdate)(cfg->valid | SpikeyConfig::ud_dac);
	else
		cfg->valid = (SpikeyConfig::SCupdate)(cfg->valid & ~SpikeyConfig::ud_dac);
	if (updateParam)
		cfg->valid = (SpikeyConfig::SCupdate)(cfg->valid | SpikeyConfig::ud_param);
	else
		cfg->valid = (SpikeyConfig::SCupdate)(cfg->valid & ~SpikeyConfig::ud_param);
	if (updateRowConf)
		cfg->valid = (SpikeyConfig::SCupdate)(cfg->valid | SpikeyConfig::ud_rowconfig);
	else
		cfg->valid = (SpikeyConfig::SCupdate)(cfg->valid & ~SpikeyConfig::ud_rowconfig);
	if (updateColConf)
		cfg->valid = (SpikeyConfig::SCupdate)(cfg->valid | SpikeyConfig::ud_colconfig);
	else
		cfg->valid = (SpikeyConfig::SCupdate)(cfg->valid & ~SpikeyConfig::ud_colconfig);
	if (updateWeight)
		cfg->valid = (SpikeyConfig::SCupdate)(cfg->valid | SpikeyConfig::ud_weight);
	else
		cfg->valid = (SpikeyConfig::SCupdate)(cfg->valid & ~SpikeyConfig::ud_weight);
	// cout << "cfg->valid = " << cfg->valid << endl;

	SpikeyCalibratable::config(cfg);
	if (updateChip || updateDAC || updateParam || updateRowConf || updateColConf || updateWeight)
		LOG4CXX_INFO(logger, "Hardware config written (enable loglevel 3 to list parameter names)");
	if (updateChip)
		LOG4CXX_DEBUG(logger, "  tsense; tpcsec; tpcorperiod; ");
	if (updateDAC)
		LOG4CXX_DEBUG(logger, "  irefdac; vcasdac; vm; vstart; vrest; ");
	if (updateParam) {
		LOG4CXX_DEBUG(logger, "  synapses: drviout, adjdel, drvifall, drvirise; neuron: ileak, "
		                      "icb; biasb; outamp; vout; voutbias; probepad; probebias; ");
		// wait for 50 ms in order to let voltages settle to their target values
		usleep(50000); // TP: TODO: this has to be tuned!!!
	}
	if (updateRowConf)
		LOG4CXX_DEBUG(logger, "  synapses: config; ");
	if (updateColConf)
		LOG4CXX_DEBUG(logger, "  neurons: config; ");
	if (updateWeight)
		LOG4CXX_DEBUG(logger, "  w; ");
}

void PySpikey::interruptActivity(boost::shared_ptr<PySpikeyConfig> original_cfg)
{
	// cout << "Interrupting all possibly remaining spiking activity from previous experiments!" <<
	// endl;
	// buffer update flags
	SpikeyConfig::SCupdate validBuffer = (SpikeyConfig::SCupdate)original_cfg->valid;
	// buffer synaptic weights
	valarray<ubyte> weightBuffer = original_cfg->weight;

	// temporarily manipulate update flags
	original_cfg->valid = (SpikeyConfig::SCupdate)(0 | SpikeyConfig::ud_weight);
	// temporarily set all weights to zero
	original_cfg->weight = zero_weight_array;

	// transfer zero-weight array
	SpikeyCalibratable::config(original_cfg);
	// wait for 100 us, which - assuming a speedup factor of 10.000 - corresponds to 1 s in
	// biological time
	// this is more than any reasonable neural or synaptic time constant, i.e. every activity should
	// die out
	usleep(100);

	// re-establish original configuration
	original_cfg->weight = weightBuffer;
	SpikeyCalibratable::config(original_cfg);
	original_cfg->valid = validBuffer;
}

/**
 * Assign a membrane pin (0-7) to test pin
 *
 * \param membranePin identifies membrane pin (0-7 is valid)
 */
void PySpikey::assign2TestPin(int membranePin, int signal, int block)
{
	boost::shared_ptr<SC_Mem> membus = boost::dynamic_pointer_cast<SC_Mem>(bus);
	boost::shared_ptr<SC_SlowCtrl> sc = membus->getSCTRL();
	assert(sc); // Don't have a slow control object!

	// assign membrane pin to test pin
	if (membranePin != -1) {
		assert(membranePin < 8);
		sc->setAnaMux(membranePin);
	}

	// assign voltage to test pin
	else {
		assert(block < 2);                      // left and right block
		assert(signal < hw_const->ar_maxlen()); // longest chain (24)
		sc->setAnaMux(8);                       // disable MUX
		boost::shared_ptr<AnalogReadout> analog_readout = getAR();
		if (block == 0)
			analog_readout->set(analog_readout->voutl, signal, true);
		else
			analog_readout->set(analog_readout->voutr, signal, true);
	}
	Flush();
}

void PySpikey::assignMembranePin2TestPin(uint membranePin)
{
	assign2TestPin(membranePin, -1, -1);
}

void PySpikey::assignVoltage2TestPin(uint signal, uint block)
{
	assign2TestPin(-1, signal, block);
}


// just a temporary tool
// void printBitfield(uint64_t v) {
//    for (int i = sizeof(v)*8 - 1 ; i >= 0; i--) std::cout << ((1ULL << i) & v  ? "1" : "0");
//    std::cout << std::endl;
//}


void PySpikey::assignMultipleVoltages2IBTest(std::vector<int> vouts, int leftBlock, int rightBlock)
{
	// build basic bitchain
	uint64_t bitvec = 0ULL;
	std::vector<int>::iterator Iter;

	for (Iter = vouts.begin(); Iter != vouts.end(); Iter++) {
		// std::cout << *Iter << " ";
		bitvec = bitvec |
		         (1ULL << ((hw_const->ar_maxlen() - 1 - *Iter) + hw_const->ar_chainnumberwidth()));
	}

	std::cout << "bitvec:         ";
	for (int i = hw_const->ar_maxlen() - 1 + hw_const->ar_chainnumberwidth(); i >= 0; i--)
		std::cout << ((1ULL << i) & bitvec ? "1" : "0");
	std::cout << std::endl;


	boost::shared_ptr<AnalogReadout> analog_readout = getAR();
	uint64_t bitvecBlock;
	if (leftBlock == true) {
		bitvecBlock = bitvec;
		std::cout << "set leftblock:  ";
		analog_readout->setMultiple(analog_readout->voutl, bitvecBlock, false);
	}

	if (rightBlock == true) {
		bitvecBlock = bitvec;
		std::cout << "set rightblock: ";
		analog_readout->setMultiple(analog_readout->voutr, bitvecBlock, false);
	}

	Flush();
}

float PySpikey::getTemp()
{
	boost::shared_ptr<SC_Mem> membus = boost::dynamic_pointer_cast<SC_Mem>(bus);
	boost::shared_ptr<SC_SlowCtrl> sc = membus->getSCTRL();
	assert(sc); // Don't have a slow control object!

	return sc->getTemp();
}

void PySpikey::autocalib(boost::shared_ptr<PySpikeyConfig> cfg, string filenamePlot)
{
	string calibfile = string(getenv("SPIKEYHALPATH")) + "/spikeycalib.xml";
	cout << _printIndentation << "Trying to open " << calibfile
	     << " for auto calibration... (write access!)" << endl;
	boost::shared_ptr<SC_Mem> membus = boost::dynamic_pointer_cast<SC_Mem>(bus);
	boost::shared_ptr<SC_SlowCtrl> sc = membus->getSCTRL();
	if (!sc)
		cout << _printIndentation << "pyspikey: Don't have a slow control object!" << endl;
	else {
		sc->setAnaMux(8);
		usleep(300000);
		if (cfg != NULL)
			clb->autoCalibRobust(calibfile, filenamePlot, cfg);
		else
			clb->autoCalibRobust(calibfile, filenamePlot);
	}
}


void PySpikey::calibParam(float f)
{
	SpikeyCalibratable::calibParam(f);
}


void PySpikey::checkSynRam(uint maxrows, uint maxcols, uint loops)
{
	uint errors = 0;
	uint64_t d, dd;

	// timing parameters
	uint clockper = 10; // nanosecs
	uint tsense = 150 / clockper;
	uint tpcsec = 30 / clockper;
	uint tdel = 6 + tsense + tpcsec;

	// get the synapse control object
	boost::shared_ptr<SynapseControl> synapse_control = getSC();

	for (uint l = 0; l < loops; ++l) {
		bus->Clear();
		srand(l);
		for (uint r = 0; r < maxrows; ++r) {
			for (uint c = 0; c < maxcols; ++c) {
				d = 0;
				for (uint i = 0; i < 6; i++)
					d |= (rand() & 15) << (i * 4);

				synapse_control->write_sram(r, c, d, tdel);
			}
			synapse_control->close();
		}

		for (uint r = 0; r < maxrows; ++r) {
			for (uint c = 0; c < maxcols; ++c)
				synapse_control->read_sram(r, c, tdel);

			synapse_control->close();
		}
		srand(l);

		for (uint r = 0; r < maxrows; ++r) {
			for (uint c = 0; c < maxcols; ++c) {
				d = 0;
				for (uint i = 0; i < 6; i++)
					d |= (rand() & 15) << (i * 4);

				synapse_control->rcv_data(dd);
				dd = (dd >> (hw_const->sc_aw() + hw_const->sc_commandwidth())) &
				     mmw(hw_const->sc_dw());
				if (dd != d) {
					errors += 1;
					// cout<<_printIndentation << "row,col: "<<r<<","<<c<<" pattern:
					// ";binout(cout,d,24)<<" read:    ";binout(cout,dd,24)<<endl;
				}
			}
		}
		cout << _printIndentation << "loop: " << l << " errors: " << errors << endl;
	}
}


void PySpikey::processCorrFlags(synapseRect rect)
{
	// automatic processing - timing checked (TP, 17.08.2015)
	synapse_control->proc_corr(rect.rowMin, rect.rowMax, true,
	                           (timings.tdel + 2 * (timings.tpcorperiod + 4 * 64)) *
	                               (rect.rowMax - rect.rowMin + 1));

	// TP: former method was to process each row independently => no automatic update possible
}


void PySpikey::readCorrFlags(synapseRect rect)
{
	for (int row = rect.rowMin; row <= rect.rowMax; row++) {
		for (int col = (rect.colMin % 64); col <= (rect.colMax % 64);
		     col += 4) { // all 6 of the 64 blocks and 4 cols each are read at once
			// ECM: 25+20 (+10 safe); was 100; tjms used: default == no timing; AG: less than
			// proc_corr, because no weight processing required
			synapse_control->read_corr(row, col,
			                           (timings.tdel + 2 * (timings.tpcorperiod + 4 * 64)) *
			                               (rect.rowMax - rect.rowMin + 1));
			synapse_control->close(10); // switch to a/causal => rowconfig changed; 16.05.2011: AG
			                            // said>=2 cycles, 10=save
			synapse_control->read_corr(row, col + (1 << hw_const->sc_rowconfigbit()),
			                           (timings.tdel + 2 * (timings.tpcorperiod + 4 * 64)) *
			                               (rect.rowMax - rect.rowMin + 1));
			synapse_control->close(10); // switch to a/causal => rowconfig changed
		}
	}
}


void PySpikey::receiveCorrFlags(synapseRect rect, std::vector<std::vector<int>>& matrix)
{
	for (int row = rect.rowMin; row <= rect.rowMax; row++) {
		for (int col = (rect.colMin % 64); col <= (rect.colMax % 64); col += 4) {
			uint64_t ra, rc;
			synapse_control->rcv_data(ra);
			synapse_control->rcv_data(rc);

			uint64_t rc_shifted = rc >> (hw_const->sc_aw() + hw_const->sc_commandwidth());
			uint64_t ra_shifted = ra >> (hw_const->sc_aw() + hw_const->sc_commandwidth());

			uint pack = 0; // filling 64-block ascending => mark 64-block
			uint blk = 3;  // shifting by blk * 4 bit :)
			for (uint i = 0; i < 6; ++i) {
				// 64-block-no: 2 1 0 5 4 3 (* 64 = col no, LSB right)
				// position:    5 4 3 2 1 0 (* 4 bit shift)
				switch (i) {
					case 0:
						blk = 3;
						break; // block (0)
					case 1:
						blk = 4;
						break; // block (1)
					case 2:
						blk = 5;
						break; // block (2)
					case 3:
						blk = 0;
						break; // block (3)
					case 4:
						blk = 1;
						break; // block (4)
					case 5:
						blk = 2;
						break; // block (5)
				}
				uint inside = 0;

				for (int f = 3; f >= 0;
				     --f) { // f marks fxlag within 4-bit (asc synapse corr flags)
					// cout << "reading cf @ " <<  pack*64+col+inside << ' ' << row << endl;
					assert(matrix[row][pack * 64 + col + inside] == -1);
					matrix[row][pack * 64 + col + inside] = 0; // just detecting writes

					if ((ra_shifted >> (blk * 4 + f)) & 1) // acausal correlation detected
						matrix[row][pack * 64 + col + inside] += 1;
					if ((rc_shifted >> (blk * 4 + f)) & 1) //  causal correlation detected
						matrix[row][pack * 64 + col + inside] += 2;
					inside++;
				}
				pack++;
			}
		}
	}
	for (int r = 0; r < 256; r++)
		for (int c = 0; c < 384; c++)
			if ((r > rect.rowMin) && (r < rect.rowMax))
				if ((c > rect.colMin) && (c < rect.colMax))
					assert(matrix[r][c] != -1);
	// else is not true... weired packet format!
}


void PySpikey::readWeights(synapseRect rect)
{
	for (int row = rect.rowMin; row <= rect.rowMax; row++) {
		for (int col = (rect.colMin % 64); col <= (rect.colMax % 64); col++) {
			synapse_control->read_sram(row, col, timings.tsense * 5 * 2);
			// ECM: this used to be tsense*5, could be minized to 23? (TODO)
			// tmjs used tdel, but this triggers errors!
		}
		synapse_control->close(); // close row
	}
}


void PySpikey::receiveWeights(synapseRect rect, std::vector<std::vector<int>>& matrix)
{
	for (int row = rect.rowMin; row <= rect.rowMax; row++) {
		for (int col = rect.colMin % 64; col <= (rect.colMax % 64); col++) {
			uint64_t wd;
			synapse_control->rcv_data(wd);
			uint64_t wd_shifted = wd >> (hw_const->sc_aw() + hw_const->sc_commandwidth());
			uint pack = 0; // filling 64-block ascending => mark 64-block
			uint blk = 3;  // shifting by blk * 4 bit :)
			for (uint i = 0; i < 6; ++i) {
				// 64-block-no: 2 1 0 5 4 3 (* 64 = col no, LSB right)
				// position:    5 4 3 2 1 0 (* 4 bit shift)
				switch (i) {
					case 0:
						blk = 3;
						break; // block (0)
					case 1:
						blk = 4;
						break; // block (1)
					case 2:
						blk = 5;
						break; // block (2)
					case 3:
						blk = 0;
						break; // block (3)
					case 4:
						blk = 1;
						break; // block (4)
					case 5:
						blk = 2;
						break; // block (5)
				}
				matrix[row][pack * 64 + col] = (wd_shifted >> (blk * 4)) & 0xf;
				pack++;
			}
		}
	}
}

// set LUT (acausal [0..15], causal[16..31])
void PySpikey::setLUT(std::vector<int> lut)
{
	boost::shared_ptr<SC_Mem> membus = boost::dynamic_pointer_cast<SC_Mem>(bus);
	boost::shared_ptr<SC_SlowCtrl> sc = membus->getSCTRL();
	assert(sc); // Don't have a slow control object!
	sc->set_LUT(lut);
}
std::vector<int> PySpikey::getLUT()
{
	boost::shared_ptr<SC_Mem> membus = boost::dynamic_pointer_cast<SC_Mem>(bus);
	boost::shared_ptr<SC_SlowCtrl> sc = membus->getSCTRL();
	assert(sc); // Don't have a slow control object!
	return sc->get_LUT();
}

// insert process correlations in playback memory? and distance between them
void PySpikey::setSTDPParamsCont(bool activate, uint first, uint distance, uint distance_pre,
                                 uint startrow, uint stoprow)
{
	boost::shared_ptr<SC_Mem> membus = boost::dynamic_pointer_cast<SC_Mem>(bus);
	boost::shared_ptr<SC_SlowCtrl> sc = membus->getSCTRL();
	assert(sc); // Don't have a slow control object!
	sc->setProcCorr(activate, first, distance, distance_pre, startrow, stoprow);
	LOG4CXX_DEBUG(logger,
	              "PySpikey::setSTDPParamsCont: Process correlation parameters changed: is active: "
	                  << activate << " first: " << first << " dist: " << distance << " dist_pre: "
	                  << distance_pre << " startrow: " << startrow << " stoprow: " << stoprow);
}

void PySpikey::sendSpikeTrainWithSTDP(const PySpikeTrain& st, PySpikeTrain& stout, PySpikeTrain* et,
                                      uint minRow, uint maxRow, uint minCol, uint maxCol,
                                      bool changeWeights, bool verbose)
{

	// get SlowControl object for state machine reset
	// boost::shared_ptr<SC_Mem> membus = boost::dynamic_pointer_cast<SC_Mem>(bus);
	// boost::shared_ptr<SC_SlowCtrl> scsc = membus->getSCTRL();

	// get SynapseControl object
	boost::shared_ptr<SynapseControl> synapse_control = getSC();

	// minCol is 4-aligned...
	if (minCol % 4)
		minCol -= minCol % 4;

	if (maxCol > 384) {
		string msg = "Sorry, only 384 synapse array columns.";
		dbg(::Logger::ERROR) << msg << Logger::flush;
		throw std::runtime_error(msg);
	}

	// inverted row indices
	synapseRect rect = {minRow, maxRow, minCol, maxCol};

	// clear synapse weights & correlation information
	for (uint i = 0; i < 256; ++i) {
		for (uint j = 0; j < 384; ++j) {
			_correlationInformation[i][j] = -1;
			_synapseWeightsOld[i][j] = -1;
			_synapseWeights[i][j] = -1;
			_synapseWeightsPreLUT[i][j] = -1;
		}
	}

	/* Benchmarking -- how many read_corr loops until bits flip? */
	// cout << _printIndentation << "calling benchmark" << endl;
	// benchmarkSTDPDrift();
	// return 0;

	/* debugging access to parameters */
	// TP: TODO: tidy up here, STDP stuff moved to SC_SlowCtrl::pbEvt
	bool proc_corr = true;
	bool read_corr = false; // TP: was formerly used for STDP curve recordings
	bool read_weights_pre_lut = false;
	bool read_weights_pre = true;
	bool read_weights_post = true;

	if (read_corr)
		assert(proc_corr);

	// read weights
	if (read_weights_pre_lut)
		readWeights(rect);

	// weights are not updated when correlations are processed (because LUT is identity)
	if (proc_corr) {
		LOG4CXX_DEBUG(logger, "identity LUT okay? "
		                          << synapse_control->fill_plut(SynapseControl::write, 1, 0));
	}

	// correlation flag reset
	if (proc_corr)
		processCorrFlags(rect);
	// read weights
	if (read_weights_pre)
		readWeights(rect);

	// after flag reset fill LUT linearly
	if (changeWeights == true) {
		LOG4CXX_DEBUG(logger, "custom LUT okay? " << synapse_control->fill_plut(
		                          SynapseControl::custom | SynapseControl::write, 1, 0));
	}

	// TP: needed for continuous STDP (for manual triggering of STDP see pbEvt)
	// move to changeWeights == True
	//
	// time constants              old vals | from tmjs_spikeycore
	// uint clockper=10;           // nanosecs
	// uint tsense=300/clockper;   // 100ns    | 100ns
	// uint tpcsec=60/clockper;    //  20ns    |  20ns
	// uint tpcorperiod=360;       //          | 252ns 2*tdel
	// synapse_control->write_time(tsense,tpcsec,tpcorperiod); // time register

	///////////////// from Spikey::sendSpikeTrain() /////////////////
	/* sync */

	// reset fifos and priority encoder
	neuronreset = true;
	writeSCtl();

	// reset event out buffers
	for (uint i = 0; i < hw_const->event_outs(); i++)
		setCCBit(hw_const->cr_eout_rst() + i, true);
	writeCC();
	for (uint i = 0; i < hw_const->event_outs(); i++)
		setCCBit(hw_const->cr_eout_rst() + i, false);
	writeCC();
	bus->setStime(0); // (sf) the event sorter requires that the systime
	                  // starts at zero after reset.

	Send(SpikenetComm::sync); // 1. sync
	neuronreset = false;      // 2. un-reset

	// TP: not used, because of bug in synapse finite-state machine
	// instead manual triggering of STDP (see pbEvt)
	// if(changeWeights){         // 3. activate continuous STDP updates
	// pccont = true;
	// cout << "continuous STDP is activated" << endl;
	//}

	writeSCtl(); // 4. execute

	// trigger the continuous update controller before sending spikes
	// if (proc_corr)
	// processCorrFlags(rect);

	/*  ____  _____ _   _ ____      ____  ____ ___ _  _______ _____ ____      _    ___ _   _  */
	/* / ___|| ____| \ | |  _ \    / ___||  _ \_ _| |/ / ____|_   _|  _ \    / \  |_ _| \ | | */
	/* \___ \|  _| |  \| | | | |   \___ \| |_) | || ' /|  _|   | | | |_) |  / _ \  | ||  \| | */
	/*  ___) | |___| |\  | |_| |    ___) |  __/| || . \| |___  | | |  _ <  / ___ \ | || |\  | */
	/* |____/|_____|_| \_|____/    |____/|_|  |___|_|\_\_____| |_| |_| \_\/_/   \_\___|_| \_| */
	/*                                                                                        */
	for (uint i = 0; i < st.d.size(); ++i)
		Send(SpikenetComm::write, st.d[i]);

	// mute all outputs (after dummy spike) => fixes status data reads/spike race condition
	for (uint i = 0; i < hw_const->event_outs(); i++)
		setCCBit(hw_const->cr_eout_rst() + i, true);
	writeCC(100); // adds additional delay, TODO: this should be "chain delay" (=> smaller than 100)

	// TP: see two previous comments by TP
	// pccont = false; //stop continuous STDP update controller
	// writeSCtl(1000);

	// get buffer stats directly after execution.
	// -> During readback, execute recspiketrain in the same order as SendSpikeTrain!
	getCC()->read_clkstat(3);
	getCC()->read_clkbstat(3);
	getCC()->read_clkhistat(3);
	/////////////////  end of from sendSpikeTrain() /////////////////

	// recSpikeTrain has to distinguish between this case and normal sendSpikeTrain-routine
	_lastRunWasSTDP = true; // TP: obsolete?

	// manual readout of correlation flags
	if (changeWeights == false) {
		if (read_corr) // read flags
			readCorrFlags(rect);
	}

	// read weights
	if (read_weights_post)
		readWeights(rect);

	/* flush here */
	Flush();
	Run();
	waitPbFinished();

	// receive SYNAPSE WEIGHTS (before resetting the flags)
	if (read_weights_pre_lut)
		receiveWeights(rect, _synapseWeightsPreLUT);

	// receive SYNAPSE WEIGHTS (before spike train)
	if (read_weights_pre)
		receiveWeights(rect, _synapseWeightsOld);

	/* receiving data */
	recSpikeTrain(stout);

	// receive CORRELATION FLAGS & SYNAPSE WEIGHTS (after spike train)
	if (read_corr && changeWeights == false)
		receiveCorrFlags(rect, _correlationInformation);

	if (read_weights_post)
		receiveWeights(rect, _synapseWeights);


	// Create output matrices.
	if (verbose && changeWeights == false) {
		for (uint col = minCol; col <= maxCol; ++col) {
			cout << "Col: " << dec << setw(3) << col << ':' << endl;
			for (uint line = 0; line < 256; ++line) {
				uint row = 255 - line;
				int& flag = _correlationInformation[row][col];
				int& valueOld = _synapseWeightsOld[row][col];
				int& value = _synapseWeights[row][col];

				if (!(line % 32))
					cout << "L: " << dec << setw(3) << line << "  ";
				switch (valueOld) {
					case -1:
						cout << ' ';
						break; // unset
					case 0:
						cout << '0';
						break; // synapse off (weight 0)
					case 15:
						cout << 'F';
						break; // synapse at maximum
					default:
						cout << hex << valueOld;
						break;
				}
				// cout << '/';
				switch (flag) {
					case -1:
						cout << '|';
						break; // not read
					case 0:
						cout << '-';
						break; // detected no correlation
					case 1:
						cout << 'A';
						break; // Acausal
					case 2:
						cout << 'C';
						break; // Causal
					case 3:
						cout << 'B';
						break; // Both
					default:
						cout << '?'; // this is a bug
						cout << '(' << dec << flag << ')';
						break;
				}
				// cout << '/';
				switch (value) {
					case -1:
						cout << ' ';
						break; // unset
					case 0:
						cout << '0';
						break; // synapse off (weight 0)
					case 15:
						cout << 'F';
						break; // synapse at maximum
					default:
						cout << hex << value;
						break;
				}
				if (((line + 1) % 32) == 0)
					cout << endl;
				else
					cout << ' ';
			}
			cout << endl;
		}
	}

	// get dropped/modified events:
	boost::shared_ptr<SC_Mem> mem(boost::dynamic_pointer_cast<SC_Mem>(bus));
	if (mem != NULL) { // check if correct subclass used as bus
		mem->savePbContent(); // hack!
		if (et != NULL) {
			et->d = *(mem->eev(chip())); // copy error events
			mem->eev(chip())->clear();   // clear error events
		}
	}
}

void PySpikey::benchmarkSTDPDrift()
{
	// time constants              old vals | from tmjs_spikeycore
	uint clockper = 10;           // nanosecs
	uint tsense = 300 / clockper; // 100ns    | 100ns
	uint tpcsec = 60 / clockper;  //  20ns    |  20ns
	uint tpcorperiod = 360;       //          | 252ns 2*tdel

	// get the synapse control object
	boost::shared_ptr<SynapseControl> synapse_control = getSC();

	// intitialize STDP lookup
	synapse_control->write_time(tsense, tpcsec, tpcorperiod); // time register
	synapse_control->read_time();                             // ECM: obsolete?
	if (!synapse_control->check_time(tsense, tpcsec, tpcorperiod))
		cout << _printIndentation << "check_time failed!" << endl;

	// Do not update weights before reading correlation data!
	synapse_control->fill_plut(SynapseControl::write, 1, 0);
	// uint64_t dummy;
	// synapse_control->rcv_data(dummy);

	///////////////// from Spikey::sendSpikeTrain()    /////////////////
	// adjust system time to spike train time
	bus->setStime(0); // to be modified!!!

	//***** sync
	// reset fifos and priority encoder
	neuronreset = true;
	writeSCtl();
	for (uint i = 0; i < hw_const->event_outs(); i++)
		setCCBit(hw_const->cr_eout_rst() + i, true);
	writeCC();
	for (uint i = 0; i < hw_const->event_outs(); i++)
		setCCBit(hw_const->cr_eout_rst() + i, false);
	writeCC();

	Send(SpikenetComm::sync); // 1. sync
	neuronreset = false;      // 2. un-reset
	writeSCtl();              // 3. execute!
	/////////////////  end of from sendSpikeTrain() /////////////////


	// clear synapse weights
	for (uint i = 0; i < 384; ++i) {
		for (uint j = 0; j < 256; ++j) {
			_correlationInformation[i][j] = -1;
			_synapseWeightsOld[i][j] = -1;
			_synapseWeights[i][j] = -1;
		}
	}

	uint row = 255 - 32;
	uint col = 51;

	// reset CORRELATION FLAGS, fill_plut => not linear => no weight updates needed before
	//    synapse_control->proc_corr(row,row,true,tdel+2*(tpcorperiod+4*64));


	uint64_t ra, rb, rc, rd;

	synapse_control->read_sram(row, col, tsense * 5);
	synapse_control->close();
	synapse_control->read_corr(row, col, 1000);
	synapse_control->close();

	synapse_control->rcv_data(ra);
	synapse_control->rcv_data(rb);

	synapse_control->read_sram(row, col, tsense * 5);
	synapse_control->close();
	synapse_control->read_corr(row, col, 1000);
	synapse_control->close();

	synapse_control->rcv_data(rc);
	synapse_control->rcv_data(rd);

	cout << _printIndentation << "ra ";
	bin(ra, std::cout, 4);
	cout << endl;
	cout << _printIndentation << "rb ";
	bin(rb, std::cout, 4);
	cout << endl;
	cout << _printIndentation << "rc ";
	bin(rc, std::cout, 4);
	cout << endl;
	cout << _printIndentation << "rd ";
	bin(rd, std::cout, 4);
	cout << endl;

	// read CORRELATION FLAGS code was here (removed by ECM)

	// clear memory, very important, otherwise massive mem leak!
	boost::shared_ptr<SC_Mem> mem(boost::dynamic_pointer_cast<SC_Mem>(bus));
	if (mem != NULL) {
		// check if correct subclass used as bus
		// mem->rcvd(chip())->clear();
		mem->eev(chip())->clear(); // clear error events
	}
}

vector<vector<int>> PySpikey::getCorrFlags(uint minRow, uint maxRow, uint minCol, uint maxCol)
{
	if (maxCol < minCol) {
		PyErr_SetString(PyExc_TypeError, "in PySpikey::getCorrFlags(): maxCol < minCol!");
		boost::python::throw_error_already_set();
	}
	if (maxRow < minRow) {
		PyErr_SetString(PyExc_TypeError, "in PySpikey::getCorrFlags(): maxRow < minRow!");
		boost::python::throw_error_already_set();
	}

	uint numRows = maxRow - minRow + 1;
	uint numCols = maxCol - minCol + 1;
	_corrFlagsForPython.resize(numRows);
	for (uint i = 0; i < numRows; ++i)
		_corrFlagsForPython[i].resize(numCols);

	uint trow = 0;
	for (uint row = minRow; row <= maxRow; ++row) {
		uint tcol = 0;

		for (uint col = minCol; col <= maxCol; ++col) {
			_corrFlagsForPython[trow][tcol] = _correlationInformation[255 - row][col];
			++tcol;
		}
		++trow;
	}

	return _corrFlagsForPython;
}


vector<vector<int>> PySpikey::getSynapseWeights(uint minRow, uint maxRow, uint minCol, uint maxCol)
{
	if (maxRow < minRow) {
		PyErr_SetString(PyExc_TypeError, "in PySpikey::getSynapseWeights(): maxRow < minRow!");
		boost::python::throw_error_already_set();
	}
	if (maxCol < minCol) {
		PyErr_SetString(PyExc_TypeError, "in PySpikey::getSynapseWeights(): maxCol < minCol!");
		boost::python::throw_error_already_set();
	}
	uint numRows = maxRow - minRow + 1;
	uint numCols = maxCol - minCol + 1;
	_weightsForPython.resize(numRows);
	for (uint i = 0; i < numRows; ++i)
		_weightsForPython[i].resize(numCols);

	uint trow = 0;
	for (uint row = minRow; row <= maxRow; ++row) {
		uint tcol = 0;
		for (uint col = minCol; col <= maxCol; ++col) {
			_weightsForPython[trow][tcol] = _synapseWeights[255 - row][col];
			++tcol;
		}
		++trow;
	}

	return _weightsForPython;
}


/*             _ __     __    _ _                          *
 *   __ _  ___| |\ \   / /__ | | |_ __ _  __ _  ___  ___   *
 *  / _` |/ _ \ __\ \ / / _ \| | __/ _` |/ _` |/ _ \/ __|  *
 * | (_| |  __/ |_ \ V / (_) | | || (_| | (_| |  __/\__ \  *
 *  \__, |\___|\__| \_/ \___/|_|\__\__,_|\__, |\___||___/  *
 *  |___/                                |___/             */
vector<vector<double>> PySpikey::getVoltages()
{
	/* To be used after chip has been configured. */

	uint numBlocks = SpikeyConfig::num_blocks;
	uint numVouts = PramControl::num_vout_adj; // SpikeyConfig::num_vout;

	_voltagesForPython.resize(numBlocks);
	for (uint i = 0; i < numBlocks; ++i)
		_voltagesForPython[i].resize(numVouts);

	// analog readout control class
	boost::shared_ptr<AnalogReadout> analog_readout = getAR();

	// run through blocks
	for (uint b = 0; b < numBlocks; ++b) {
		// run through all vouts except the three 'aro' values (for spikey2: num_vouts_adj = 20)
		for (uint n = 0; n < numVouts; ++n) {
			vector<double> w;

			if (!b) {
				analog_readout->clear(analog_readout->voutr, 200);         // clear chain 1 ?
				analog_readout->set(analog_readout->voutl, n, false, 200); // set without verify
			} else {
				analog_readout->clear(analog_readout->voutl, 200);         // clear chain 0 ?
				analog_readout->set(analog_readout->voutr, n, false, 200); // set without verify
			}

			for (int i = 0; i < 10; ++i) // averaging
				w.push_back(readAdc(SBData::ibtest));

			_voltagesForPython[b][n] = mean(w);
		}
	}
	return _voltagesForPython;
}

vector<double> PySpikey::getVout(uint blk, uint vout, uint count)
{

	_ibtestValuesForPython.clear();

	boost::shared_ptr<SC_Mem> membus = boost::dynamic_pointer_cast<SC_Mem>(bus);
	boost::shared_ptr<SC_SlowCtrl> sc = membus->getSCTRL();
	assert(sc);
	boost::shared_ptr<AnalogReadout> analog_readout = getAR();
	assert(analog_readout);

	// clear both chains
	analog_readout->clear(analog_readout->voutl, 2000);
	analog_readout->clear(analog_readout->voutr, 2000);

	// setting blk/vout
	if (!blk)
		analog_readout->set(analog_readout->voutl, vout, false, 200);
	else
		analog_readout->set(analog_readout->voutr, vout, false, 200);

	/*
	 * voutl ---.
	 *          |----- ibtest
	 * voutr ---'  |
	 *             ib
	 */

	sc->setAnaMux(8); // disable MUX

	for (uint i = 0; i < count; ++i) {
		usleep(10000);
		_ibtestValuesForPython.push_back(readAdc(SBData::ibtest));
	}

	return _ibtestValuesForPython;
}

vector<double> PySpikey::getIbCurr(uint count)
{

	_ibtestValuesForPython.clear();

	boost::shared_ptr<SC_Mem> membus = boost::dynamic_pointer_cast<SC_Mem>(bus);
	boost::shared_ptr<SC_SlowCtrl> sc = membus->getSCTRL();
	assert(sc);
	boost::shared_ptr<AnalogReadout> analog_readout = getAR();
	assert(analog_readout);

	// clear both chains
	analog_readout->clear(analog_readout->voutl, 2000);
	analog_readout->clear(analog_readout->voutr, 2000);

	/* now, only ib is connected to ibtest:
	 *
	 * voutl ---.
	 *          |----- ibtest
	 * voutr ---'  |
	 *             ib
	 */

	sc->setAnaMux(8); // disable MUX

	for (uint i = 0; i < count; ++i) {
		// usleep(50000); // fixed in nathan_top_newtempsense.bit (01/2008)
		_ibtestValuesForPython.push_back(readAdc(SBData::ibtest));
	}
	return _ibtestValuesForPython;
}

void PySpikey::setupFastAdc(float simtime)
{
	boost::shared_ptr<SC_Mem> membus = boost::dynamic_pointer_cast<SC_Mem>(bus);
	boost::shared_ptr<SC_SlowCtrl> sc = membus->getSCTRL();
	assert(sc); // Don't have a slow control object!
	sc->setupFastAdc((unsigned int)floor(simtime * 1000.0 + 1.0), 0);
}

boost::python::object PySpikey::readFastAdc()
{
	using namespace boost::python;

	boost::shared_ptr<SC_Mem> membus = boost::dynamic_pointer_cast<SC_Mem>(bus);
	boost::shared_ptr<SC_SlowCtrl> sc = membus->getSCTRL();
	assert(sc); // Don't have a slow control object!

	// create numpy c++ array
	const size_t size = sc->adc_num_samples * 2;
	if (sc->adc_num_samples > 32 * 1024 * 1024 - 1) { // 128MB
		throw std::runtime_error("memory overflow for ADC data");
	};
	const int num_dims = 1;
	npy_intp dims[] = {static_cast<npy_intp>(size)};
	object np_mem_array(handle<>(PyArray_SimpleNew(num_dims, dims, NPY_INT)));
	int* mem_data = static_cast<int*>(PyArray_DATA((PyArrayObject*)np_mem_array.ptr()));

	// fill array
	// larger chunks (e.g. 4Msamples) may lead to timeout errors in libusb_bulk_transfer
	const unsigned int max_size = 2 * 1024 * 1024 - 1;
	for (uint32_t chunk = 0; chunk < sc->adc_num_samples; chunk += max_size) {
		unsigned int chunk_size = max_size;
		if (chunk + chunk_size >= sc->adc_num_samples) {
			chunk_size = sc->adc_num_samples - chunk;
		}

		// TODO: address to parameter file
		Vbufuint_p data = sc->mem->readBlock(sc->adc_start_adr + 0x08000000 + chunk, chunk_size);

		for (size_t i = 0; i < chunk_size; i++) {
			*mem_data = ((data[i] >> 16) & 0xfff);
			++mem_data;
			*mem_data = (data[i] & 0xfff);
			++mem_data;
		}
	}

	return np_mem_array;
}

void PySpikey::triggerAdc()
{
	boost::shared_ptr<SC_Mem> membus = boost::dynamic_pointer_cast<SC_Mem>(bus);
	boost::shared_ptr<SC_SlowCtrl> sc = membus->getSCTRL();
	assert(sc); // Don't have a slow control object!

	sc->triggerAdc();
}
