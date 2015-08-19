#include <gtest/gtest.h>
#include "logger.h"
#include "networks/decorrNetwork.h"

static log4cxx::LoggerPtr logger = log4cxx::Logger::getLogger("Tst.DNW");

namespace spikey2
{

TEST(HWTest, runDecorrNetwork)
{
	for (int i = 0; i < 3; i++) {
		decorrNetwork network;
		uint spikes_no = network.run();
		// limit: 10Hz * 10s * 192 neurons = 19200
		ASSERT_TRUE(spikes_no > 19200);
	}
}

TEST(HWTest, runDecorrNetworkInf)
{
	/* used for stability tests */
	for (int i = 0; i < 50000; i++) { // only 50000 (4GB), because of memory leak
		std::cout << i << std::endl;
		decorrNetwork network;
		network.run();
	}
}
}
