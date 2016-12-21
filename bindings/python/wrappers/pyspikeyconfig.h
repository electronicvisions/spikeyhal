#define PY_ARRAY_UNIQUE_SYMBOL hal

#include <boost/python.hpp>


using namespace spikey2;


//! Python wrapper class for the C++ class SpikeyConfig. Container class for all configuration
//parameter values of a Spikey chip.
/*! Holds values for all parameters that define an initial configuration state of a Spikey chip,
  e.g. neuron voltages, synapse driver configuration, synaptic weights, hardware clock settings
  etc...*/
class PySpikeyConfig : public SpikeyConfig
{
public:
	PySpikeyConfig();
	void initialize(std::vector<double> stdParams);
	bool readConfigFile(std::string filename);
	bool writeConfigFile(std::string filename);
	void enableNeuron(int neuronIndex, bool value);
	void enableMembraneMonitor(int neuronIndex, bool value);
	bool membraneMonitorEnabled(int neuronIndex);
	void setSynapseDriver(int driverIndex, int sourceType, int source, int type, float drviout,
	                      float drvifall, float drvirise, float adjdel);
	void setWeights(int neuronIndex, std::vector<ubyte> weight);
	void setVoltages(std::vector<std::vector<double>> voltages);
	void setVoltageBiases(std::vector<std::vector<double>> voltages);
	void setBiasbs(std::vector<float> biasbs);
	void setILeak(int neuronIndex, float value);
	void setIcb(int neuronIndex, float value);
	void setSTDPParams(float vm, int tpcsec, int tpcorperiod, float vclra, float vclrc,
	                   float vcthigh, float vctlow, float adjdel);
	float getILeak(int neuronIndex);
	float getIcb(int neuronIndex);
	void clearConfig();
	void clearColConfig();
	void clearRowConfig();
	void clearParams();
	void clearWeights();
	vector<int> weight_wrapper();

	static const uint maxDiscreteWeight;

private:
	string printIndentation;
	// default parameter value possible/allowed (might be larger than zero)
	float defaultParamValue;
	vector<bool> synapseConfigured;
};
