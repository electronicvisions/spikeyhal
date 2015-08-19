#define PY_ARRAY_UNIQUE_SYMBOL hal

#include <boost/python.hpp>
#include <string>

using namespace spikey2;

class PySpikeyTM
{

public:
	boost::shared_ptr<Spikey> sp;
	std::vector<boost::shared_ptr<Spikenet>> chip;
	boost::shared_ptr<SpikenetComm> bus;

	std::string tbName;
	std::string rName;

	uint startTime;
	uint nathanNum;
	uint chipNum;
	uint loops[3];
	time_t now;
	std::string error;

	/// constructor sets some variables
	PySpikeyTM(uint nathan, uint chip) : nathanNum(nathan), chipNum(chip)
	{
		tbName = "SpikeyTM";
		rName = "SpikeyTM";
		startTime = 0;
		loops[0] = 10; // loops of link test
		loops[1] = 1;  // loops of parameterRAM test - unused
		loops[2] = 50; // loops of (event) loopback test
		error = "";
		time(&now); // a time for random seeds :)
		sp = boost::shared_ptr<Spikey>();
		std::cout << "Using Nathan " << nathanNum << ", chip " << chipNum << std::endl;
	}

	/// destructor (does nothing ;))
	~PySpikeyTM() {}

	/// access to private error-variable :)
	std::string getError() { return error; }

	/// creating objects
	void initMem(uint busModel, bool withSpikey = false);

	bool test();          //< call following tests
	bool link();          //< test new (72bit) link with alternating '1' and '0'
	bool parameterRAM();  //< test parameter RAM randomly
	bool eventLoopback(); //< test event loopback

	/// eventLoopback()-helpers:
	uint bool2int(std::vector<bool> array);
	bool checkEvents(uint& firsterr, SpikeTrain& tx, SpikeTrain& rx,
	                 boost::shared_ptr<SC_SlowCtrl> sct);
};
