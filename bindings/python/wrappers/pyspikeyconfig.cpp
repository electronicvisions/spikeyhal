#define PY_ARRAY_UNIQUE_SYMBOL hal
#define NO_IMPORT_ARRAY


#include "spikey2_lowlevel_includes.h"
#include "pyspikeyconfig.h"

static log4cxx::LoggerPtr logger = log4cxx::Logger::getLogger("HAL.PyC");

// shortcut namespace
namespace py = boost::python;

using namespace spikey2;

const uint PySpikeyConfig::maxDiscreteWeight = 15;

PySpikeyConfig::PySpikeyConfig() : SpikeyConfig::SpikeyConfig()
{
	// for nicer stdout messages
	printIndentation = "        ";
	synapseConfigured.resize(SpikeyConfig::num_presyns * SpikeyConfig::num_blocks, false);
}


void PySpikeyConfig::initialize(vector<double> stdParams)
{
	SpikeyConfig::setValid((SpikeyConfig::SCupdate)(SpikeyConfig::ud_all), true);
	double buffer;
	vector<double>::iterator itParams = stdParams.begin();

	buffer = *(itParams);
	itParams++;
	this->tsense = static_cast<int>(buffer);
	buffer = *(itParams);
	itParams++;
	this->tpcsec = static_cast<int>(buffer);
	buffer = *(itParams);
	itParams++;
	this->tpcorperiod = static_cast<int>(buffer);
	buffer = *(itParams);
	itParams++;
	this->irefdac = static_cast<int>(buffer);

	this->vcasdac = *(itParams);
	itParams++;
	this->vm = *(itParams);
	itParams++;
	this->vstart = *(itParams);
	itParams++;
	this->vrest = *(itParams);
	itParams++;

	for (uint s = 0; s < (SpikeyConfig::num_blocks * SpikeyConfig::num_presyns); ++s)
		this->synapse[s].adjdel = *(itParams);
	itParams++;
	for (uint n = 0; n < (SpikeyConfig::num_blocks * SpikeyConfig::num_neurons); ++n)
		this->neuron[n].icb = *(itParams);
	itParams++;
	for (uint n = 0; n < PramControl::num_biasb; ++n) {
		this->biasb[n] = *(itParams);
		itParams++;
	}
	for (uint n = 0; n < PramControl::num_outamp; ++n) {
		this->outamp[n] = *(itParams);
		itParams++;
	}
	for (uint b = 0; b < SpikeyConfig::num_blocks; ++b)
		for (uint n = 0; n < hw_const->ar_numvouts(); ++n)
			this->voutbias[b * hw_const->ar_numvouts() + n] = *(itParams);
	itParams++;
	for (uint b = 0; b < SpikeyConfig::num_blocks; ++b) {
		this->probepad[b] = *(itParams);
		this->probebias[b] = *(itParams + 1);
	}
	itParams++;
	itParams++;
	this->defaultParamValue = *(itParams);
	// cout << "default parameter for SpikeyConfig currents: " << this->defaultParamValue << " uA"
	// << endl;
}


bool PySpikeyConfig::readConfigFile(std::string filename)
{
	if (!SpikeyConfig::readParam(filename)) {
		cout << printIndentation << "ERROR: Reading of file " << filename << " failed" << endl;
		return false;
	} else {
#ifdef PYHAL_VERBOSE
		cout << printIndentation << "INFO: Reading of file " << filename << " successful" << endl;
#endif
		return true;
	}
}


bool PySpikeyConfig::writeConfigFile(std::string filename)
{
	if (!SpikeyConfig::writeParam(filename)) {
		cout << printIndentation << "ERROR: Writing to file " << filename << " failed" << endl;
		return false;
	} else {
#ifdef PYHAL_VERBOSE
		cout << printIndentation << "INFO: Writing to file " << filename << " successful" << endl;
#endif
		return true;
	}
}


