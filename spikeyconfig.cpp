#include "common.h"

// communication
#include "idata.h"
#include "sncomm.h"

// chip
#include "spikenet.h"
#include "ctrlif.h"
#include "synapse_control.h" //synapse control class
#include "pram_control.h"    //parameter ram etc

#include "spikeyconfig.h"

static log4cxx::LoggerPtr logger = log4cxx::Logger::getLogger("HAL.Cfg");

using namespace spikey2;

SpikeyConfig::SpikeyConfig()
    : dbg(::Logger::instance()), hw_const(new HardwareConstantsRev5USB), valid(ud_none)
{
	// TP: TODO: it would be nice not having HardwareConstantsRev5USB to be hard-coded here
	// anyway, only ar_numvouts is used here ...
}

SpikeyConfig::SpikeyConfig(SCupdate v)
    : dbg(::Logger::instance()), hw_const(new HardwareConstantsRev5USB), valid(ud_none)
{
	setValid(v, true); // fields v valid from the beginning
}

SpikeyConfig::SpikeyConfig(boost::shared_ptr<HardwareConstants> hw)
    : dbg(::Logger::instance()), hw_const(hw), valid(ud_none)
{
}

SpikeyConfig::SpikeyConfig(boost::shared_ptr<HardwareConstants> hw, SCupdate v)
    : dbg(::Logger::instance()), hw_const(hw), valid(ud_none)
{
	setValid(v, true); // fields v valid from the beginning
}

//******** SpikeyConfig methods *********


void SpikeyConfig::updateHardwareConstants(boost::shared_ptr<HardwareConstants> new_hw_const)
{
	hw_const = new_hw_const;
}

// frees/allocs memory for valid fields
void SpikeyConfig::setValid(SCupdate u, bool on)
{
	if (on) {
		// set valarrays to correct size if valid state has changed
		if (!isValid(ud_weight) && u & ud_weight)
			weight.resize(num_blocks * num_neurons * num_presyns, 0);
		if (!isValid(ud_param)) {
			if (u & ud_param) {
				voutbias.resize(num_blocks * hw_const->ar_numvouts(), 0);
				vout.resize(num_blocks * hw_const->ar_numvouts(), 0);
				probepad.resize(num_blocks, 0);
				probebias.resize(num_blocks, 0);
				outamp.resize(PramControl::num_outamp, 0);
				biasb.resize(PramControl::num_biasb, 0);
				if (!isValid(ud_rowconfig))
					synapse.resize(num_blocks * num_presyns);
				if (!isValid(ud_colconfig))
					neuron.resize(num_blocks * num_neurons);
			} else {
				if (!isValid(ud_rowconfig) && (u & ud_rowconfig))
					synapse.resize(num_blocks * num_presyns);
				if (!isValid(ud_colconfig) && (u & ud_colconfig))
					neuron.resize(num_blocks * num_neurons);
			}
		}
	} else {
		if (u & ud_weight)
			weight.resize(num_blocks * num_neurons * num_presyns, 0);
		if (u & ud_param) {
			voutbias.resize(0);
			vout.resize(0);
			probepad.resize(0);
			probebias.resize(0);
			outamp.resize(0);
			biasb.resize(0);
			if (!isValid(ud_rowconfig))
				synapse.resize(0);
			else
				synapse.resize(num_blocks * num_presyns);
			if (!isValid(ud_colconfig))
				neuron.resize(0);
			else
				neuron.resize(num_blocks * num_neurons);
		}
	}
	// update valid bits
	if (on)
		valid = (SCupdate)(valid | u);
	else
		valid = (SCupdate)(valid & ~u);
	LOG4CXX_TRACE(logger, dec << "SpikeyConfig::setValid weight:" << weight.size()
	                          << " syn:" << synapse.size() << " neuron:" << neuron.size());
}

bool SpikeyConfig::readParam(string name)
{
	ifstream i(name.c_str());
	if (!i.good())
		return false;
	LOG4CXX_DEBUG(logger, "Read SpikeyConfig from file: " << name);
	i >> (*this);
	i.close();
	return 1;
}

bool SpikeyConfig::writeParam(string name)
{
	ofstream o(name.c_str());
	if (!o.good())
		return false;
	LOG4CXX_DEBUG(logger, "Write SpikeyConfig to file: " << name);
	o << (*this);
	return o.good();
}

