#include "common.h"
#include "idata.h"
#include <cassert>
#include "logger.h"

static log4cxx::LoggerPtr logger = log4cxx::Logger::getLogger("HAL.IDa");

using namespace spikey2;

namespace spikey2
{
// stream idata structs
ostream& operator<<(ostream& o, const SBData& d)
{
	switch (d.payload()) {
		case SBData::t_empty:
			return o << "ID:--"
			         << " ";
		case SBData::t_fire:
			return o << "ID:fi"
			         << " ";
		case SBData::t_dac:
			return o << "ID:da v:" << dec << setw(4) << setfill(' ') << d.ADvalue() << " ";
		case SBData::t_adc:
			return o << "ID:ad v:" << dec << setw(4) << setfill(' ') << d.ADvalue() << " ";
		default:
			return o << "ID:illegal";
	}
}

// stream sbdata structs
ostream& operator<<(ostream& o, const IData& d)
{
	switch (d.payload()) {
		case IData::t_empty:
			return o << "ID:--";
		case IData::t_ci:
			return o << "ID:ci C:" << hex << setw(1) << setfill('0') << d.cmd() << " D:" << d.data()
			         << " ";
		case IData::t_event:
			return o << "ID:ev A:" << hex << setw(3) << setfill('0') << d.neuronAdr()
			         << " T:" << setw(3) << d.time() << " ";
		case IData::t_control:
			return o << "ID:dl r:" << d.reset() << " pr:" << d.pllreset() << " ci:" << d.cimode()
			         << " vr:" << d.vrest_gnd() << " vm:" << d.vm_gnd() << " ib:" << d.ibtest_meas()
			         << " pe:" << d.power_enable() << " r0:" << d.rx_clk0_phase()
			         << " r1:" << d.rx_clk1_phase() << " ";
		case IData::t_bustest:
			return o << "ID:bt"
			         << " ";
		case IData::t_delay:
			return o << "ID:ct"
			         << " ";
		default:
			return o << "ID:illegal ";
	}
}

istream& operator>>(istream& i, IData& d)
{
	string type;
	i.ignore(256, ':');
	if (!(i >> type))
		return i;
	if (type == "ev") {
		uint n, t;
		i.ignore(256, ':');
		i >> hex >> n;
		i.ignore(256, ':');
		i >> hex >> t;
		d.setPayload(IData::t_event);
		d.setNeuronAdr() = n;
		d.setTime() = t;
	} else {
		LOG4CXX_ERROR(
		    logger, "De-serialization of non-event-type IData objects is currently not supported.");
		LOG4CXX_ERROR(logger, "Got type " << type);
		assert(false);
	}
	return i;
}
}

vector<bool>& SBData::emptyfire()
{
	static vector<bool>* dummy = NULL;
	if (!dummy)
		dummy = new vector<bool>();
	return *dummy;
}
