// testbench generation for spikenet simulations

#include "common.h"
#include "idata.h"
#include "sncomm.h"

#include "spikenet.h"
#include "ctrlif.h"
#include "synapse_control.h" //synapse control class

#include "pram_control.h"

#include <algorithm>
#include <cassert>

static log4cxx::LoggerPtr logger = log4cxx::Logger::getLogger("HAL.PRC");

using namespace spikey2;

bool lesscurrent(const PramData& a, const PramData& b)
{
	return a.value < b.value;
}


void PramControl::sort_pd_triangle(vector<PramData>& in)
{
	stable_sort(in.begin(), in.end(), lesscurrent);

	// to avoid a discontinuous ramp, produce a triangle _.´`._
	vector<PramData> temp(in.size());
	vector<PramData>::const_iterator iter = in.begin();
	vector<PramData>::iterator toit = temp.begin();
	vector<PramData>::reverse_iterator roit = temp.rbegin();

	while (iter != in.end()) {
		*toit = *iter;
		if (++iter == in.end())
			break;
		*roit = *iter;
		++toit;
		++roit;
		++iter;
	}
	in = temp; // overwrite old vector
}


void PramControl::sort_pd(vector<PramData>& in)
{
	// keep order the same for same values -> reduces unnecessary address transitions
	stable_sort(in.begin(), in.end(), lesscurrent);
}

void PramControl::blocksort_pd(vector<PramData>& in)
{
	// keep order the same for same values -> reduces unnecessary address transitions

	stable_sort(in.begin(), in.end(), lesscurrent);

	// write blockwise, we need to split by cadrs
	vector<PramData> syn0, syn1, neuron0, neuron1, voutl, voutr, bias, outamp, manip;
	map<unsigned, PramData> temp;
	map<unsigned, PramData>::iterator tempIter;

	// now sort blockwise
	std::vector<PramData>::iterator inIter;
	for (inIter = in.begin(); inIter != in.end(); inIter++) {
		assert(inIter->cadr < 4096); // 2 ** 12

		if (inIter->cadr < sp->hw_const->pr_adr_syn1start()) {
			syn0.push_back(*inIter);
			continue;
		}
		if (inIter->cadr < sp->hw_const->pr_adr_neuron0start()) {
			syn1.push_back(*inIter);
			continue;
		}
		if (inIter->cadr < sp->hw_const->pr_adr_neuron1start()) {
			neuron0.push_back(*inIter);
			continue;
		}
		if (inIter->cadr < sp->hw_const->pr_adr_voutlstart()) {
			neuron1.push_back(*inIter);
			continue;
		}
		if (inIter->cadr < sp->hw_const->pr_adr_voutrstart()) {
			voutl.push_back(*inIter);
			continue;
		}
		if (inIter->cadr < sp->hw_const->pr_adr_biasstart()) {
			voutr.push_back(*inIter);
			continue;
		}
		if (inIter->cadr < sp->hw_const->pr_adr_outampstart()) {
			bias.push_back(*inIter);
			continue;
		}
		if (inIter->cadr > sp->hw_const->pr_adr_outampstart()) {
			outamp.push_back(*inIter);
			continue;
		}
	}

	//    sort_pd_triangle(syn0);

	/* now build new "in" writing blocks sequentially
	 *
	 * vouts are written three times :)
	 */

	in.clear();
	in.insert(in.end(), syn0.begin(), syn0.end());
	in.insert(in.end(), voutl.begin(), voutl.end());
	in.insert(in.end(), voutr.begin(), voutr.end());
	in.insert(in.end(), syn1.begin(), syn1.end());
	in.insert(in.end(), neuron0.begin(), neuron0.end());
	in.insert(in.end(), neuron1.begin(), neuron1.end());
	in.insert(in.end(), bias.begin(), bias.end());
	in.insert(in.end(), outamp.begin(), outamp.end());
	in.insert(in.end(), manip.begin(), manip.end());
}

PramControl::DACblock PramControl::whereIs(uint cadr)
{
	assert(cadr < 4096); // 2 ** 12

	PramControl::DACblock inBlock;

	if (cadr < sp->hw_const->pr_adr_syn1start()) {
		inBlock = PramControl::syn0Blk;
	} else if (cadr < sp->hw_const->pr_adr_neuron0start()) {
		inBlock = PramControl::syn1Blk;
	} else if (cadr < sp->hw_const->pr_adr_neuron1start()) {
		inBlock = PramControl::neuron0Blk;
	} else if (cadr < sp->hw_const->pr_adr_voutlstart()) {
		inBlock = PramControl::neuron1Blk;
	} else if (cadr < sp->hw_const->pr_adr_voutrstart()) {
		inBlock = PramControl::voutlBlk;
	} else if (cadr < sp->hw_const->pr_adr_biasstart()) {
		inBlock = PramControl::voutrBlk;
	} else if (cadr < sp->hw_const->pr_adr_outampstart()) {
		inBlock = PramControl::biasBlk;
	} else if (cadr > sp->hw_const->pr_adr_outampstart()) {
		inBlock = PramControl::outampBlk;
	}

	return inBlock;
}

