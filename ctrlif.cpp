// *** class ControlInterface ***

#include "common.h" // library includes

#include "idata.h"
#include "sncomm.h"
#include "ctrlif.h"

#include "spikenet.h"

static log4cxx::LoggerPtr logger = log4cxx::Logger::getLogger("HAL.Ifc");

using namespace spikey2;

void data_block::retrieve(void)
{
	if (!in_memory) {
		if (prev != NULL) {
			LOG4CXX_DEBUG(logger,
			              "data_block::retrieve: Trying to retrieve previous data_block in list.");
			prev->retrieve();
		}
		for (uint i = 0; i < size; i++) {
		}
	} else {
		LOG4CXX_DEBUG(logger, "data_block::retrieve: Data already in memory. Doing nothing.");
	}
}

void ControlInterface::flush()
{
	getSp()->Flush();
}

void ControlInterface::write_cmd(uint64_t cmd, uint del)
{
	IData d(get_cicmd(), cmd); // create data packet
	sp->Send(SpikenetComm::write, d, cidelay + del);
}

void ControlInterface::read_cmd(uint64_t cmd, uint del)
{
	IData d(get_cicmd(), cmd); // create data packet
	sp->Send(SpikenetComm::read, d, cidelay + del);
	sp->bus->read_issued = true;
}

SpikenetComm::Commstate ControlInterface::rcv_idata(IData** data)
{
	*data = new IData(get_cicmd(), static_cast<uint64_t>(0));
	SpikenetComm::Commstate result = sp->Receive(SpikenetComm::read, **data);
	LOG4CXX_TRACE(logger, "ControlInterface::rcv_idata: data (hex): " << hex << (*data)->data());
	return result;
}

SpikenetComm::Commstate ControlInterface::rcv_data(uint64_t& data)
{
	IData d(get_cicmd(), data); // create data packet
	SpikenetComm::Commstate result = sp->Receive(SpikenetComm::read, d);

	if (result != SpikenetComm::ok) {
		string msg = "ControlInterface::rcv_data: !ok result from Receive!";
		dbg(Logger::ERROR) << msg << Logger::flush;
		throw std::runtime_error(msg);
	} else {
		// check if correct data is in queue
		if ((d.cmd() & (0xf ^ sp->hw_const->ci_errori())) != get_cicmd()) {
			string msg = "ControlInterface::rcv_data: Invalid CI command type!";
			dbg(Logger::ERROR) << msg << " Expected: 0x" << hex << get_cicmd() << " - Got: 0x"
			                   << (d.cmd() & (0xf ^ sp->hw_const->ci_errori())) << Logger::flush;
			result = SpikenetComm::readfailed;
			throw std::runtime_error(msg);
		}

		// check for command error flag (i.e. Spikey was still busy when receiving the command)
		if (d.cmd() & sp->hw_const->ci_errori()) {
			string msg = "ControlInterface::rcv_data: Error flag set!";
			dbg(Logger::ERROR) << msg << " Command was 0x" << hex
			                   << (d.cmd() & (0xf ^ sp->hw_const->ci_errori())) << Logger::flush;
			result = SpikenetComm::readfailed;
			throw std::runtime_error(msg);
		}
	}

	LOG4CXX_TRACE(logger, "ControlInterface::rcv_data: data (hex): " << hex << d.data());
	data = d.data(); // copy back read data
	return result;
}

bool ControlInterface::check(uint64_t data, uint64_t mask, uint64_t val)
{
	if (rcv_data(data) != SpikenetComm::ok)
		return false;
	LOG4CXX_TRACE(logger, "ControlInterface::check " << hex << setw(16) << setfill('0') << data
	                                                 << " " << setw(16) << setfill('0') << val
	                                                 << " " << setw(16) << setfill('0') << mask);
	if (!((data & mask) == val)) {
		LOG4CXX_TRACE(logger, hex << "rcv " << (data & mask) << " val " << (val & mask)
		                          << " errmask ");
		std::ostringstream stream;
		binout(stream, ((data ^ val) & mask), 52);
		LOG4CXX_TRACE(logger, "ControlInterface::check():error mask" << stream.str());
	}
	return (data & mask) == val;
}
// *** END class ControlInterface END ***
