#define PY_ARRAY_UNIQUE_SYMBOL hal

#include <boost/python.hpp>
#include <boost/python/wrapper.hpp>

using namespace spikey2;

//! Provides access to the so-called Flyspi boards, which carry the Spikey chips.
class PySC_Mem : public SC_Mem
{

public:
	PySC_Mem(uint starttime, std::string workstation = "");
	void intClear();
	int minTimebin();
	int stime();
	std::string getWorkStationName();
};