bool PramControl::isVoltage(uint cadr)
{
	return ((whereIs(cadr) == PramControl::voutlBlk || whereIs(cadr) == PramControl::voutrBlk) &&
	        cadr % 2 == 0);
}

int PramControl::lookUpLUT(uint previousDac, uint targetCadr, uint actualDac, vector<LutData>& lut)
{
	// this method returns LUT numbers

	// calculate which write time to use
	int step = actualDac - previousDac;
	if (step < 0)
		step = step * (-1);
	uint powerOfTwo;
	// 0 - 15
	if (actualDac < 15) {
		if (previousDac == 0)
			powerOfTwo = 11;
		else
			powerOfTwo = 13;
	}
	// 15-100
	else if (actualDac < 100) {
		powerOfTwo = 9;
	}
	// 100-300
	else if (actualDac < 300) {
		powerOfTwo = 7;
	}
	// 300-600
	else if (actualDac < 600) {
		if (step > 300)
			powerOfTwo = 9;
		else
			powerOfTwo = 7;
	}
	// 600-1023
	else {
		if (step <= 512)
			powerOfTwo = 5;
		else
			powerOfTwo = 11;
	}
	assert(powerOfTwo < 16);
	// determine to which LUT entry the calculated write time corresponds
	vector<LutData>::iterator lutIt = lut.begin();
	bool lutFound = 0;
	int counter = 0;
	for (; lutIt != lut.end(); lutIt++) {
		if (isVoltage(targetCadr)) {
			if (lutIt->luttime == powerOfTwo) {
				lutFound = 1;
				break;
			}
		} else {
			if (lutIt->lutboost == powerOfTwo) {
				lutFound = 1;
				break;
			}
		}
		counter++;
	}
	if (!lutFound) {
		LOG4CXX_ERROR(logger, "LUT to write " << (isVoltage(targetCadr) ? "voltage" : "current")
		                                      << " with 2^" << powerOfTwo << " not found!");
		return -1;
	} else
		return counter;
}

void PramControl::check_pd_timing(vector<PramData>& in, vector<LutData>& lut)
{
	vector<PramData>::iterator inIter = in.begin();
	vector<PramData>::iterator prevInIter = inIter;
	int defaultTimingVoltage = 2; // correspond both to 2^7 cycles
	int defaultTimingCurrent = 9; // but for currents we'll have to write with boost only
	int maxVoltageDacValue = 0;
	int maxCurrentDacValue = 0;
	int minVoltageDacValue = 1023;
	int minCurrentDacValue = 1023;

	LOG4CXX_TRACE(logger, "First entry in pram: " << inIter->cadr << " = " << inIter->value << " @ "
	                                              << inIter->lutadr);
	// write first value with default timing
	// check if first entry is a voltage parameter
	if ((whereIs(inIter->cadr) == PramControl::voutlBlk ||
	     whereIs(inIter->cadr) == PramControl::voutrBlk) &&
	    inIter->cadr % 2 == 0) {
		cout << 1;
		inIter->lutadr = defaultTimingVoltage;
	} else
		inIter->lutadr = defaultTimingCurrent;
	inIter++;

	unsigned int pramCycleCnt = 0;
	unsigned int lutAdr; //,lutAdrBits;
	for (; inIter != in.end(); prevInIter = inIter++) {

		PramData source(prevInIter->cadr, prevInIter->value, prevInIter->lutadr);
		PramData target(inIter->cadr, inIter->value, inIter->lutadr);
		// check max and min Dac value
		if (isVoltage(target.cadr)) {
			if (target.value > maxVoltageDacValue)
				maxVoltageDacValue = target.value;
			if (target.value < minVoltageDacValue)
				minVoltageDacValue = target.value;
		} else {
			if (target.value > maxCurrentDacValue)
				maxCurrentDacValue = target.value;
			if (target.value < minCurrentDacValue)
				minCurrentDacValue = target.value;
		}
		// adjust LUT values
		lutAdr = lookUpLUT(source.value, target.cadr, target.value, lut);
		assert(lutAdr < 16);
		inIter->lutadr = lutAdr; // overwrite default timing
		// count write cycles
		uint fac = lut.at(inIter->lutadr).lutrepeat == 0 ? 1 : lut.at(inIter->lutadr).lutrepeat;
		pramCycleCnt += int(1 << lut.at(inIter->lutadr).luttime) * fac;
		pramCycleCnt += int(1 << lut.at(inIter->lutadr).lutboost) * fac;
	}
	// output full pram refresh time to the software user
	if (sp->bus->classname != "sc_trans") {
		if (pramCycleCnt != lastPramCycleCnt)
			LOG4CXX_DEBUG(logger, "A full pram refresh takes "
			                          << pramCycleCnt << " cycles ("
			                          << (pramCycleCnt * 1.0 / (100 * 1000)) // 100 MHz, ms
			                          << " ms).");
		lastPramCycleCnt = pramCycleCnt;
	}

	if (pramCycleCnt * 1.0 / (100 * 1000) > 8 && maxVoltageDacValue >= 600) {
		LOG4CXX_WARN(logger,
		             "Pram refresh time too long for precise Dac voltage values larger than 600!");
	} else if (pramCycleCnt * 1.0 / (100 * 1000) > 2 && minVoltageDacValue < 50) {
		LOG4CXX_WARN(logger,
		             "Pram refresh time too long for precise Dac voltage values smaller than 50!");
	} else if (pramCycleCnt * 1.0 / (100 * 1000) > 8 && minCurrentDacValue < 100) {
		LOG4CXX_WARN(logger,
		             "Pram refresh time too long for precise Dac current values smaller than 50!");
	}
}

