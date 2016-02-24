#include <iostream>


// forward declarations required outside namespaces!
class PySpikey;
class FacetsHWS1V2Access;
class TMSpikeyClass;
class TMSpikeyClassAG;

namespace spikey2
{

class SpikeyVoutCalib;

//! Adds calibrate-functions to the Spikey class
class SpikeyCalibratable : public Spikey
{
	friend class SpikeyVoutCalib;
	friend class ::TMSpikeyClass;
	friend class ::TMSpikeyClassAG;
	friend class ::FacetsHWS1V2Access;
	friend class ::PySpikey;

public:
	SpikeyCalibratable(boost::shared_ptr<Spikenet> snet, float clk = 10.0, uint chipid = 0, int spikeyNr = 0,
	                   string calibfile = "spikeycalib.xml");
	SpikeyCalibratable(boost::shared_ptr<SpikenetComm> comm, float clk = 10.0, uint chipid = 0, int spikeyNr = 0,
			           string calibfile = "spikeycalib.xml");
	virtual ~SpikeyCalibratable(){};
	boost::shared_ptr<SpikeyVoutCalib> clb;

private:
	virtual void loadParam(boost::shared_ptr<SpikeyConfig>, vector<PramData>&);
	virtual uint convVoltDac(double voltage, int voutNr = 0);
	vector<uint> convVoltDacWarnings;
};

} // end of namespace spikey2