namespace spikey2
{
// a few names
string spikeyconfig_name("SpikeyConfig"), scend_name("EndSpikeyConfig"), ud_chip_name("ud_chip"),
    ud_dac_name("ud_dac"), tsense_name("tsense"), tpcsec_name("tpcsec"),
    tpcorperiod_name("tpcorperiod"), irefdac_name("irefdac"), vcasdac_name("vcasdac"),
    vstart_name("vstart"), vrest_name("vrest"), vm_name("vm"), ud_param_name("ud_param"),
    synapse_name("synapse"), neuron_name("neuron"), drviout_name("drviout"), adjdel_name("adjdel"),
    drvifall_name("drvifall"), drvirise_name("drvirise"), ileak_name("ileak"), icb_name("icb"),
    biasb_name("biasb"), outamp_name("outamp"), vout_name("vout"), voutbias_name("voutbias"),
    probepad_name("probepad"), probebias_name("probebias"), ud_rowconfig_name("ud_rowconfig"),
    ud_colconfig_name("ud_colconfig"), config_name("config"), ud_weight_name("ud_weight"),
    w_name("w"), end_name("end");


// ******* stream spikeyconfig *******
ostream& operator<<(ostream& o, const SpikeyConfig& origCfg)
{
	o << spikeyconfig_name << dec << endl;
	o.precision(4);
	SpikeyConfig cfg = SpikeyConfig(origCfg);
	cfg.valid = SpikeyConfig::ud_all;
	if (cfg.valid & SpikeyConfig::ud_chip) {
		o << ud_chip_name << " " << bool(origCfg.valid & SpikeyConfig::ud_chip) << endl;
		o << tsense_name << " " << cfg.tsense << " " << tpcsec_name << " " << cfg.tpcsec << " "
		  << tpcorperiod_name << " " << cfg.tpcorperiod << endl;
	}

	if (cfg.valid & SpikeyConfig::ud_dac) {
		o << ud_dac_name << " " << bool(origCfg.valid & SpikeyConfig::ud_dac) << endl;
		o << irefdac_name << " " << cfg.irefdac << " " << vcasdac_name << " " << cfg.vcasdac << " "
		  << vm_name << " " << cfg.vm;
		o << " " << vstart_name << " " << cfg.vstart << " " << vrest_name << " " << cfg.vrest
		  << endl;
	}

	if (cfg.valid & SpikeyConfig::ud_param) {
		o << ud_param_name << " " << bool(origCfg.valid & SpikeyConfig::ud_param) << endl;
		float ll[4] = {-1, -1, -1, -1}, l[4];
		for (uint b = 0; b < SpikeyConfig::num_blocks; ++b)
			for (uint n = 0; n < SpikeyConfig::num_presyns; ++n) {
				l[0] = cfg.synapse[b * SpikeyConfig::num_presyns + n].drviout;
				l[1] = cfg.synapse[b * SpikeyConfig::num_presyns + n].adjdel;
				l[2] = cfg.synapse[b * SpikeyConfig::num_presyns + n].drvifall;
				l[3] = cfg.synapse[b * SpikeyConfig::num_presyns + n].drvirise;
				uint i;
				for (i = 0; i < 4; i++)
					if (ll[i] != l[i])
						break;
				if (i == 4)
					continue; // skip this output because it was identical to last one
				for (i = 0; i < 4; i++)
					ll[i] = l[i];
				o << synapse_name << " " << b* SpikeyConfig::num_presyns + n << " " << drviout_name
				  << " " << l[0] << " " << adjdel_name << " " << l[1];
				o << " " << drvifall_name << " " << l[2] << " " << drvirise_name << " " << l[3]
				  << endl;
			}
		o << end_name << endl;
		ll[0] = -1; // invalidate
		for (uint b = 0; b < SpikeyConfig::num_blocks; ++b)
			for (uint n = 0; n < SpikeyConfig::num_neurons; ++n) {
				l[0] = cfg.neuron[b * SpikeyConfig::num_neurons + n].ileak;
				l[1] = cfg.neuron[b * SpikeyConfig::num_neurons + n].icb;
				uint i;
				for (i = 0; i < 2; i++)
					if (ll[i] != l[i])
						break;
				if (i == 2)
					continue;
				for (i = 0; i < 2; i++)
					ll[i] = l[i]; // skip this output because it was identical to last one
				o << neuron_name << " " << b* SpikeyConfig::num_neurons + n << " " << ileak_name
				  << " " << l[0] << " " << icb_name << " " << l[1] << endl;
			}
		o << end_name << endl;
		o << biasb_name;
		for (uint n = 0; n < PramControl::num_biasb; ++n)
			o << " " << cfg.biasb[n];
		o << endl
		  << outamp_name;
		for (uint n = 0; n < PramControl::num_outamp; ++n)
			o << " " << cfg.outamp[n];
		o << endl;
		for (uint b = 0; b < SpikeyConfig::num_blocks; ++b) {
			o << setw(voutbias_name.size()) << left << (b == 0 ? vout_name : " ") << right;
			for (uint n = 0; n < cfg.hw_const->ar_numvouts(); ++n) // voutl is block 1 since the
			                                                       // vout blocks are erroneously
			                                                       // swapped in Spikey
				o << " " << setw(4) << cfg.vout[b * cfg.hw_const->ar_numvouts() + n];
			o << endl;
		}

		for (uint b = 0; b < SpikeyConfig::num_blocks; ++b) {
			o << setw(voutbias_name.size()) << (b == 0 ? voutbias_name : " ");
			for (uint n = 0; n < cfg.hw_const->ar_numvouts(); ++n) // voutl is block 1 since the
			                                                       // vout blocks are erroneously
			                                                       // swapped in Spikey
				o << " " << setw(4) << cfg.voutbias[b * cfg.hw_const->ar_numvouts() + n];
			o << endl;
		}
		for (uint b = 0; b < SpikeyConfig::num_blocks; ++b) {
			o << setw(probepad_name.size()) << (b == 0 ? probepad_name : " ");
			o << " " << setw(4) << cfg.probepad[b];
			o << endl;
		}
		for (uint b = 0; b < SpikeyConfig::num_blocks; ++b) {
			o << setw(probebias_name.size()) << (b == 0 ? probebias_name : " ");
			o << " " << setw(4) << cfg.probebias[b];
			o << endl;
		}
	}
	if (cfg.valid & SpikeyConfig::ud_rowconfig) {
		o << ud_rowconfig_name << " " << bool(origCfg.valid & SpikeyConfig::ud_rowconfig) << endl;
		bitset<SpikeyConfig::num_sc> ll, l;
		for (uint b = 0; b < SpikeyConfig::num_blocks; ++b)
			for (uint n = 0; n < SpikeyConfig::num_presyns; ++n) {
				l = cfg.synapse[b * SpikeyConfig::num_presyns + n].config;
				if ((ll == l) && (b != 0 || n != 0))
					continue; // skip first test, first one must be always written
				ll = l;
				o << synapse_name << " " << b* SpikeyConfig::num_presyns + n << " " << config_name
				  << " " << l << endl;
			}
		o << end_name << endl;
	}
	if (cfg.valid & SpikeyConfig::ud_colconfig) {
		o << ud_colconfig_name << " " << bool(origCfg.valid & SpikeyConfig::ud_colconfig) << endl;
		bitset<SpikeyConfig::num_nc> ll, l;
		for (uint b = 0; b < SpikeyConfig::num_blocks; ++b)
			for (uint n = 0; n < SpikeyConfig::num_neurons; ++n) {
				l = cfg.neuron[b * SpikeyConfig::num_neurons + n].config;
				if ((ll == l) && (b != 0 || n != 0))
					continue; // skip first test, first one must be always written
				ll = l;
				o << neuron_name << " " << b* SpikeyConfig::num_neurons + n << " " << config_name
				  << " " << l << endl;
			}
		o << end_name << endl;
	}

	if (cfg.valid & SpikeyConfig::ud_weight) {
		o << ud_weight_name << " " << bool(origCfg.valid & SpikeyConfig::ud_weight) << endl;
		for (uint b = 0; b < SpikeyConfig::num_blocks; ++b)
			for (uint n = 0; n < SpikeyConfig::num_neurons; ++n) {
				valarray<ubyte> syn(cfg.col(b, n));
				// if(syn.sum()==0)continue;//supress all zero lines
				o << w_name << " " << dec << setw(3) << b * SpikeyConfig::num_perprienc * 3 + n;
				for (uint c = 0; c < SpikeyConfig::num_presyns; ++c)
					o << " " << hex << setw(1) << (uint)syn[c];
				o << endl;
			}
		o << end_name << endl;
	}
	o << scend_name << endl;
	return o;
}

// updates cfg from stream (does not change fields update==0 in input stream)
istream& operator>>(istream& i, SpikeyConfig& cfg)
{
	i.exceptions(istream::eofbit | istream::failbit | istream::badbit);
	string s;
	uint j, lastsyn = 0, lastn = 0;
	LOG4CXX_TRACE(logger, "SpikeyConfig operator<<");
	try {
		enum { none, outer, dac, chip, weight, colconf, rowconf, param } state = none;
		while (true) {
			i >> s;
			if (s[0] == '#') {
				i.ignore(0xfffffff, '\n');
				continue;
			} // comment, skip to end of line
			LOG4CXX_TRACE(logger, s << " ");
			switch (state) {
				case none: // look for header
					if (s == spikeyconfig_name)
						state = outer;
					continue;
				case outer: // look for update 1 statement
					if (s == scend_name)
						break; // finish reading

					if (s != ud_chip_name && s != ud_dac_name && s != ud_param_name &&
					    s != ud_rowconfig_name && s != ud_colconfig_name && s != ud_weight_name) {
						LOG4CXX_WARN(logger, "Unknown 'update' field in SpikeyConfig::operator>>:"
						                         << s << " ignoring");
						i.ignore(0xfffffff, '\n');
						continue;
					}
					lastsyn = 0xffffff;
					lastn = 0xffffff;
					i >> j;
					if (j == 1) { // if update true in file and config -> update
						if (s == ud_chip_name) {
							state = chip;
							cfg.setValid(SpikeyConfig::ud_chip, true);
						}
						if (s == ud_dac_name) {
							state = dac;
							cfg.setValid(SpikeyConfig::ud_dac, true);
						}
						if (s == ud_param_name) {
							state = param;
							cfg.setValid(SpikeyConfig::ud_param, true);
						};
						if (s == ud_rowconfig_name) {
							state = rowconf;
							cfg.setValid(SpikeyConfig::ud_rowconfig, true);
						}
						if (s == ud_colconfig_name) {
							state = colconf;
							cfg.setValid(SpikeyConfig::ud_colconfig, true);
						}
						if (s == ud_weight_name) {
							state = weight;
							cfg.setValid(SpikeyConfig::ud_weight, true);
						}
					};
					continue;
				case chip:
					i >> cfg.tsense >> s >> cfg.tpcsec >> s >> cfg.tpcorperiod;
					state = outer;
					continue;
				case dac:
					i >> cfg.irefdac >> s >> cfg.vcasdac >> s >> cfg.vm >> s >> cfg.vstart >> s >>
					    cfg.vrest;
					state = outer;
					continue;
				case param:
					if (s == synapse_name || s == end_name) {
						if (s != end_name)
							i >> j; // get index
						else
							j = SpikeyConfig::num_blocks * SpikeyConfig::num_presyns;
						if (lastsyn + 1 < j) { // fill remaining indices
							LOG4CXX_TRACE(logger, "syn filling up from: " << lastsyn + 1 << " to "
							                                              << j - 1);
							for (uint k = lastsyn + 1; k < j; ++k) {
								cfg.synapse[k].drviout = cfg.synapse[lastsyn].drviout;
								cfg.synapse[k].adjdel = cfg.synapse[lastsyn].adjdel;
								cfg.synapse[k].drvifall = cfg.synapse[lastsyn].drvifall;
								cfg.synapse[k].drvirise = cfg.synapse[lastsyn].drvirise;
							}
						}
						if (s != end_name)
							i >> s >> cfg.synapse[j].drviout >> s >> cfg.synapse[j].adjdel >> s >>
							    cfg.synapse[j].drvifall >> s >> cfg.synapse[j].drvirise;
						lastsyn = j;
					}
					if (s == neuron_name || s == end_name) {
						if (s != end_name) {
							i >> j; // get index
							// if(j>SpikeyConfig::num_neurons)j-=SpikeyConfig::num_nadr-SpikeyConfig::num_neurons;//adjust
							// neuron index
							// else j=SpikeyConfig::num_blocks*SpikeyConfig::num_neurons;
						} else
							j = SpikeyConfig::num_blocks * SpikeyConfig::num_neurons;
						if (lastn + 1 < j) { // fill remaining indices
							LOG4CXX_TRACE(logger, "neuron filling up from: " << lastn + 1 << " to "
							                                                 << j - 1);
							for (uint k = lastn + 1; k < j; ++k) {
								cfg.neuron[k].ileak = cfg.neuron[lastn].ileak;
								cfg.neuron[k].icb = cfg.neuron[lastn].icb;
							}
						}
						if (s != end_name)
							i >> s >> cfg.neuron[j].ileak >> s >> cfg.neuron[j].icb;
						lastn = j;
					}
					if (s == biasb_name)
						for (uint n = 0; n < PramControl::num_biasb; ++n)
							i >> cfg.biasb[n];
					if (s == outamp_name)
						for (uint n = 0; n < PramControl::num_outamp; ++n)
							i >> cfg.outamp[n];
					if (s == vout_name)
						for (uint b = 0; b < SpikeyConfig::num_blocks; ++b)
							for (uint n = 0; n < cfg.hw_const->ar_numvouts(); ++n)
								i >> cfg.vout[b * cfg.hw_const->ar_numvouts() + n];
					if (s == voutbias_name) {
						for (uint b = 0; b < SpikeyConfig::num_blocks; ++b)
							for (uint n = 0; n < cfg.hw_const->ar_numvouts(); ++n)
								i >> cfg.voutbias[b * cfg.hw_const->ar_numvouts() + n];
					}
					if (s == probepad_name) {
						for (uint b = 0; b < SpikeyConfig::num_blocks; ++b)
							i >> cfg.probepad[b];
					}

					if (s == probebias_name) {
						for (uint b = 0; b < SpikeyConfig::num_blocks; ++b)
							i >> cfg.probebias[b];
						state = outer; // probebias is always last field in param
					}
					continue;
				case weight:
					if (s == w_name) {
						i >> j;
						uint b = 0;
						if (j >= SpikeyConfig::num_neurons) {
							b = 1;
							j -= SpikeyConfig::num_neurons;
						}
						valarray<ubyte> syn(SpikeyConfig::num_presyns);
						LOG4CXX_TRACE(logger, "");
						for (uint c = 0; c < SpikeyConfig::num_presyns; ++c) {
							uint k;
							i >> hex >> k;
							syn[c] = k;
							if (k != 0)
								LOG4CXX_TRACE(logger, " " << (uint)syn[c]);
						};
						cfg.col(b, j) = syn;
					} else
						state = outer; // end weights
					i >> dec;
					continue;
				case rowconf:
					if (s != end_name)
						i >> j; // get index
					else {
						j = SpikeyConfig::num_blocks * SpikeyConfig::num_presyns;
						state = outer;
					}

					if (lastsyn + 1 < j) // fill remaining indices
						for (uint k = lastsyn + 1; k < j; ++k) {
							cfg.synapse[k].config = cfg.synapse[lastsyn].config;
						}
					if (s != end_name)
						i >> s >> cfg.synapse[j].config;
					lastsyn = j;
					continue;
				case colconf:
					if (s != end_name) {
						i >> j; // get index
					} else {
						j = SpikeyConfig::num_blocks * SpikeyConfig::num_neurons;
						state = outer;
					}

					if (lastn + 1 < j) // fill remaining indices
						for (uint k = lastn + 1; k < j; ++k) {
							cfg.neuron[k].config = cfg.neuron[lastn].config;
						}
					if (s != end_name)
						i >> s >> cfg.neuron[j].config;
					lastn = j;
					continue;
			} // end switch
			if (s == scend_name)
				break;
		} // end while
	} // end try
	catch (ifstream::failure e) {
		LOG4CXX_ERROR(logger, "Exception SpikeyConfig operator>>");
	}
	return i;
}
}

