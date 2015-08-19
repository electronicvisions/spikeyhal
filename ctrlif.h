#include "logger.h"
#include "sstream"

namespace spikey2
{

class Spikenet; // forward declaration

class data_block
{
private:
	data_block* prev;
	data_block* next;         //!< pointers to other data_blocks in list
	vector<uint64_t> payload; //!< contains data read from Spikey
	bool in_memory;           //!< payload contains valid data?
	uint startoffs;           //!< "global" offset relative to the currently built pbmem program
	uint size;                //!< size of this block

protected:
	::Logger& dbg;

public:
	data_block(data_block* prev) : prev(prev), dbg(::Logger::instance())
	{
		if (dynamic_cast<data_block*>(prev)) {
			prev->set_next(this);
			startoffs = prev->get_soff() + prev->get_size();
		} else {
			startoffs = 0;
		}
		size = 0;
		in_memory = false;
		payload.clear();
	};

	void set_prev(data_block* p) { prev = p; };
	void set_next(data_block* n) { next = n; };

	data_block* get_prev(void) { return prev; };
	data_block* get_next(void) { return next; };

	uint get_soff(void) { return startoffs; };
	uint get_size(void) { return size; };

	void add(void) { payload.push_back(static_cast<uint64_t>(0)); };

	void retrieve(void);
	uint64_t get(uint index);

}; // end class data_block

// abstract base class for all control interfaces in spikenet
class ControlInterface
{
protected:
	::Logger& dbg;

private:
protected:
	Spikenet* sp;

public:
	enum Timing {
		synramdelay = 10, // (ag): !!! increased from 6 - seemed too short - JS please check
		cidelay = 1
	}; // additional control interface delay to minimum delay
	ControlInterface(Spikenet* s) : dbg(::Logger::instance()), sp(s) {}
	virtual ~ControlInterface(){};

	void write_cmd(uint64_t, uint del); // issue write command to testbench
	void read_cmd(uint64_t, uint del); // issue read command to testbench
	SpikenetComm::Commstate rcv_idata(IData**); // check for received data
	SpikenetComm::Commstate rcv_data(uint64_t&); // check for received data
	void flush(void); // clear send buffers

	// type of control interface
	virtual uint get_cicmd(void) = 0; // no ci subtype for base class
	Spikenet* getSp(void) { return sp; };

	bool check(uint64_t data, uint64_t mask, uint64_t val);
};

} // end of namespace spikey2