void PramControl::write_pd(vector<PramData>& in, uint del, vector<LutData> lut)
{

	vector<PramData>::iterator inIter = in.begin();

	LOG4CXX_TRACE(logger,
	              "PramControl::check_pd_timing: Parameter dump: cadr value luttime+lutboost");
	int counter = 0;
	for (; inIter != in.end(); inIter++) {
		// dbg(::Logger::DEBUG1) << inIter->cadr << "\t" << inIter->value << "\t" <<
		// lut.at(inIter->lutadr).luttime + lut.at(inIter->lutadr).lutboost;
		// if (isVoltage(inIter->cadr)) dbg << " (V)";
		// here's where the actual pram values are written
		write_pram(counter++, *inIter, del);
	}
}

void AnalogReadout::set(int c, uint num, bool verify, uint del)
{
	assert(del >= scdelay);
	if (sp->hw_const->revision() != 4) {
		if (verify) {
			read_cmd((1ULL << ((sp->hw_const->ar_maxlen() - 1 - num) +
			                   sp->hw_const->ar_chainnumberwidth())) |
			             c & mmw(sp->hw_const->ar_chainnumberwidth()),
			         del);
			read_cmd((1ULL << ((sp->hw_const->ar_maxlen() - 1 - num) +
			                   sp->hw_const->ar_chainnumberwidth())) |
			             c & mmw(sp->hw_const->ar_chainnumberwidth()),
			         del);
		} else
			write_cmd((1ULL << ((sp->hw_const->ar_maxlen() - 1 - num) +
			                    sp->hw_const->ar_chainnumberwidth())) |
			              c & mmw(sp->hw_const->ar_chainnumberwidth()),
			          del);
	} else {
		if (verify) {
			/*	In Spikey4 the ar_maxlen was not increased, even though
			 *	the number of cells in the chain was increased by one.
			 *	Thus, as a workaround, one has to write to the chain twice,
			 *	depending on the expected value of the 25th cell.
			 */
			if (num == 0) {
				/*	If we want to write a 1 to cell nr. 24, we first need to
				 *	write a 1 to the first cell and send 24 zeros afterwards.
				 */
				read_cmd(0 | c & mmw(sp->hw_const->ar_chainnumberwidth()), del);
				read_cmd((1ULL << ((sp->hw_const->ar_maxlen() - 24) +
				                   sp->hw_const->ar_chainnumberwidth())) |
				             c & mmw(sp->hw_const->ar_chainnumberwidth()),
				         del);
				//					|	= zero		|
				read_cmd(0 | c & mmw(sp->hw_const->ar_chainnumberwidth()), del);
				read_cmd((1ULL << ((sp->hw_const->ar_maxlen() - 24) +
				                   sp->hw_const->ar_chainnumberwidth())) |
				             c & mmw(sp->hw_const->ar_chainnumberwidth()),
				         del);
				//					|	= zero		|
				read_cmd(0 | c & mmw(sp->hw_const->ar_chainnumberwidth()), del);
			} else {
				/*	If we want to write a 0 to cell nr. 24 (i.e. a 1 to a cell
				 *	other than cell nr. 24), we first need to
				 *	write a 0 to all cells and send the real values afterwards.
				 */
				read_cmd(0 | c & mmw(sp->hw_const->ar_chainnumberwidth()), del);
				read_cmd(0 | c & mmw(sp->hw_const->ar_chainnumberwidth()), del);
				read_cmd((1ULL << ((sp->hw_const->ar_maxlen() - num) +
				                   sp->hw_const->ar_chainnumberwidth())) |
				             c & mmw(sp->hw_const->ar_chainnumberwidth()),
				         del);
				read_cmd(0 | c & mmw(sp->hw_const->ar_chainnumberwidth()), del);
				read_cmd((1ULL << ((sp->hw_const->ar_maxlen() - num) +
				                   sp->hw_const->ar_chainnumberwidth())) |
				             c & mmw(sp->hw_const->ar_chainnumberwidth()),
				         del);
			}
		} else {
			/*	In Spikey4 the ar_maxlen was not increased, even though
			 *	the number of cells in the chain was increased by one.
			 *	Thus, as a workaround, one has to write to the chain twice,
			 *	depending on the expected value of the 25th cell.
			 */
			if (num == 0) {
				/*	If we want to write a one to cell nr. 1, we first need to
				 *	write a 1 to the last cell and send 24 zeros afterwards.
				 */
				write_cmd((1ULL << ((sp->hw_const->ar_maxlen() - 24) +
				                    sp->hw_const->ar_chainnumberwidth())) |
				              c & mmw(sp->hw_const->ar_chainnumberwidth()),
				          del);
				write_cmd(0 | c & mmw(sp->hw_const->ar_chainnumberwidth()), del);
			} else {
				/*	If we want to write a 0 to cell nr. 24 (i.e. a 1 to another
				 *	cell than cell nr. 24), we first need to
				 *	write a 0 to all cells and send the real values afterwards.
				 */
				write_cmd(0 | c & mmw(sp->hw_const->ar_chainnumberwidth()), del);
				write_cmd((1ULL << ((sp->hw_const->ar_maxlen() - num) +
				                    sp->hw_const->ar_chainnumberwidth())) |
				              c & mmw(sp->hw_const->ar_chainnumberwidth()),
				          del);
			}
		}
	}
}

