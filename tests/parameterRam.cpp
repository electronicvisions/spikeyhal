#include <gtest/gtest.h>

#include "common.h"

#include "idata.h"
#include "sncomm.h"
#include "sc_sctrl.h"
#include "sc_pbmem.h"

#include "ctrlif.h"
#include "spikenet.h"

#include "pram_control.h"

static log4cxx::LoggerPtr logger = log4cxx::Logger::getLogger("Tst.Ram");

namespace spikey2
{
TEST(HWTest, parameterRAM)
{
	/*
	 * Randomly writes and reads all parameter RAMs.
	 */
	boost::shared_ptr<SpikenetComm> bus(new SC_Mem()); // playback memory interface
	boost::shared_ptr<Spikenet> chip(new Spikenet(bus));
	boost::shared_ptr<PramControl> pram_control = chip->getPC(); // get pointer to parameter control
	                                                             // class

	vector<uint> parametersIn;

	uint noParams = 3072; // number of parameters
	uint noLutEntries = 16; // number of LUT entries
	uint noTrials = 10; // number of trials

	unsigned long time_seed = time(NULL);
	LOG4CXX_INFO(logger, "random seed is " << time_seed);
	srand(time_seed);

	for (uint trial = 0; trial < noTrials; trial++) {
		LOG4CXX_INFO(logger, "Trial " << trial);

		parametersIn.clear();

		// write and read global parameters
		parametersIn.push_back(rand() % 256);
		parametersIn.push_back(rand() % 3744);

		pram_control->write_period(parametersIn[0]);
		pram_control->write_parnum(parametersIn[1]);

		pram_control->read_period();
		pram_control->read_parnum();

		// boost::shared_ptr<ChipControl>  chip_control = chip->getCC(); //get pointer to chip
		// control class
		// chip_control->setCtrl((1<<(cr_pram_en)), 100);

		// write and read parameter RAM
		for (uint address = 0; address < noParams; address++) {
			parametersIn.push_back(rand() % 1024); // address on anablock
			parametersIn.push_back(rand() % 1024); // value
			parametersIn.push_back(rand() % 16); // LUT address
			uint delay = (rand() % 5) + 5;
			pram_control->write_pram(address, parametersIn[2 + address * 3 + 0],
			                         parametersIn[2 + address * 3 + 1],
			                         parametersIn[2 + address * 3 + 2], delay);
			delay = (rand() % 5) + 5;
			pram_control->read_pram(address, delay);
		}

		// write and read look-up tables
		for (uint lutAddress = 0; lutAddress < noLutEntries; lutAddress++) {
			parametersIn.push_back(rand() % 16); // time value of LUT
			parametersIn.push_back(rand() % 16); // boost time exponent of LUT
			parametersIn.push_back(rand() % 256); // time exponent of LUT
			parametersIn.push_back(rand() % 16); // boost time value of LUT
			uint delay = (rand() % 5) + 5;
			pram_control->write_lut(lutAddress, parametersIn[2 + 3 * noParams + lutAddress * 4 + 0],
			                        parametersIn[2 + 3 * noParams + lutAddress * 4 + 1],
			                        parametersIn[2 + 3 * noParams + lutAddress * 4 + 2],
			                        parametersIn[2 + 3 * noParams + lutAddress * 4 + 3], delay);
			delay = (rand() % 5) + 5;
			pram_control->read_lut(lutAddress, delay);
		}

		// write to memory and run
		chip->Flush();
		chip->Run();

		// check results
		bool success = pram_control->check_period(parametersIn[0]);
		LOG4CXX_INFO(logger, "Check period: " << (success ? "pass" : "fail"));
		EXPECT_EQ(true, success);

		success = pram_control->check_parnum(parametersIn[1]);
		LOG4CXX_INFO(logger, "Check numparam: " << (success ? "pass" : "fail"));
		EXPECT_EQ(true, success);

		success = true;
		for (uint address = 0; address < noParams; address++) {
			if (!(pram_control->check_pram(address, parametersIn[2 + address * 3 + 0],
			                               parametersIn[2 + address * 3 + 1],
			                               parametersIn[2 + address * 3 + 2]))) {
				success = false;
				LOG4CXX_ERROR(logger, "Parameter RAM check failed at parameter: " << address);
				break;
			}
		}
		LOG4CXX_INFO(logger, "Check parameter RAM: " << (success ? "pass" : "fail"));
		EXPECT_EQ(true, success);

		success = true;
		for (uint lutAddress = 0; lutAddress < noLutEntries; lutAddress++) {
			if (!(pram_control->check_lut(lutAddress,
			                              parametersIn[2 + 3 * noParams + lutAddress * 4 + 0],
			                              parametersIn[2 + 3 * noParams + lutAddress * 4 + 1],
			                              parametersIn[2 + 3 * noParams + lutAddress * 4 + 2],
			                              parametersIn[2 + 3 * noParams + lutAddress * 4 + 3]))) {
				success = false;
				LOG4CXX_ERROR(logger, "Look-up table check failed at entry: " << lutAddress);
				break;
			}
		}
		LOG4CXX_INFO(logger, "Check look-up table: " << (success ? "pass" : "fail"));
		EXPECT_EQ(true, success);
	}
}
}