// ********** Spiketrain methods **********

// output format
// neuron list ->
// time \/ (timbin indicates event)
//
//       0 0 0 0 2 2 5
//       1 2 3 4 0 1 0
//
// 0
// 1
// 2        4
// 3              1

void SpikeTrain::getNeuronList(vector<uint>& n) const
{
	n.clear();
	for (uint i = 0; i < d.size(); ++i)
		n.push_back(d[i].neuronAdr());
	sort(n.begin(), n.end());
	// now prune
	vector<uint>::iterator e = unique(n.begin(), n.end());
	n.erase(e, n.end());
}

bool SpikeTrain::writeToFile(string filename) const
{
	ofstream o(filename.c_str());
	if (!(o.good()))
		return false;
	LOG4CXX_DEBUG(logger, "Writing SpikeTrain to file: " << filename);

	uint neuron, time;
	o << this->d.size() << endl;
	for (uint e = 0; e < this->d.size(); ++e) {
		if (this->d[e].isEvent()) {
			time = this->d[e].time();
			// cout << "given hw time " << time << endl;
			neuron = this->d[e].neuronAdr();
			// cout << "given hw neuron " << neuron << endl;
			o << neuron << "\t" << time << '\n';
		}
	}
	return o.good();
}

void SpikeTrain::writeNeuroToolsFile(string filename) const
{
	ofstream o(filename.c_str());
	if (!(o.good()))
		return;

	LOG4CXX_DEBUG(logger, "Writing SpikeTrain to file: " << filename);

	int neuron, time;
	for (uint e = 0; e < this->d.size(); ++e) {
		if (this->d[e].isEvent()) {
			time = this->d[e].time();
			// cout << "given hw time " << time << endl;
			neuron = this->d[e].neuronAdr();
			// cout << "given hw neuron " << neuron << endl;
			o << time << '\t' << neuron << '\n';
		}
	}
}


