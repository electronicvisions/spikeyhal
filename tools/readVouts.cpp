#include <gtest/gtest.h>

#include "common.h"

#include "idata.h"
#include "sncomm.h"
#include "sc_sctrl.h"
#include "sc_pbmem.h"

#include "ctrlif.h"
#include "spikenet.h"

#include "pram_control.h"
#include "synapse_control.h"
#include "spikeyconfig.h"
#include "spikey.h"
#include "spikeycalibratable.h"
#include "spikeyvoutcalib.h"

static log4cxx::LoggerPtr logger = log4cxx::Logger::getLogger("Tool.ReadVouts");

using namespace spikey2;
uint randomseed = 42;

int main(int argc, char* argv[])
{
	/*
	 * Reads out all vouts, e.g. used for testing of bonding.
	 */

	boost::shared_ptr<SpikenetComm> bus = boost::shared_ptr<SC_Mem>(new SC_Mem()); // playback memory interface
	boost::shared_ptr<Spikenet> chip = boost::shared_ptr<Spikenet>(new Spikenet(bus, 0));
	boost::shared_ptr<SpikeyCalibratable> sp = boost::shared_ptr<SpikeyCalibratable>(new SpikeyCalibratable(chip));
	boost::shared_ptr<SpikeyConfig> cfg = boost::shared_ptr<SpikeyConfig>(new SpikeyConfig(bus->hw_const));
	boost::shared_ptr<SC_Mem> mem(boost::dynamic_pointer_cast<SC_Mem>(chip->bus));

	// load network to initialize chip
	std::string filenameConfig = "tests/spikeyconfig_decorrnetwork.out";
	assert(cfg->readParam(filenameConfig));
	sp->config(cfg); // config chip
	sp->clb->loadConfig(cfg); // use this config also for vout readout

	// allowed parameter range
	double maxLimit = 1.1;
	double minLimit = 0.9;

	double* xvals = new double[1];
	double* yvals = new double[1];
	double* yweights = new double[1];

	xvals[0] = 1.0; // target value
	yvals[0] = 0.0;
	yweights[0] = 0.0;

	// write results to file for further analysis
	ofstream outfile;
	std::string workstation = mem->getWorkStationName();
	outfile.open("read_vouts_" + workstation + ".dat");

	for (uint b = 0; b < SpikeyConfig::num_blocks; ++b) {
		//for (uint n = 0; n < PramControl::num_vout_adj; ++n) {
		for (uint n = 0; n < 10; ++n) {
			sp->clb->readVoutValues(b, n, 10, xvals, 1, yvals, yweights, true);

			outfile << b << " " << n << " " << yvals[0] << " " << yweights[0];
			outfile << std::endl;

			assert(yvals[0] < maxLimit);
			assert(yvals[0] > minLimit);
		}
	}

	outfile.close();
}
