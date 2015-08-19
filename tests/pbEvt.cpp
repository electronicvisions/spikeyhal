#include <gtest/gtest.h>

#include <exception>

/* ugly stuff */
#include "common.h" // library includes
#include "idata.h"
#include "sncomm.h"
#include "spikenet.h" //communication and chip classes
#include "sc_sctrl.h"

uint randomseed = 42; // see createtb.cpp

static log4cxx::LoggerPtr logger = log4cxx::Logger::getLogger("Tst.pbEvt");

namespace spikey2
{
TEST(pbEvtTests, checkSomeEventVector)
{
	// create object to test
	boost::shared_ptr<SC_SlowCtrl> sc;
	EXPECT_NO_THROW(sc = boost::shared_ptr<SC_SlowCtrl>(new SC_SlowCtrl()););

	IData a;
	uint64_t b;
	uint oldstime, newstime;
	vector<IData> in, out;
	vector<uint64_t> out_sdram;
	fstream inev(GTESTPATH "/pbEvt-test1-in.dat", fstream::in);
	fstream outev(GTESTPATH "/pbEvt-test1-out.dat", fstream::in);
	fstream outsdram(GTESTPATH "/pbEvt-test1-out-sdram.dat", fstream::in);

	// read fixture data
	inev >> dec >> oldstime;
	while (inev >> a)
		in.push_back(a);
	outev >> dec >> newstime;
	while (outev >> a)
		out.push_back(a);
	while (outsdram >> hex >> b)
		out_sdram.push_back(b);

	// call function to test
	EXPECT_NO_THROW(sc->pbEvt(in, /*newstime&*/ oldstime, /*stime*/ 90, /*chip*/ 0););

	// check side-effects
	EXPECT_EQ(oldstime, newstime);
	EXPECT_EQ(true, out == *sc->ev(0));
	// the output file contains only this part
	EXPECT_EQ(true, out_sdram == sc->sdrambuf);
}
} // namespace