bool SpikeTrain::readFromFile(string filename)
{
	ifstream i(filename.c_str());
	if (!(i.good()))
		return false;
	LOG4CXX_DEBUG(logger, "Reading SpikeTrain from file: " << filename);

	int neuron, time;
	uint numSpikes;

	this->d.clear();
	i >> numSpikes;
	for (uint s = 0; s < numSpikes; ++s) {
		i >> neuron >> time;
		this->d.push_back(IData::Event(neuron, time));
	}
	return i.good();
}


// for sort later
bool lessneuron(const IData& a, const IData& b)
{
	return a.neuronAdr() < b.neuronAdr();
}
bool lessntime(const IData& a, const IData& b)
{
	if (a.clk() != b.clk())
		return a.clk() < b.clk();
	else
		return a.bin() < b.bin();
}


namespace spikey2
{
ostream& operator<<(ostream& o, const SpikeTrain& st)
{
	if (st.d.empty()) {
		o << "no events" << endl;
		return o; // nothing to print
	}
	// sort spiketrain in time
	vector<IData> sst = st.d;
	sort(sst.begin(), sst.end(), lessntime);
	uint starttime = sst[0].clk();
	uint endtime = sst[sst.size() - 1].clk();

	// calculate mean event rate and decide wether empty time points should be shown
	bool showempty = true;
	float mean = sst.size() / (endtime - starttime + 1);
	if (mean < 0.1)
		showempty = false; // limit is 10%

	// generate neuron list from st
	vector<uint> neurons;
	st.getNeuronList(neurons);
	// check number of digits for time and neuron output
	uint numlines = 1, lastneuron = neurons[neurons.size() - 1];
	if (lastneuron > 100)
		numlines = 3;
	else if (lastneuron > 10)
		numlines = 2;
	// display header
	for (int l = numlines - 1; l >= 0; --l) {
		o << "#       ";
		for (uint i = 0; i < neurons.size(); ++i) {
			uint no = neurons[i];
			no /= (uint)pow(10.0, (int)l);
			if (l != 0 && neurons[i] < pow(10.0, (int)l))
				o << "  "; // supress leading zeros if l!=0
			else
				o << (no % 10) << " ";
		}
		o << endl;
	}
	o << "# ------";
	for (uint i = 0; i < neurons.size(); ++i)
		o << "--";
	o << "-";
	vector<IData>::const_iterator j = sst.begin();
	for (uint l = starttime; l <= endtime; ++l) { // loop over all clock steps
		vector<IData> nattime; // activ neurons in clock step
		while (j != sst.end() && j->clk() == l) {
			// cout << endl << "spconfig: pushing back " << dec << j->clk() << endl;
			nattime.push_back(*j); // get list of neurons activ in this clock step
			++j;
		}
		if (nattime.empty()) {
			if (j != sst.end()) {
				if (!showempty)
					continue;
			} // nothing for this bin
			else
				break; // spiketrain empty
		}
		sort(nattime.begin(), nattime.end(), lessneuron);
		vector<IData>::iterator nt = nattime.begin();
		// diplay time index
		o << endl
		  << setfill(' ') << setw(6) << hex << l << ": ";
		if (nattime.empty())
			continue;
		for (uint i = 0; i < neurons.size(); ++i) { // output timbin for neurons with event
			if (nt->neuronAdr() == neurons[i]) {
				// (sf) added code to show duplicates
				if (((nt + 1) != nattime.end()) && ((nt + 1)->neuronAdr() == neurons[i])) {
					o << "D ";
					while ((++nt != nattime.end()) && (nt->neuronAdr() == neurons[i]))
						;
				} else {
					o << hex << nt->bin() << " ";
					++nt;
				}

				if (nt == nattime.end())
					break;
			} else
				o << "  ";
		}
	}
	o << endl;
	return o;
}
}
