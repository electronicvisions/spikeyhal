// spikey is the main hardware abstaction class

// ******** user-level spikenet class ********
// no MemObj can be stored at the moment
// so the MemObj field in the input data is ignored (assumed 'invalid')
// and only 'invalid' or 'ok' is returned

#include <iostream>
#include <cassert>

// forward declarations required outside namespaces!
class PySpikey;
class PySpikeyTM;
class FacetsHWS1V2Access;
class TMSpikeyClass;
class TMSpikeyClassAG;
class TMEarlyDebug;

namespace spikey2
{


//! Main spikey interface
class Spikey : public Spikenet
{
	friend class ::TMSpikeyClass;
	friend class ::TMSpikeyClassAG;
	friend class ::TMEarlyDebug;
	friend class ::FacetsHWS1V2Access;
	friend class ::PySpikey;
	friend class ::PySpikeyTM;
	friend class SpikeyCalibratable;
	friend class SpikeyVoutCalib;

private:
	float clockper; // main spikey core clock period (5 to 10ns)
	uint voutperiod; // was 18-> 5+3*(period-1) + 2*((period-1)>>2)->67
	uint lutboost; // (19-1)=18: in 5+3*20+2*5, pramtime is 1<<lutboost + 3: 35, !67!, 131, 259
	static const float voutslope; // 1.54/(1.958*150); //Volt per microamp*nanosecs

	uint synramdel, synfstdel; // delay for synapse ram access

	// bitset<cr_width> statusreg;
	vector<bool> statusreg;
	bool dllreset, neuronreset, pccont; // CI Spikenet control/status reg
	boost::shared_ptr<SpikeyConfig> actcfg;

	// returns mean values for vout0 and vout4, rms error, number of tries (-1 if unsucessfull) in
	// vector<float>
	vector<float> calibParam(float precision, // required precision
	                         uint nadc = 2, // number of adc samples taken per measurement
	                         uint nmes = 100, // number of measurements
	                         uint ndist = 100, // number of pram writes to disturb pram logic
	                         uint maxnmes = 2000); // maximum number of tries for precision
	virtual void loadParam(boost::shared_ptr<SpikeyConfig>, vector<PramData>&);
	virtual uint convVoltDac(double voltage);
	double convDacVolt(uint dac);
	uint convCurDac(double current);
	double convDacCur(uint dac);

	uint shiftRamData(uint nidx, uint ramdatanibble); // shift ramdata nibbles to correct position
	                                                  // for neuron number nidx

	double dacimax; // max output current of dac
	double rconv100; // corner min
	vector<LutData> lut;

	bool pram_settled; // stores, whether analog parameter memories can be assumed as settled after
	                   // a chip reset

	static constexpr float tempMax = 55.0; //!< max temperature; for higher temperature
	                                       //communication links may become unstable (see issue
	                                       //#1418)


public:
	using Spikenet::bus;
	using Spikenet::readAdc;

	//! transfer ci spikenet control register to spikey
	void writeSCtl(uint del = 0) { getSC()->write_ctrl(dllreset, neuronreset, pccont, del); };
	//! transfer CC control register to spikey
	void writeCC(uint del = 0);
	//! set/reset particular chip control bit. No change to actual chip register until writeCC is
	//issued!
	void setCCBit(uint pos, bool value);
	uint64_t statusregToUint64(void);

	Spikey(boost::shared_ptr<Spikenet> spnet, float clockper = 10.0, uint chipid = 0);
	Spikey(boost::shared_ptr<SpikenetComm> comm, float clockper = 10.0, uint chipid = 0);
	virtual ~Spikey(){};
	float getClkPer() { return clockper; };

	void initialize();

	//! enables/disables the connection of the experiment trigger signal to the global trigger line
	//of the backplane
	void setGlobalTrigger(bool value);

	// loads values into spikey according to update flags and MemObj
	// confdata must be valid until Spikey is destroyed or new reference is passed !!!
	MemObj config(boost::shared_ptr<SpikeyConfig> confdata);

	// transmit spiketrain 'st' and allocate nec. memory
	// If dropmod is false, events that can't be transmitted due to bandwidth limitations are
	// dropped
	// and stored in et. If it is true, those events' time stamps will be increased to hopefully
	// transfer them.
	// In this case, !!! something has to be done with the modified events!!! ;-)
	MemObj sendSpikeTrain(const SpikeTrain& st, SpikeTrain* et = NULL, bool dropmod = false);
	void replayPB();

	// (sf) wait until playback is idle again an update pointers
	// not needed when playbackMode == standard
	void waitPbFinished();

	// the last transmitted spiketrain (st.state==invalid) or (st.adr) is send and the received data
	// collected in 'st'
	MemObj recSpikeTrain(SpikeTrain& st, bool nonblocking = true);


	void setFifoDepth(int depth, int delay = 4);
	void setEvOut(bool value, int delay);
	void setEvIn(bool value, int delay);
	void setEvInb(bool value, int delay);
};

// ***** Spikey inline method definitions *****

// for further information about the following conversion methods,
// please refer to: Andreas Hartel, Diploma thesis, 2010

inline double Spikey::convDacCur(uint dac)
{
	if (dac >= 1024)
		dac = 1023;
	double current = dac / 1024.0 * dacimax;
	return current;
}

inline double Spikey::convDacVolt(uint dac)
{
	double voltage = convDacCur(dac) * rconv100;
	return voltage;
}

// mapping in analog_chip: [lu,lm,ll,ru,rm,rl]
inline uint Spikey::shiftRamData(uint nidx, uint data)
{
	data &= 0xf; // only 4 bit per prienc block
	uint shift;
	if (nidx >= SpikeyConfig::num_neurons) {
		shift = 0;
		nidx -= SpikeyConfig::num_neurons;
	} else
		shift = 12;
	shift += (nidx & 0xc0) >> 4; // results in 0,4,8
	return data << shift;
}

} // end of namespace spikey2
