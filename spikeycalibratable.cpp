#include "common.h"

// communication
#include "idata.h"
#include "sncomm.h"
#include "sc_sctrl.h"
#include "sc_pbmem.h"

// chip

#include "spikenet.h"
#include "ctrlif.h"
#include "synapse_control.h" //synapse control class
#include "pram_control.h"    //parameter ram etc

#include "spikeyconfig.h"
#include "spikeyvoutcalib.h"
#include "spikey.h"
#include "spikeycalibratable.h"

// xml
#include <Qt/qdom.h>
#include <Qt/qfile.h>
#include <iostream>

static log4cxx::LoggerPtr logger = log4cxx::Logger::getLogger("HAL.Cal");

using namespace spikey2;

//******** SpikeyCalibratable ***********

SpikeyCalibratable::SpikeyCalibratable(boost::shared_ptr<Spikenet> snet, float clk, uint chipid,
                                       int spikeyNr, string calibfile)
    : Spikey(snet, clk, chipid)
{
	clb.reset(new SpikeyVoutCalib(this, spikeyNr, calibfile));
	convVoltDacWarnings.resize(SpikeyConfig::num_blocks * hw_const->ar_numvouts());
	for (uint i = 0; i < SpikeyConfig::num_blocks * hw_const->ar_numvouts(); ++i) {
		convVoltDacWarnings[i] = 1; // 1 warning, then silence
	}
}

SpikeyCalibratable::SpikeyCalibratable(boost::shared_ptr<SpikenetComm> comm, float clk, uint chipid,
                                       int spikeyNr, string calibfile)
    : Spikey(comm, clk, chipid)
{
	clb.reset(new SpikeyVoutCalib(this, spikeyNr, calibfile));
	convVoltDacWarnings.resize(SpikeyConfig::num_blocks * hw_const->ar_numvouts());
	for (uint i = 0; i < SpikeyConfig::num_blocks * hw_const->ar_numvouts(); ++i) {
		convVoltDacWarnings[i] = 1; // 1 warning, then silence
	}
}

// !!! this the convVoltDac for SPIKEY2 !!!
uint SpikeyCalibratable::convVoltDac(double voltage, int voutNr)
{
	LOG4CXX_TRACE(logger, "corslope: " << clb->corSlope[voutNr] << "\tcoroff: "
	                                   << clb->corOff[voutNr] << "\t unmod voltage: " << voltage);
	if (convVoltDacWarnings[voutNr] &&
	    !(clb->validMin[voutNr] <= voltage && voltage <= clb->validMax[voutNr])) {
		LOG4CXX_WARN(logger, "vout#" << setw(2) << setprecision(4) << voutNr << " set to "
		                             << voltage << " but valid range is: " << clb->validMin[voutNr]
		                             << " - " << clb->validMax[voutNr]);
		convVoltDacWarnings[voutNr]--;
	}
	voltage = (voltage / clb->corSlope[voutNr]) - clb->corOff[voutNr];
	double microamps = voltage / rconv100;
	if (microamps < 0)
		microamps = 0;
	LOG4CXX_TRACE(logger, "\tmod voltage: " << voltage);
	LOG4CXX_TRACE(logger, "Spikey 2 - Vout Voltage: " << voltage << " Current: " << microamps
	                                                  << " ");
	return convCurDac(microamps);
}


//! convert paramters to spikey format
void SpikeyCalibratable::loadParam(boost::shared_ptr<SpikeyConfig> c, vector<PramData>& pd)
{
	LOG4CXX_DEBUG(logger, "using spikey calibratable");
	pd.clear();
	pd.reserve(3072);

	// for spikey4 chips, configure only right block synapses and neurons!
	// synapses
	uint bstart, jsynstart;
	if (hw_const->revision() == 4) {
		bstart = 1;
		jsynstart = hw_const->pr_adr_syn1start();
	} else {
		bstart = 0;
		jsynstart = hw_const->pr_adr_syn0start();
	}

	uint j = jsynstart;
	for (uint b = bstart; b < SpikeyConfig::num_blocks; ++b)
		for (uint n = 0; n < SpikeyConfig::num_presyns; ++n) {
			pd.push_back(PramData(
			    j++, convCurDac(c->synapse[b * SpikeyConfig::num_presyns + n].drviout), 0));
			pd.push_back(
			    PramData(j++, convCurDac(c->synapse[b * SpikeyConfig::num_presyns + n].adjdel), 0));
			pd.push_back(PramData(
			    j++, convCurDac(dacimax - c->synapse[b * SpikeyConfig::num_presyns + n].drvirise),
			    0));
			pd.push_back(PramData(
			    j++, convCurDac(c->synapse[b * SpikeyConfig::num_presyns + n].drvifall), 0));
		}
	// neurons
	if (hw_const->revision() == 4) {
		j = hw_const->pr_adr_neuron1start();
		for (uint n = 0; n < SpikeyConfig::num_neurons; ++n) {
			pd.push_back(PramData(
			    j++, convCurDac(dacimax - c->neuron[SpikeyConfig::num_neurons + n].ileak), 9));
			pd.push_back(
			    PramData(j++, convCurDac(c->neuron[SpikeyConfig::num_neurons + n].icb), 9));
			j += 2;
		}
	} else {
		j = hw_const->pr_adr_neuron0start();
		for (uint n = 0; n < SpikeyConfig::num_neurons; ++n) {
			pd.push_back(PramData(j++, convCurDac(dacimax - c->neuron[+n].ileak), 9));
			pd.push_back(PramData(j++, convCurDac(c->neuron[+n].icb), 9));
			j += 2;
		}
		j = hw_const->pr_adr_neuron1start();
		for (uint n = 0; n < SpikeyConfig::num_neurons; ++n) {
			pd.push_back(PramData(
			    j++, convCurDac(dacimax - c->neuron[SpikeyConfig::num_neurons + n].ileak), 9));
			pd.push_back(
			    PramData(j++, convCurDac(c->neuron[SpikeyConfig::num_neurons + n].icb), 9));
			j += 2;
		}
	}
	// bias block
	j = hw_const->pr_adr_biasstart();
	for (uint n = 0; n < PramControl::num_biasb; ++n)
		pd.push_back(PramData(j++, convCurDac(dacimax - c->biasb[n]), 0));
	j = hw_const->pr_adr_outampstart();
	for (uint n = 0; n < PramControl::num_outamp; ++n)
		pd.push_back(PramData(j++, convCurDac(c->outamp[n]), 0));

	// re-swapped writing order for blocks because changed for spikey2 -- DB
	j = hw_const->pr_adr_voutrstart();
	for (uint n = 0; n < hw_const->ar_numvouts();
	     ++n) { // voutl is block 1 since the vout blocks are erroneously swapped in Spikey
		pd.push_back(PramData(
		    j++, convVoltDac(c->vout[hw_const->ar_numvouts() + n], hw_const->ar_numvouts() + n),
		    0));
		pd.push_back(PramData(j++, convCurDac(c->voutbias[hw_const->ar_numvouts() + n]), 0));
	}
	j = hw_const->pr_adr_voutlstart();
	for (uint n = 0; n < hw_const->ar_numvouts(); ++n) {
		pd.push_back(PramData(j++, convVoltDac(c->vout[n], n), 0));
		pd.push_back(PramData(j++, convCurDac(c->voutbias[n]), 0));
	}
	// optimize order of pram entires
	// getPC()->sort_pd(pd);
}
