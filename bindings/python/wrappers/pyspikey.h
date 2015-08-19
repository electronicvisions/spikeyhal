#define PY_ARRAY_UNIQUE_SYMBOL hal

#include <boost/python.hpp>


using namespace spikey2;


//! Python wrapper class for the C++ class Spikey. Encapsulates the main interface functions to the
//so-called Spikey chip.
/*! Encapsulates the main interface functions to the so-called Spikey chip.
  The most important functions for operation of the chip are config(), sendSpikeTrain() and
  receiveSpikeTrain().*/
class PySpikey : public SpikeyCalibratable
{

public:
	PySpikey(boost::shared_ptr<PySC_Mem> comm, float clockper = 10.0, uint chipid = 0,
	         uint spikeyNr = 0, string calibfile = "spikeycalib.xml");

	//! playback memory reset
	void clearPlaybackMem();
	//! transmit spiketrain 'st' and allocate nec. memory
	void sendSpikeTrain(const PySpikeTrain& st, PySpikeTrain* et = NULL);
	//! replays spiketrain
	void resendSpikeTrain();
	//! the last transmitted spiketrain (st.state==invalid) or (st.adr) is sent and the received
	//data collected in 'st'
	void recSpikeTrain(PySpikeTrain& st);
	/*! loads values into spikey according to update flags
	   confdata must be valid until Spikey is destroyed or new reference is passed */
	void config(boost::shared_ptr<PySpikeyConfig> cfg, bool updateChip, bool updateDAC,
	            bool updateParam, bool updateRowConf, bool updateColConf, bool updateWeight);

	//! connects the analog membrane potential trace on pin 'membranePin' (0..7) to the readout pin
	//(for oscilloscope)
	void assignMembranePin2TestPin(uint membranePin);

	//! connects the voltage 'signal' to the readout pin (for oscilloscope)
	/*! signal is one of
	enum vout {Ei0=0,Ei1=1, //inhibitory reversal potential
	                    El0=2,El1=3,	//leakage reversal potential
	                    Er0=4,Er1=5,	//reset potential
	                    Ex0=6,Ex1=7,	//excitatory reversal potential
	                    Vclra=8,			//storage clear bias synapse array acausal (higher
	bias->smaller amount stored on cap)
	                    Vclrc=9,			//dito, causal
	                    Vcthigh=10,		//correlation threshold high
	                    Vctlow=11,		//correlation threshold low
	                    Vfac0=12,Vfac1=13, 	//short term facilitation reference voltage
	                    Vstdf0=14,Vstdf1=15,//short term capacitor high potential
	                    Vt0=16,Vt1=17,//neuron threshold voltage
	                    Vcasneuron=18,//neuron input cascode gate voltage
	                    Vresetdll=19,	//dll reset voltage
	                    aro_dllvctrl=20, 	//dll control voltage readout (only spikey v2)
	                    aro_pre1b=21,			//spike input buf 1 presyn (only spikey v2)
	                    aro_selout1hb=22,	//spike input buf 1 selout (only spikey v2)
	                    probepad=31,	//left vout voltage buffer block of spikey  (only spikey v1)
	                    num_vout_adj=Vresetdll+1, //number of adjustable vout buffers !*/
	void assignVoltage2TestPin(uint signal, uint block = 0);

	void assign2TestPin(int membrane, int signal, int block);

	void assignMultipleVoltages2IBTest(std::vector<int> vouts, int leftBlock, int rightBlock);

	//! get temperature of sensor between Flyspi and Spikey board
	float getTemp();
	//! initializes autocalibration and writes calib data into calibration file
	void autocalib(boost::shared_ptr<PySpikeyConfig> cfg = boost::shared_ptr<PySpikeyConfig>(),
	               string filenamePlot = "");
	//! calibration routine, workaround of Spikey_v1 bug, good choice for f is 0.002
	void calibParam(float f);

	//! set LUT (acausal [0..15], causal[16..31])
	void setLUT(std::vector<int> lut);
	std::vector<int> getLUT();
	//! set parameters for continuous STDP
	void setSTDPParamsCont(bool activate, uint first, uint distance, uint distance_pre,
	                       uint startrow, uint stoprow);
	//! like sendSpikeTrain(...), but with STDP enabled (changeWeights == true to change Weights)
	/*!
	 *
	 * \param st SpikeTrain to send
	 * \param minRow read row >= minRow
	 * \param maxRow read row <= maxRow
	 * \param minCol read col >= minCol
	 * \param maxCol read col <= maxCol
	 * \param changeWeights change weights after correlation
	 * \param verbose more output
	 */
	void sendSpikeTrainWithSTDP(const PySpikeTrain& st,     // SpikeTrain to send
	                            PySpikeTrain& stout,        // ErrorSpikeTrain to "return"
	                            PySpikeTrain* et = NULL,    // ErrorSpikeTrain to "return"
	                            uint minRow = 0,            // window
	                            uint maxRow = 255,          //     to
	                            uint minCol = 0,            //      look
	                            uint maxCol = 255,          //         at ;)
	                            bool changeWeights = false, // if false, weights aren't changed
	                            bool verbose = false);      // printing matrix entries

