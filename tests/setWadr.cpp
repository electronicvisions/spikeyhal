#include <gtest/gtest.h>

#include <exception>

/* ugly stuff */
#include "common.h" // library includes
#include "idata.h"
#include "sncomm.h"
#include "spikenet.h" //communication and chip classes
#include "sc_sctrl.h"

static log4cxx::LoggerPtr logger = log4cxx::Logger::getLogger("Tst.Wadr");

namespace spikey2
{
TEST(HWTest, setWadr)
{
	// create object to test
	boost::shared_ptr<SC_SlowCtrl> obj;
	EXPECT_NO_THROW(obj = boost::shared_ptr<SC_SlowCtrl>(new SC_SlowCtrl()););

	uint adrWrite = (1 << 24); // write this address to configuration register
	uint adrRead = 0; // and test whether same address is read back from configuration register
	uint adrCurRead = 0; // and test whether same address is written from configuration register to
	                     // actual RAM pointer
	// call function to test
	EXPECT_NO_THROW(obj->setWadr(adrWrite); obj->getWadr(adrRead);
	                obj->resetPlayback(); // triggers config register to RAM pointer
	                adrCurRead = obj->curwadr(););
	// std::cout << "write " << hex << adrWrite << " read control register " << hex << adrRead << "
	// read RAM pointer " << hex << adrCurRead << std::endl;
	EXPECT_EQ(adrWrite, adrRead);
	EXPECT_EQ(adrWrite, adrCurRead);
}
} // namespace