void PySpikeyConfig::setSynapseDriver(int driverIndex, int sourceType, int source, int type,
                                      float drviout, float drvifall, float drvirise, float adjdel)
{
	// cout << "configuring synapse driver " << driverIndex << " to be type " << type << " with
	// drviout " << drviout << ", old drviout was " << synapse[driverIndex].drviout << endl;

	// Is a complete new row configuation provided?
	bool isCompleteRowconf = (sourceType != -1) && (source != -1) && (type != -1);
	// Throw exception in case of incomplete (but non-empty) rowconf data

	if ((!(isCompleteRowconf)) && ( (sourceType != -1) || (source != -1) || (type != -1) ) )  {
		PyErr_SetString(PyExc_TypeError, "Incomplete row configuration given! "
		                                  "From (sourceType, source, type) provide either all or none.");
		py::throw_error_already_set();
	}

	// Is this a complete call (i.e. no default values are used)?
	bool isCompleteData = isCompleteRowconf &&
	                      (!std::isnan(drviout)) && (!std::isnan(drvifall)) &&
	                      (!std::isnan(drvirise)) && (!std::isnan(adjdel));

	// check if this is the first time this synapse driver is configured...
	bool firstConfiguration = !(synapseConfigured[driverIndex]);

	// this bitset is needed by SpikeyConfig::synapse
	bitset<SpikeyConfig::num_sc> newrowconf, oldrowconf;

	// get old configuration
	oldrowconf = synapse[driverIndex].config;

	// fill connectivity bitset (only if isCompleteRowconf)
	if (isCompleteRowconf) {
		newrowconf[0] = (sourceType == 0); // sourceType == 1: external input ----- souceType == 0:
		                                   // internal feedback
		newrowconf[1] =
				(sourceType == 0
						? (driverIndex / SpikeyConfig::num_presyns == source / SpikeyConfig::num_neurons)
						: false); // in case of feedback: from same or adjacent block)
		newrowconf[2] = static_cast<bool>(type & 4);   // excitatory
		newrowconf[3] = static_cast<bool>(type & 8);   // inhibitory
		newrowconf[4] = static_cast<bool>(type & 16);  // enable STP
		newrowconf[5] = static_cast<bool>(type & 32);  // fac or dep
		newrowconf[6] = static_cast<bool>(type & 64);  // cap2
		newrowconf[7] = static_cast<bool>(type & 128); // cap4
		// 4: enable short term depression / facilitation
		// 5: depression (1) / fac (0)
		// 6: enable cap2 (+= 2/8) => 1/8 or 3/8 ? (please CHECK!)
		// 7: enable cap4 (+= 4/8) => 5/8 or 7/8 ? (please CHECK!)

		// cout << ">> C++: " << newrowconf << endl;
		// if (newrowconf[2] && newrowconf[3])
		//   newrowconf[2] = false;
	}
	else {
		newrowconf = oldrowconf;
	}

	// Clip all current values to valid range (if given)
	if (!std::isnan(drviout)) {
		if (drviout > 2.5) {
			drviout = 2.5;
			LOG4CXX_WARN(logger, "Drviout outside range: clipped to 2.5muA");
		}
		if (drviout < 0) {
			drviout = 0;
			LOG4CXX_WARN(logger, "Drviout outside range: clipped to 0muA");
		}
	}
	if (!std::isnan(drvifall)) {
		if (drvifall > 2.5) {
			drvifall = 2.5;
			LOG4CXX_WARN(logger, "Drvifall outside range: clipped to 2.5muA");
		}
		if (drvifall < 0) {
			drvifall = 0;
			LOG4CXX_WARN(logger, "Drvifall outside range: clipped to 0muA");
		}
	}
	if (!std::isnan(drvirise)) {
		if (drvirise > 2.5) {
			drvirise = 2.5;
			LOG4CXX_WARN(logger, "Drvirise outside range: clipped to 2.5muA");
		}
		if (drvirise < 0) {
			drvirise = 0;
			LOG4CXX_WARN(logger, "Drvirise outside range: clipped to 0muA");
		}
	}
	if (!std::isnan(adjdel)) {
		if (adjdel > 2.5) {
			adjdel = 2.5;
			LOG4CXX_WARN(logger, "Adjdel outside range: clipped to 2.5muA");
		}
		if (adjdel < 0) {
			adjdel = 0;
			LOG4CXX_WARN(logger, "Adjdel outside range: clipped to 0muA");
		}
	}

	// Set the driver
	if (firstConfiguration) {
		// Only accept complete function arguments during first call.
		if (!(isCompleteData)) {
				PyErr_SetString(PyExc_TypeError, "Incomplete synapse driver configuation during first "
				                                 "configuration of this driver! Provide full information "
				                                 "for fist configuration.");
				py::throw_error_already_set();
		}
		synapse[driverIndex].config = newrowconf;

		// fill current parameters
		synapse[driverIndex].drviout = drviout;
		synapse[driverIndex].drvifall = drvifall;
		synapse[driverIndex].drvirise = drvirise;
		synapse[driverIndex].adjdel = adjdel;
		synapseConfigured[driverIndex] = true;
	} else {
		// else: change already initialized driver
		if ((!newrowconf[2]) && !(newrowconf[3])) {
			// Do nothing, because this synapse is neither excitatory nor inhibitory (weight=zero),
			// i.e. the driver shall not be manipulated at all.
			// This is important in order to avoid overriding valid synapse driver configurations
			// corresponding to non-zero synapses in the same row!
		} else {
			// check for ambiguous configuration
			if ( (newrowconf[2] && newrowconf[3]) || (oldrowconf[2] && newrowconf[3]) || (oldrowconf[3] && newrowconf[2]) ) {
				PyErr_SetString(PyExc_TypeError, "Ambiguous synapse driver configuration! Can't be "
				                                 "excitatory and inhibitory at the same time!");
				py::throw_error_already_set();
			}
			if (newrowconf[0] != oldrowconf[0]) {
				PyErr_SetString(PyExc_TypeError, "Ambiguous synapse driver configuration! Source "
				                                 "can't be external and feedback at the same "
				                                 "time!");
				py::throw_error_already_set();
			}
			if (newrowconf[1] != oldrowconf[1]) {
				PyErr_SetString(PyExc_TypeError, "Ambiguous synapse driver configuration! Feedback "
				                                 "can't be from both blocks at the same time!");
				py::throw_error_already_set();
			}
			if (newrowconf[4] != oldrowconf[4]) {
				PyErr_SetString(PyExc_TypeError, "Ambiguous synapse driver configuration! Short "
				                                 "term plasticity can't be turned ON and OFF at "
				                                 "the same time! Check, whether all inputs of this "
				                                 "synapse driver have the same short term "
				                                 "plasticity.");
				py::throw_error_already_set();
			}
			if (newrowconf[5] != oldrowconf[5]) {
				PyErr_SetString(PyExc_TypeError, "Ambiguous synapse driver configuration! Synapse "
				                                 "can't be depressing and facilitating at the same "
				                                 "time!");
				py::throw_error_already_set();
			}
			if ((newrowconf[6] != oldrowconf[6]) || (newrowconf[7] != oldrowconf[7])) {
				PyErr_SetString(PyExc_TypeError, "Ambiguous synapse driver configuration! Driver "
				                                 "only supports one STP-configuration for C_2 at "
				                                 "the same time!");
				py::throw_error_already_set();
			}
			this->synapse[driverIndex].config = newrowconf;
			// fill current parameters (each, if given)
			if (!std::isnan(drviout))
				synapse[driverIndex].drviout = drviout;
			if (!std::isnan(drvifall))
				synapse[driverIndex].drvifall = drvifall;
			if (!std::isnan(drvirise))
				synapse[driverIndex].drvirise = drvirise;
			if (!std::isnan(adjdel))
				synapse[driverIndex].adjdel = adjdel;
		}
	}
}


