// *** class Spikenet ***

#include "common.h" // library includes

#include "idata.h"
#include "sncomm.h"
#include "spikenet.h" //communication and chip classes
#include "ctrlif.h"
#include "synapse_control.h"
#include "pram_control.h"
#include "logger.h"

static log4cxx::LoggerPtr logger = log4cxx::Logger::getLogger("HAL.Net");

using namespace spikey2;

Spikenet::Spikenet(boost::shared_ptr<SpikenetComm> comm, uint chip) : dbg(::Logger::instance())
{
	hw_const = comm->hw_const;
	bus = comm;
	synapse_control = boost::shared_ptr<SynapseControl>(new SynapseControl(this));
	pram_control = boost::shared_ptr<PramControl>(new PramControl(this));
	chip_control = boost::shared_ptr<ChipControl>(new ChipControl(this));
	loopback = boost::shared_ptr<Loopback>(new Loopback(this));
	analog_readout = boost::shared_ptr<AnalogReadout>(new AnalogReadout(this));
	chipid = chip;
	rcvnum = 0;
	setCheckEvent(true);
	lastctrl = IData(true, true, true);
	;
}

// checks events received against stored values
// true if all have matched
bool Spikenet::checkEvents(uint& firsterr)
{
	LOG4CXX_ERROR(logger, "Spikenet::checkEvents not yet implemented for USB access!");
	return false;
}


// checks link pattern received against the passed value
// true if patterns are equal, otherwise false bits are stored in errmask.
bool Spikenet::checkPattern(uint64_t pat, uint64_t& errmask)
{
	IData p(0, (uint64_t)0);
	Receive(SpikenetComm::ctrl, p); // read received pattern
	LOG4CXX_TRACE(logger, "Pat: " << hex << pat << " - tpat: " << hex
	                              << (uint64_t)p.busTestPattern());
	errmask = mmw(2 * (hw_const->sg_direct_msb() + 1)) & (pat ^ p.busTestPattern());
	if (errmask == 0)
		return true;
	else
		return false;
}

float Spikenet::readAdc(SBData::ADtype type, uint del)
{
	SBData f;
	f.setAdc(type);
	SendSBD(SpikenetComm::read, f, del);
	//	cout<<" f:"<<hex<<&f;
	RecSBD(SpikenetComm::read, f);
	LOG4CXX_TRACE(logger, "Spikenet::readAdc: " << f);
	return f.ADvalue();
}

// *** END class Spikenet END ***