	//! benching drift times
	void benchmarkSTDPDrift();
	//! executes a synapse ram test
	void checkSynRam(uint maxrows, uint maxcols, uint loops);
	//! get back correlation information most recently measured at a specific synapse
	int correlationInformation(int neuron, int synapse)
	{
		return _correlationInformation[neuron][synapse];
	}
	//! flush the content of the playback memory to the chip
	void flushPlaybackMemory() { Spikenet::Flush(); };

	//! return a single correlation flag
	int getCorrFlag(uint row, uint col) { return _correlationInformation[255 - row][col]; }
	//! return a vector of vectors of correlation flags
	std::vector<std::vector<int>> getCorrFlags(uint minRow = 0, uint maxRow = 255, uint minCol = 0,
	                                           uint maxCol = 384);

	//! return a single synapse weight
	int getSynapseWeight(uint row, uint col) { return _synapseWeights[255 - row][col]; }
	//! return a vector of vectors of synapse weights
	std::vector<std::vector<int>> getSynapseWeights(uint minRow = 0, uint maxRow = 255,
	                                                uint minCol = 0, uint maxCol = 384);

	//! returns AD converted vout-voltages (2 * numVouts)
	std::vector<std::vector<double>> getVoltages();
	std::vector<double> getVout(uint blk, uint vout, uint count);
	std::vector<double> getIbCurr(uint count);

	//! calls PramControl::write_lut() (q'nd wrap ;))
	void write_lut(uint cadr, uint normal, uint boost, uint lutrepeat, uint lutstep)
	{
		printf("PySpikey::write_lut(%2d, %2d, %2d, %2d, %2d)\n", cadr, normal, boost, lutrepeat,
		       lutstep);
		getPC()->write_lut(cadr, normal, boost, lutrepeat, lutstep);

		/* debugging: */
		vector<LutData> lut = getPC()->get_lut();
		for (uint i = 0; i < lut.size(); ++i) {
			printf("LUT%d %d %d %d %d\n", i, lut.at(i).luttime, lut.at(i).lutboost,
			       lut.at(i).lutrepeat, lut.at(i).lutstep);
		}
	}

	//! returns the revision number of the currently used Spikey chip
	int revision() { return hw_const->revision(); };

	//! Interrupts possible remaining activity from previous experiments by temporariliy setting all
	//synaptic weights to zero
	void interruptActivity(boost::shared_ptr<PySpikeyConfig> original_cfg);

	//! setup fast USB ADC
	void setupFastAdc(float simtime);

	//! read voltage data from USB ADC
	boost::python::object readFastAdc();

	//! triggers fast ADC manually
	void triggerAdc();

private:
	std::vector<std::vector<int>> _correlationInformation;
	std::vector<std::vector<int>> _synapseWeights;
	std::vector<std::vector<int>> _synapseWeightsOld;
	std::vector<std::vector<int>> _synapseWeightsPreLUT;
	bool _lastRunWasSTDP;
	string _printIndentation;
	std::vector<std::vector<int>> _corrFlagsForPython;
	std::vector<std::vector<int>> _weightsForPython;
	std::vector<std::vector<double>> _voltagesForPython;
	std::vector<double> _ibtestValuesForPython;
	valarray<ubyte> zero_weight_array;

	/* stdp specific stuff */
	struct synapseRect {
		uint rowMin;
		uint rowMax;
		uint colMin;
		uint colMax;
	};
	struct timings {
		uint clockper, tsense, tpcsec, tdel, tpcorperiod;

		timings()
		    :               // time constants (konservativst!)   old vals | from tmjs_spikeycore
		      clockper(10), //      nanosecs
		      tsense(100 / clockper),    // 100ns    | 100ns // was 300/clockper
		      tpcsec(20 / clockper),     //  20ns    |  20ns // was 60/clockper
		      tdel(6 + tsense + tpcsec), //          | 126ns
		      tpcorperiod(2 * tdel)      //          | 252ns, 2*tdel // was 360
		{
		}

		/* increasing everything by a factor of 10 */
		// tsense *= 10;
		// tpcsec *= 10;
		// tdel *= 10;
		// tpcorperiod *= 10;

	} timings;


	boost::shared_ptr<SynapseControl> synapse_control;
	void processCorrFlags(synapseRect);
	void readCorrFlags(synapseRect);
	void readWeights(synapseRect);
	void receiveCorrFlags(synapseRect, std::vector<std::vector<int>>&);
	void receiveWeights(synapseRect, std::vector<std::vector<int>>&);
};


template <typename T>
T highbit(T& t)
{
	return t = (((T)(-1)) >> 1) + 1;
}

template <typename T>
std::ostream& bin(T& value, std::ostream& o, uint blocksize = 4)
{
	uint count = 0;
	for (T bit = highbit(bit); bit; bit >>= 1, count++) {
		o << ((value & bit) ? '1' : '0');
		if ((count + 1) % blocksize == 0)
			o << ' ';
	}
	return o;
}