boost::python::tuple PySpikeyConfig::getSynapseDriver(int driverIndex)
{
	if (synapseConfigured[driverIndex] == false) {
		PyErr_SetString(PyExc_AttributeError, "Can not read parameters before initialization."
		                                      "Use setSynapseDriver() for first configuration.");
		py::throw_error_already_set();
	}
	return boost::python::make_tuple(synapse[driverIndex].drviout,
	                                 synapse[driverIndex].drvifall,
	                                 synapse[driverIndex].drvirise,
	                                 synapse[driverIndex].adjdel
	);
}


void PySpikeyConfig::enableNeuron(int neuronIndex, bool value)
{
	this->neuron[neuronIndex].config[2] = value;
	// if(value) cout << "enabling recording of neuron " << neuronIndex << endl;
}


void PySpikeyConfig::enableMembraneMonitor(int neuronIndex, bool value)
{
	this->neuron[neuronIndex].config[1] = value;
}


bool PySpikeyConfig::membraneMonitorEnabled(int neuronIndex)
{
	return this->neuron[neuronIndex].config[1];
}


void PySpikeyConfig::setWeights(int neuronIndex, vector<ubyte> weight)
{
	if (weight.size() != SpikeyConfig::num_presyns) {
		ostringstream s;
		s << "weight array not correct type:\nmust be 1-d with length " << SpikeyConfig::num_presyns
		  << "." << endl;
		PyErr_SetString(PyExc_TypeError, s.str().c_str());
		py::throw_error_already_set();
	}

	uint block = neuronIndex / SpikeyConfig::num_neurons;
	uint column = neuronIndex % SpikeyConfig::num_neurons;
	// run through all synapses
	for (uint row = 0; row < SpikeyConfig::num_presyns; ++row)
		this->getWeight(block, row, column) = weight[row];
}


