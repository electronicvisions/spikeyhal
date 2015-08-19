// testbench generation for spikenet simulations

#include "common.h" // library includes

#include "idata.h"
#include "sncomm.h"

#include "spikenet.h"
#include "ctrlif.h"
#include "synapse_control.h" //synapse control class

static log4cxx::LoggerPtr logger = log4cxx::Logger::getLogger("HAL.SyC");

using namespace spikey2;

void SynapseControl::set_LUT(std::vector<int> _lut)
{
	assert(_lut.size() == 2 * (tbmax + 1));
	for (int i = 0; i < 2 * (tbmax + 1); i++) {
		assert(_lut[i] <= tbmax);
		assert(_lut[i] >= 0);
	}
	lut = _lut;
}

// generate LUT data written to the chip
// for each LUT entry weight has to be repeated "no of correlation priority encoders"
uint SynapseControl::gen_plut_data(float nval)
{
	uint out = 0;
	for (uint j = 0; j < sp->hw_const->sc_cprienc(); j++) {
		out |= ((uint)nval) << (j * sp->hw_const->sc_plut_res());
	}
	return out;
}

// creates linear table with slope and offset (measured from zero)
bool SynapseControl::fill_plut(int mode, float slope, float off)
{
	float nval;
	int i;
	bool ok = true;
	std::vector<uint> plut(2 * (tbmax + 1)); // acausal and causal in one vector

	if (mode & linear) { // calculate table (linear)
		for (i = tbmax; i >= 0; i--) { // acausal; plut[0..15]
			nval = off + (i - 1) * slope;
			if (nval < 0)
				nval = 0;
			if (nval > tbmax)
				nval = tbmax;
			plut[i] = gen_plut_data(nval);
			LOG4CXX_TRACE(logger, "plut i=" << i << " nval=" << nval << " out=" << plut[i]);
		}
		for (i = 0; i <= tbmax; i++) { // causal; plut[16..31]
			nval = off + (i + 1) * slope;
			if (nval < 0)
				nval = 0;
			if (nval > tbmax)
				nval = tbmax;
			plut[i + tbmax + 1] = gen_plut_data(nval);
			LOG4CXX_TRACE(logger, "plut i=" << i + tbmax + 1 << " nval=" << nval
			                                << " out=" << plut[i + tbmax + 1]);
		}
	} else if (mode & custom) {
		for (i = 0; i < (2 * (tbmax + 1)); i++) {
			plut[i] = gen_plut_data(lut[i]);
			LOG4CXX_TRACE(logger, "plut i=" << i << " nval=" << lut[i] << " out=" << plut[i]);
		}
	} else { // identity
		for (i = 0; i < (2 * (tbmax + 1)); i++) {
			nval = i & mmw(sp->hw_const->sc_plut_res()); // hardy modulo
			plut[i] = gen_plut_data(nval);
			LOG4CXX_TRACE(logger, "plut i=" << i << " nval=" << nval << " out=" << plut[i]);
		}
	}

	// TP: TODO: verifying the LUT does not work currently, because manual Flush() needed
	// transfer to chip
	for (i = 0; i < (2 * (tbmax + 1)); i++) {
		if (mode & write)
			write_plut(i, plut[i]);

		if (mode & verify) { // this does not work
			read_plut(i);
			if (i > 0) {
				if (!check_plut(plut[i - 1]))
					ok = false;
			} else
				check_plut(0); // ignore result
		}
	}
	if (mode & verify) { // verify last table entry
		read_plut(0);
		if (!check_plut(plut[i - 1]))
			ok = false;
	}
	return ok;
}
