#define PY_ARRAY_UNIQUE_SYMBOL hal

#include <boost/python.hpp>
#include <boost/python/wrapper.hpp>


using namespace spikey2;

//! Python wrapper class for the C++ class SpikenetComm. Encapsulates the transfer of information
//between the spikenetcontroller and the Spikey chips.
class PySpikenetComm : public SpikenetComm, public boost::python::wrapper<SpikenetComm>
{
	SpikenetComm::Commstate Send(Mode mode, IData data = emptydata, uint del = 0, uint chip = 0,
	                             uint syncoffset = 0);
	SpikenetComm::Commstate Receive(Mode mode, IData& data, uint chip = 0);

public:
	PySpikenetComm(std::string type) : SpikenetComm(type){};
};