void PySpikeyConfig::setVoltages(vector<vector<double>> voltages)
{
	for (uint b = 0; b < SpikeyConfig::num_blocks; ++b) {
		// cout << "b = " << b << endl;
		uint offset = b * hw_const->ar_numvouts();
		for (uint n = 0; n < hw_const->ar_numvouts(); ++n) {
			// cout << "n = " << n << endl;
			this->vout[offset + n] = voltages[b][n];
			// cout << "got vout value " << buffer << endl;
		}
	}
}


void PySpikeyConfig::setVoltageBiases(vector<vector<double>> voltagebiases)
{
	for (uint b = 0; b < SpikeyConfig::num_blocks; ++b) {
		// cout << "b = " << b << endl;
		uint offset = b * hw_const->ar_numvouts();
		for (uint n = 0; n < hw_const->ar_numvouts(); ++n) {
			// cout << "n = " << n << endl;
			this->voutbias[offset + n] = voltagebiases[b][n];
			// cout << "got vout value " << buffer << endl;
		}
	}
}

void PySpikeyConfig::setBiasbs(vector<float> biasbs)
{
	for (uint n = 0; n < PramControl::num_biasb; ++n) {
		this->biasb[n] = biasbs[n];
		// cout << "biasb "<< n << " set to " << biasbs[n] << endl;
	}
}


void PySpikeyConfig::setILeak(int neuronIndex, float value)
{
	// cout << "setting iLeak to " << value << endl;
	this->neuron[neuronIndex].ileak = value;
}


float PySpikeyConfig::getILeak(int neuronIndex)
{
  return this->neuron[neuronIndex].ileak;
}


void PySpikeyConfig::setIcb(int neuronIndex, float value)
{
	// cout << "setting iLeak to " << value << endl;
	this->neuron[neuronIndex].icb = value;
}


float PySpikeyConfig::getIcb(int neuronIndex)
{
  return this->neuron[neuronIndex].icb;
}


void PySpikeyConfig::setSTDPParams(float vm, int tpcsec, int tpcorperiod, float vclra, float vclrc,
                                   float vcthigh, float vctlow, float adjdel)
{
	static_cast<void>(vctlow);
	this->vm = vm;
	this->tpcsec = tpcsec;
	this->tpcorperiod = tpcorperiod;

	for (uint b = 0; b < SpikeyConfig::num_blocks; ++b) {
		// cout << "b = " << b << endl;
		uint offset = b * hw_const->ar_numvouts();
		this->vout[offset + 8] = vclra;
		this->vout[offset + 9] = vclrc;
		this->vout[offset + 10] = vcthigh;
	}

	for (uint s = 0; s < (SpikeyConfig::num_blocks * SpikeyConfig::num_presyns); ++s)
		this->synapse[s].adjdel = adjdel;
}


void PySpikeyConfig::clearConfig()
{
#ifdef PYHAL_VERBOSE
	cout << printIndentation << "clearing SpikeyConfig object... " << endl;
#endif
	// clear neuron parameters
	clearColConfig();

	// clear synapse driver parameters
	clearRowConfig();

	// clear voltages and currents
	clearParams();

	// clear weights
	clearWeights();
#ifdef PYHAL_VERBOSE
	cout << printIndentation << "clearing SpikeyConfig object done." << endl;
#endif
}


