#include "hardwareConstantsRev4USB.h"
#include "hardwareConstantsRev5USB.h"

#include "common.h"
#include "logger.h"

namespace spikey2
{

// SpikeyConfig and SpikeTrain are used to hold the argument data for Spikey
//
// it uses a common coordinate system identical to the event coordinates in IData
// block: left=0, right=1
// col: 0=innermost, 3*64-1:outermost neuron column
// row: 0=lowest, 255=uppermost synapse row


// ******** MemObj *********
// memobj represents an allocated memory object in the playback memory
class MemObj
{
public:
	enum state {
		invalid = 0,
		ok = 1,
		allocated = 3
	} // allocated means a copy resides in the playback mem
	_state;

private:
	uint adr; // base address
	uint size; // size in 32bit words
public:
	MemObj() : _state(invalid){};
	MemObj(state s) : _state(s){};
};


// ******** configures a spikey chip ******
//! Container class for all configuration parameter values of a Spikey chip.
class SpikeyConfig
{
protected:
	::Logger& dbg;
	boost::shared_ptr<HardwareConstants> hw_const;

public:
	MemObj mem;

	enum metric {
		num_presyns = 256,
		num_neurons = 64 * 3,
		num_blocks = 2,
		num_prienc = 3, // number of priority encoders per block
		num_perprienc = 64, // number of neurons per priority encoder
		num_nadr = 256, // number of neuron addresses per block
		num_nc = 4,
		num_sc = 8 // number of bits in nc, sc bitset
	};
	enum SCupdate {
		ud_verify = 0x1000,
		ud_chip = 32,
		ud_dac = 16,
		ud_param = 8,
		ud_weight = 4,
		ud_colconfig = 2,
		ud_rowconfig = 1,
		ud_config = 3,
		ud_none = 0,
		ud_all = 0xff
	} valid; // mask valid fields
	// timing, all units are ns
	float tsense,    // spikey internal timing: sense synapse ram bitlines
	    tpcsec,      // pre-charge time for secondary read when processing correlation
	    tpcorperiod; // minimum time used for the correlation processing of a single row
	// external analog, all units are either uA or V
	float irefdac, // reference current of spikey dac
	    vcasdac,   // spikey dac cascode voltage
	    vm,        // spikey synapse array correlation measurement Vm (precharge voltage of m.cap)
	    vrest,     // spikey synapse driver resting potential
	    vstart;    // spikey synapse driver start potential

	// currents
	vector<float> voutbias, // see pram_control.h
	    probebias, outamp, biasb;
	// voltages
	vector<float> vout, probepad;

	// neurons and synapses
	struct NeuronConf
	{
		bitset<num_nc> config;
		float ileak, // leakage ota bias
		    icb; // comparator bias
		NeuronConf()
		{
			config = 0;
			ileak = 0;
			icb = 0;
		};
	};
	std::valarray<NeuronConf> neuron;

	struct SynapseConf
	{
		bitset<num_sc> config;
		float drviout, adjdel, drvifall, drvirise;
		SynapseConf()
		{
			config = 0;
			drviout = 0;
			adjdel = 0;
			drvifall = 0;
			drvirise = 0;
		};
	};
	std::valarray<SynapseConf> synapse;

	// weights: three dimensions:
	// outer: block 0,1
	// middle: row 0-255
	// inner: col 0-3*64
	std::valarray<ubyte> weight;

	// ******** Constructors ********
	SpikeyConfig(); // nothing valid after default const.
	SpikeyConfig(SCupdate v); // fields v valid from the beginning
	SpikeyConfig(boost::shared_ptr<HardwareConstants>); // nothing valid after default const.
	SpikeyConfig(boost::shared_ptr<HardwareConstants>, SCupdate v); // fields v valid from the
	                                                                // beginning
	// ******** Destructor ********
	~SpikeyConfig()
	{
		hw_const = boost::shared_ptr<HardwareConstants>();
	}; // nothing valid after default const.
	// ******** methods ********
	// returns gslices to access a row or a col of synapses
	std::valarray<ubyte> col(uint block, uint col) const;
	slice_array<ubyte> col(uint block, uint col);
	std::valarray<ubyte> row(uint block, uint row) const;
	slice_array<ubyte> row(uint block, uint row);
	ubyte& getWeight(uint block, uint row, uint col);

	bool isValid(SCupdate u) const { return (valid & u) == u; };
	bool isValid(int u) const
	{
		return (valid & static_cast<SCupdate>(u)) == static_cast<SCupdate>(u);
	};

	void setValid(SCupdate u, bool on);
	void setValid(int u, bool on) { setValid(static_cast<SCupdate>(u), on); };

	// read/write spikeyconfig from file
	bool writeParam(string name);
	bool readParam(string name);

	void updateHardwareConstants(boost::shared_ptr<HardwareConstants>);

	// ******* stream spikeyconfig *******
	friend ostream& operator<<(ostream& o, const SpikeyConfig& cfg);
	friend istream& operator>>(istream& i, SpikeyConfig& cfg);
};
// ****** end of SpikeyConfig class declaration ********

// returns gslices to access a row or a col of synapses
inline std::valarray<ubyte> SpikeyConfig::col(uint block, uint col) const
{
	return weight[slice(block * num_presyns * num_neurons + col, num_presyns, num_neurons)];
}
inline slice_array<ubyte> SpikeyConfig::col(uint block, uint col)
{
	return weight[slice(block * num_presyns * num_neurons + col, num_presyns, num_neurons)];
}

inline std::valarray<ubyte> SpikeyConfig::row(uint block, uint row) const
{
	return weight[slice(block * num_presyns * num_neurons + row * num_neurons, num_neurons, 1)];
}
inline slice_array<ubyte> SpikeyConfig::row(uint block, uint row)
{
	return weight[slice(block * num_presyns * num_neurons + row * num_neurons, num_neurons, 1)];
}
inline ubyte& SpikeyConfig::getWeight(uint block, uint row, uint col)
{
	return weight[block * num_presyns * num_neurons + row * num_neurons + col];
}

// ******* spike train *******
//! Container class for event input/output data to be sent to/received from the Spikey neuromorphic
//system.
class SpikeTrain
{
protected:
	::Logger& dbg;

public:
	SpikeTrain() : dbg(::Logger::instance()) {}

	SpikeTrain(const SpikeTrain& other) : dbg(other.dbg), d(other.d), mem(other.mem) {}

	void operator=(const SpikeTrain& other)
	{
		d = other.d;
		mem = other.mem;
	}

	vector<IData> d;
	MemObj mem;
	// methods
	void getNeuronList(vector<uint>& n) const; // get sorted list of neurons in spike train
	bool writeToFile(string filename) const;
	/** writes the SpikeTrain data in a NeuroTools compatible format */
	void writeNeuroToolsFile(string filename) const;
	bool readFromFile(string filename);
};

// ******* stream spiketrain ********
ostream& operator<<(ostream& o, const SpikeTrain& st);

} // end of namespace spikey2

