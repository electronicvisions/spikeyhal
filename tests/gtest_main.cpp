#include <gtest/gtest.h>
#include "logging_ctrl.h"

class SpikeyTestEnvironment : public ::testing::Environment
{
public:
	// explicit SpikeyTestEnvironment() = default;

	void SetUp()
	{
		// configure logger
		logger_log_to_file(filename, log4cxx::Level::getInfo());
		static log4cxx::LoggerPtr logger = log4cxx::Logger::getLogger("Tst");
	}
	void TearDown() {}

private:
	std::string filename = "gtest_log.txt";
};

int main(int argc, char* argv[])
{
	testing::InitGoogleTest(&argc, argv);

	SpikeyTestEnvironment* g_env = new SpikeyTestEnvironment;
	::testing::AddGlobalTestEnvironment(g_env);

	return RUN_ALL_TESTS();
}