void PySpikeyConfig::clearColConfig()
{
	// clear neuron parameters
	bitset<SpikeyConfig::num_nc> colconf;
	colconf[0] = false;
	colconf[1] = false;
	colconf[2] = false;
	colconf[3] = false;
	for (uint b = 0; b < SpikeyConfig::num_blocks; ++b)
		for (uint n = 0; n < SpikeyConfig::num_neurons; ++n)
			this->neuron[b * SpikeyConfig::num_neurons + n].config = colconf;
#ifdef PYHAL_VERBOSE
	cout << printIndentation
	     << "clearing column (= neuron) configuration in SpikeyConfig object done." << endl;
#endif
}


void PySpikeyConfig::clearParams()
{
	// clear synapse parameters
	for (uint b = 0; b < SpikeyConfig::num_blocks; ++b) {
		for (uint v = 0; v < SpikeyConfig::num_presyns; ++v) {
			this->synapseConfigured[b * SpikeyConfig::num_presyns + v] = false;
			this->synapse[b * SpikeyConfig::num_presyns + v].drviout = this->defaultParamValue;
			this->synapse[b * SpikeyConfig::num_presyns + v].drvifall = this->defaultParamValue;
			this->synapse[b * SpikeyConfig::num_presyns + v].drvirise = this->defaultParamValue;
		}
	}

	// clear voltages
	for (uint n = 0; n < hw_const->ar_numvouts(); ++n)
		for (uint b = 0; b < SpikeyConfig::num_blocks; ++b)
			this->vout[b * hw_const->ar_numvouts() + n] = this->defaultParamValue;

	// clear neuron iLeaks
	for (uint n = 0; n < SpikeyConfig::num_neurons; ++n)
		for (uint b = 0; b < SpikeyConfig::num_blocks; ++b)
			this->neuron[b * SpikeyConfig::num_neurons + n].ileak = this->defaultParamValue;
#ifdef PYHAL_VERBOSE
	cout << printIndentation
	     << "clearing programmable voltages and currents in SpikeyConfig object done." << endl;
#endif
}


void PySpikeyConfig::clearWeights()
{
	ubyte discreteWeight = 0;
	valarray<ubyte> syn(SpikeyConfig::num_presyns);
	for (uint i = 0; i < SpikeyConfig::num_presyns; ++i)
		syn[i] = discreteWeight;

	for (uint b = 0; b < SpikeyConfig::num_blocks; ++b)
		for (uint n = 0; n < SpikeyConfig::num_neurons; ++n)
			this->col(b, n) = syn;
#ifdef PYHAL_VERBOSE
	cout << printIndentation << "clearing weights in SpikeyConfig object done." << endl;
#endif
}


void PySpikeyConfig::clearRowConfig()
{
	bitset<SpikeyConfig::num_sc> rowconfE;
	bitset<SpikeyConfig::num_sc> rowconfI;
	for (uint j = 0; j < SpikeyConfig::num_sc; ++j) {
		rowconfE[j] = false;
		rowconfI[j] = false;
	}
	rowconfE[2] = true;
	rowconfI[3] = true;

	// this is a trick to minimize configuration-specific efficacy fluctuations
	// for the synapse drivers: one half of all unused syndrivers is set excitatory,
	// the other half inhibitory
	for (uint b = 0; b < SpikeyConfig::num_blocks; ++b)
		for (uint i = 0; i < SpikeyConfig::num_presyns; ++i)
			if (i % 2 == 0)
				this->synapse[i + b * SpikeyConfig::num_presyns].config = rowconfE;
			else
				this->synapse[i + b * SpikeyConfig::num_presyns].config = rowconfI;
#ifdef PYHAL_VERBOSE
	cout << printIndentation
	     << "clearing row ( = synapse driver) configuration in SpikeyConfig object done." << endl;
#endif
}


vector<int> PySpikeyConfig::weight_wrapper()
{
	valarray<ubyte> w = weight;
	vector<int> result(w.size());
	// put all the strings inside the python list
	for (int i = 0; i < w.size(); ++i) {
		result[i] = int(w[i]);
	}
	return result;
}