bool AnalogReadout::check_last(uint num, bool iszero)
{
	uint64_t data = 0;
	uint64_t val1, val2;
	uint64_t mask;
	bool result;

	if (sp->hw_const->revision() != 4) {
		val1 = iszero ? 0 : (1ULL << ((sp->hw_const->ar_maxlen() - 1 - num) +
		                              sp->hw_const->ar_chainnumberwidth()));
		mask = mmw(sp->hw_const->ar_maxlen()) << sp->hw_const->ar_chainnumberwidth();
		rcv_data(data);
		result = check(data, mask, val1);
	} else {
		mask = mmw(sp->hw_const->ar_maxlen()) << sp->hw_const->ar_chainnumberwidth();
		if (num == 24) {
			/*	If we want to write a 1 to cell nr. 24, we first need to
			 *	write a 1 to the first cell and send 24 zeros afterwards.
			 */
			val1 = 0;
			val2 = iszero ? 0 : (1ULL << ((sp->hw_const->ar_maxlen() - 1) +
			                              sp->hw_const->ar_chainnumberwidth()));

			rcv_data(data);
			rcv_data(data);
			result = check(data, mask, val1);
			result &= check(data, mask, val1);
			result &= check(data, mask, val2);
		} else if (num == 0) {
			val1 = 0;
			val2 = iszero ? 0 : (1ULL << ((sp->hw_const->ar_maxlen() - 1) +
			                              sp->hw_const->ar_chainnumberwidth()));

			rcv_data(data);
			rcv_data(data);
			result = check(data, mask, val1);
			result &= check(data, mask, val2);
			result &= check(data, mask, val1);
		} else {
			/*	If we want to write a 0 to cell nr. 24 (i.e. a 1 to another
			 *	cell than cell nr. 24), we first need to
			 *	write a 0 to all cells and send the real values afterwards.
			 */
			val1 = 0;
			val2 = iszero ? 0 : (1ULL << ((sp->hw_const->ar_maxlen() - 1 - num) +
			                              sp->hw_const->ar_chainnumberwidth()));

			rcv_data(data);
			rcv_data(data);
			result = check(data, mask, val1);
			result &= check(data, mask, val2);
			result &= check(data, mask, val1);
		}
	}
	return result;
}

void AnalogReadout::clear(int c, uint del)
{
	assert(del >= scdelay);
	write_cmd(c & mmw(sp->hw_const->ar_chainnumberwidth()), del);
	// spikey 4 workaround
	if (sp->hw_const->revision() == 4)
		write_cmd(c & mmw(sp->hw_const->ar_chainnumberwidth()), del);
}
