#include "hardwareConstantsRev4USB.h"
#include "hardwareConstantsRev5USB.h"

#include "logger.h"

class PySpikenetComm;

namespace spikey2
{

//! Spikenet communication encapsulates the transfer of information between the spikenetcontroller
//and the spikenet chips
class SpikenetComm
{
private:
	// to access some private stuff from python
	friend class ::PySpikenetComm;

public:
	// this logger is public, so that a testmode can access it
	// please refer to testmode.h for the reasen why there is
	// no logger in testmode class
	Logger& dbg;
	enum Commstate {
		ok,
		openfailed,
		eof,
		readfailed,
		writefailed,
		parametererror,
		readtimeout,
		writetimeout,
		pbreadempty,
		pbwriteinh,
		notimplemented
	};

private:
	Commstate state;


protected:
	uint verilogseed; // seed for verilog simulator
	uint acttime; // simulation time

public:
	const std::string classname;
	boost::shared_ptr<HardwareConstants> hw_const;
	static const IData emptydata;
	static const SBData emptysbdata;
	static const uint maxid = 15; // valid id range 0 to 14
	enum Mode {
		read = 1,
		write = 2,
		ctrl = 3,
		sync = 4,
		mutex = 0xff, // lower 16 are mutex mode selects
		nonblocking = 0x100
	}; // remaining bits are flag bits that can be combined
	enum Timing { basedelay = 1 }; // one packet per buscycle

	virtual void updateHwConst(int revision)
	{
		if (revision == 4) {
			hw_const = boost::shared_ptr<HardwareConstants>(new HardwareConstantsRev4USB());
		} else if (revision == 5) {
			hw_const = boost::shared_ptr<HardwareConstants>(new HardwareConstantsRev5USB());
		}
	}

	bool read_issued; // controls dummy read command generation

	SpikenetComm(string type)
	    : dbg(Logger::instance()), state(ok), classname(type), read_issued(false)
	{
		time_t now;
		int t = time(&now);

		// generate verilog seed
		srand(t);
		verilogseed = rand();

		// go back to system seed
		srand(randomseed);
	};

	// Send: transmits data to target, buffering may be applied
	virtual Commstate Send(Mode mode, IData data = emptydata, uint del = 0, uint chip = 0,
	                       uint syncoffset = 0) = 0;

	// Receive: fetches the topmost entry from the receive chain, empties send buffer before
	// receiving
	virtual Commstate Receive(Mode mode, IData& data, uint chip = 0) = 0;

	// makes sure all data in the Send-chain is transmitted to the target (empties send buffer), but
	// does NOT start execution
	virtual Commstate Flush(uint chip = 0)
	{
		static_cast<void>(chip);
		return notimplemented;
	}

	virtual void resetFlushed() {}

	// starts execution of previously flushed data
	virtual Commstate Run(void)
	{
		dbg(Logger::ERROR) << "SpikenetComm::Run not implemented!";
		return notimplemented;
	}

	// resets all queues, unsend and unreceived data is discarded (to make sure all data is
	// transmitted, call rec until eof is reached
	virtual Commstate Clear() { return notimplemented; };

	// async. sideband data transfer to spikey->firein
	virtual Commstate SendSBD(Mode mode, const SBData& data = emptysbdata, uint del = 0,
	                          uint chip = 0)
	{
		static_cast<void>(mode);
		static_cast<void>(data);
		static_cast<void>(del);
		static_cast<void>(chip);
		return notimplemented;
	}

	// async. sideband data, read adc values
	virtual Commstate RecSBD(Mode mode, SBData& data, uint chip = 0)
	{
		static_cast<void>(mode);
		static_cast<void>(data);
		static_cast<void>(chip);
		return notimplemented;
	}

	void setState(Commstate s) { state = s; };
	Commstate getState(void) { return state; };

	uint stime() { return acttime; };
	void setStime(uint ntime) { acttime = ntime; };
};

} // end of namespace spikey2
