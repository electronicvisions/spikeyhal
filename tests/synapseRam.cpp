#include <gtest/gtest.h>

#include "common.h"

#include "idata.h"
#include "sncomm.h"
#include "sc_sctrl.h"
#include "sc_pbmem.h"

#include "ctrlif.h"
#include "spikenet.h"

#include "synapse_control.h"

static log4cxx::LoggerPtr logger = log4cxx::Logger::getLogger("Tst.Syn");

namespace spikey2
{
TEST(HWTest, synapseRAM)
{
	/*
	 * Randomly writes and reads full synapse RAM.
	 */
	boost::shared_ptr<SpikenetComm> bus(new SC_Mem()); // playback memory interface
	boost::shared_ptr<Spikenet> chip(new Spikenet(bus));
	boost::shared_ptr<SynapseControl> synControl = chip->getSC(); // get pointer to synapse control
	                                                              // class

	uint noTrials = 10; // number of trials
	uint blockMax = 6, rowMax = 256, colMax = 64; // number of blocks, rows and columns
	uint errors = 0; // error counter

	// timing parameters
	uint clockper = 10; // clock period in nano seconds
	uint tsense = 100 / clockper; // 100ns
	uint tpcsec = 20 / clockper; // 20ns
	uint tdel = 6 + tsense + tpcsec;

	unsigned long time_seed = time(NULL);
	LOG4CXX_INFO(logger, "random seed is " << time_seed);

	uint64_t dataIn, dataOut;
	for (uint trial = 0; trial < noTrials; ++trial) {
		LOG4CXX_INFO(logger, "Trial " << trial);

		srand(time_seed);
		// write synaptic weights
		for (uint row = 0; row < rowMax; ++row) {
			for (uint col = 0; col < colMax; ++col) {
				dataIn = 0;
				for (uint i = 0; i < blockMax; i++) { // pack 6 synaptic weights into data package
					dataIn |= (rand() & 15) << (i * 4);
				}
				synControl->write_sram(row, col, dataIn, tdel);
			}
			synControl->close();
		}
		// read synaptic weights
		for (uint row = 0; row < rowMax; ++row) {
			for (uint col = 0; col < colMax; ++col) {
				synControl->read_sram(row, col, tdel);
			}
			synControl->close();
		}

		// write to memory and run
		chip->Flush();
		chip->Run();

		srand(time_seed); // to reproduce random number for each block of synapses
		for (uint row = 0; row < rowMax; ++row) {
			for (uint col = 0; col < colMax; ++col) {
				dataIn = 0;
				dataOut = 0;
				for (uint i = 0; i < blockMax; i++) {
					dataIn |= (rand() & 15) << (i * 4);
				}
				synControl->rcv_data(dataOut);
				dataOut = (dataOut >> (bus->hw_const->sc_aw() + bus->hw_const->sc_commandwidth())) &
				          mmw(bus->hw_const->sc_dw());
				if (dataOut != dataIn) {
					errors += 1;
					LOG4CXX_ERROR(logger,
					              "Received weight differs from (row, column, data in, data out): "
					                  << row << ", " << col << ", " << dataIn << ", " << dataOut)
				}
			}
		}
		bool success = (errors == 0);
		LOG4CXX_INFO(logger, "Check synaptic weights: " << (success ? "pass" : "fail"));
		EXPECT_EQ(true, success);
	}
}
}
