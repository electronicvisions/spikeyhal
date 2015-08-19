// realizes the communication via the nathan system's slow control

#include "common.h" // library includes
#include "idata.h"
#include "sncomm.h"
#include "spikenet.h"
#include "sc_sctrl.h"
#include "sc_pbmem.h"

#include <boost/date_time/posix_time/posix_time.hpp>

static log4cxx::LoggerPtr logger = log4cxx::Logger::getLogger("HAL.SCM");

using namespace spikey2;

// init this global const... ugly...
const IData SpikenetComm::emptydata;

SC_Mem::SC_Mem(uint time, std::string workstation)
    : SpikenetComm("sc_pbmem"), inrec(false), insend(false), flushed(false)
{
	sc = boost::shared_ptr<SC_SlowCtrl>(new SC_SlowCtrl(time, workstation));
	updateHwConst(sc->getChipVersion());

	sc->spyctrl->write(0xb, 0); // make sure, fpga loopback is deactivated

	initChip();
	intClear();
}

// all data is copied to the playback memory
// data is sent until receive is called
// in this case the current content of the playback memory is sent to Spikey to make the expected
// receive data available

SpikenetComm::Commstate SC_Mem::Send(Mode mode, IData data, uint del, uint chip, uint syncoffset)
{
	uint syncdel;

	if (flushed) {
		LOG4CXX_ERROR(logger,
		              "SC_Mem::Send: Incomplete Flush! Issue startPlayback() before next send!");
		return writefailed;
	}

	if (inrec) {
		if (rmvadr < rstart) {
			LOG4CXX_ERROR(logger,
			              "SC_Mem::Send: You tried to send while not all data from SDRAM read!");
			return writefailed;
		}
		LOG4CXX_TRACE(logger, "##### pushing sendidx " << sendidx);
		skip.push(sendidx);
		inrec = false;
		LOG4CXX_TRACE(logger, "SC_Mem::Send: unset inrec");
	}

	if (!insend) { // start send
		sc->setPbradr(wadr); // wadr is the first empty address in playbackmem
		insend = true;
	}

	// continue send
	switch (mode & mutex) {
		case write:
		case read:
			if (data.event()) { // events are not packed at the moment
				pbdat.push_back(data);
			} else {
				pbTrans(); // send collected events
				sc->pbCI(mode, data, del + basedelay);
				if ((mode & mutex) == read) {
					sendidx++;
					// dbg(::Logger::DEBUG3) << "SC_Mem::Send sendidx now: " << sendidx;
				}
				setStime(stime() + basedelay + del); // keep track of simulation time only when
				                                     // performing ci accesses
				// (ag): systime increments due to event packing are calculated within sc-pbEvt!
			}
			break;
		case ctrl: // call sc send function
			return sc->Send(mode, data, del, chip);
		case sync:
			// send collected events
			pbTrans();

			// insert dummy read before sync/spikes.
			LOG4CXX_TRACE(logger, "SC_Mem::Send: inserting dummy read before sync");
			IData dummy(hw_const->ci_loopbacki(), static_cast<uint64_t>(0));

			// insert one dummy read to transmit last result before receive starts
			// -> wait with max chain delay to receive the data before the FPGA FSM goes into sync
			sc->pbCI(SpikenetComm::read, dummy, sc->getMaxChainDelay());
			sendidx++;
			skip.push(sendidx);

			if (del == 0)
				syncdel = sc->getSyncMax();
			else
				syncdel = basedelay + del;
			sc->pbSync(stime(), syncdel, syncoffset);
			setStime(stime() + syncdel + (syncoffset >> 1));
			break;
	}
	LOG4CXX_TRACE(logger, hex << "Time after send: " << stime());
	return ok;
}

// receives from the result ram
// each call to receive returns one entry from the result ram
// this should be similar to sc_trans behaviour
// events are also sorted into a separate event buffer like in sc_trans

SpikenetComm::Commstate SC_Mem::Receive(Mode mode, IData& data, uint chip)
{
	static_cast<void>(mode);

	if (flushed) {
		LOG4CXX_ERROR(logger, "SC_Mem::Receive: Incomplete flush of playback memory!");
		return readfailed;
	}

	if (insend) { // end send
		LOG4CXX_ERROR(logger,
		              "SC_Mem::Receive: Called while still sending data to playback memory!");
		return readfailed;
	}

	if (!inrec) { // start rec
		LOG4CXX_TRACE(logger, "SC_Mem::Receive: starting inrec (=true)");
		inrec = true;
		// in case the rstart pointer has an odd value in this case, the FPGA has automatically
		// added
		// a dummy write command to the playback receive memory to finish the final DDR cycle.
		// -> Increase this pointer to skip that dummy.
		if (radr % 2) {
			LOG4CXX_TRACE(logger, "SC_Mem::Receive: increment read address from 0x"
			                          << hex << radr << " to 0x" << (radr + 1)
			                          << " to skip dummy entry added by FPGA");
			radr++;
		}
	}

	// continue receive
	// receive data, if possible in chunks, store all data in readbuf
	// but process only oldest entry in readbuf
	// radr points on oldest entry
	while (true) {
		bool wtim0, readempty, writeinh, writefull;
		bool idle = false;

		// =false if still unused data in readbuf
		// =true if all data read => radr - 1 == rmvadr
		// in first call of Receive rmvadr<radr on purpose
		while (rmvadr <= radr) {
			// (ag): The timeout counter wtim and the state of the playback memory fsm are checked,
			// separately.
			// Therefore, this practice should also work for fast Slow-Control interfaces.

			// comment in for "safer" mode
			// poll playback memory idle flag
			waitIdle();

			sc->getCurWadr(rmvadr, wtim0, idle, readempty, writeinh, writefull);
			rmvadr += roffs; // getCurWadr returns relative address in respective physical memory
			                 // module

			// check and evaluate error bits
			if (readempty) {
				LOG4CXX_ERROR(logger, "SC_Mem::Receive: FIFO FPGA memory->Spikey ran empty!");
				return pbreadempty;
			}
			if (writeinh) {
				LOG4CXX_ERROR(logger, "SC_Mem::Receive: FIFO Spikey->FPGA memory had overflow!");
				return pbwriteinh;
			}
			if (writefull) {
				LOG4CXX_ERROR(logger, "SC_Mem::Receive: Spikey->FPGA record memory ran full!");
				return readfailed;
			}

			// if IDLE and rmvadr > radr, there is new data to be read -> BREAK
			if (idle) {
				LOG4CXX_TRACE(logger, "SC_Mem::Receive: FPGA idle, checking addresses...");
				if (rmvadr < radr) {
					if (rmvadr == radr - 1)
						return eof; // no data received since last call during execution
						            // (idle=false)
					else
						return readfailed; // something has gone wrong in the controller
				} else if (rmvadr == radr) {
					LOG4CXX_WARN(logger, "SC_Mem::Receive: recheck timeout evaluation");
					return eof; // !!! recheck timeout evaluation !!!
				} else {
					LOG4CXX_TRACE(logger,
					              "SC_Mem::Receive: Playback Memory finished. Reading back...");
					break; // playback memory has finished. Read everything back.
				}
			} else if (!wtim0) {
				LOG4CXX_TRACE(logger, "SC_Mem::Receive: Waiting for Playback Memory... (wtime!=0)");
				continue; // playback memory not finished and data not guaranteed to be valid.
				          // Continue wait.
			}

			if (rmvadr == radr - 1) {
				LOG4CXX_TRACE(logger,
				              "SC_Mem::Receive: received no more valid data during execution");
				continue;
			} else if (rmvadr < radr - 1) {
				LOG4CXX_ERROR(
				    logger, "SC_Mem::Receive: inconsistent address counters for rmvadr and radr!");
				return readfailed;
				// playback memory not idle, but rmvadr > radr - 1 -> new data, BREAK and read back
			} else {
				LOG4CXX_TRACE(
				    logger,
				    "SC_Mem::Receive: Read back valid data during Playback Memory execution");
				break; // Playback memory contains valid data. Read back.
			}
		}

		uint64_t rdata = 0;

		// start receiving from here
		if (rmvadr > recupto) {
			LOG4CXX_TRACE(logger, "SC_Mem::Receive: readBlock from 0x"
			                          << hex << recupto << ", size (int): " << (rmvadr - recupto));
			uint maxchunksize = sc->getMaxChunkSize();
			uint chunks = ((rmvadr - recupto) * 2) / maxchunksize + 1;
			uint chunkstart = recupto * 2; // spikey addresses (rmvadr, radr, ...) are 64bit
			                               // aligned, but vbuf 32bit
			uint chunksize = 0;
			for (uint chunk = 0; chunk < chunks; chunk++) {
				// cut addresses to fit in chunks
				uint virtChunksize = (rmvadr - recupto) * 2 - (chunkstart - recupto * 2);
				if (virtChunksize > maxchunksize)
					chunksize = maxchunksize;
				else
					chunksize = virtChunksize;
				// dbg(::Logger::DEBUG3) << hex << "SC_Mem::Receive: recupto: " << recupto << ",
				// rmvadr: " << rmvadr << ", chunks: " << chunks << ", chunkstart: " << chunkstart
				// << ", chunksize: " << chunksize << endl;
				Vbufuint_p recbuf = sc->mem->readBlock(chunkstart, chunksize);
				for (uint i = 0; i < (chunksize / 2); i++) {
					uint64_t temp = 0;
					temp = (uint64_t)recbuf[2 * i] & 0xffffffff;
					temp |= (uint64_t)recbuf[2 * i + 1] << 32;
					readbuf.push_back(temp);
				}
				chunkstart = chunkstart + chunksize;
			}
			recupto = rmvadr;
		}
		rdata = readbuf[radr - (roffs + adcsize)];
		LOG4CXX_TRACE(logger, "SC_Mem::Receive: read from memory: address=0x"
		                          << hex << radr << ", data=0x" << hex << rdata
		                          << ", data valid up to 0x" << hex << rmvadr);
		++radr; // increment radr only by one for each call of Receive, other data is buffered in
		        // readbuf

		if (radr > rstart + rsize - 1) {
			string msg = "SC_Mem::Receive: radr out of range!";
			dbg(::Logger::ERROR) << msg << Logger::flush;
			throw std::runtime_error(msg);
		}

		uint evmask = 7;
		sc->translate(rdata, data, evmask);
		if (data.isEmpty())
			continue; // received a system time update event, skipping
		if (data.isEvent()) {
			rcvev[chip].push_back(data); // first event
			// get all three events from the rdata word
			while (evmask) {
				sc->translate(rdata, data, evmask);
				if (data.isEvent())
					rcvev[chip].push_back(data);
			}
			continue;
		}

		// skip recorded dummy results
		if (!skip.empty() && skip.front() == recidx) {
			LOG4CXX_TRACE(logger, "SC_Mem::Receive: skip read number: " << skip.front());
			skip.pop();
			recidx++;
			continue;
		}
		// received all data
		recidx++;
		break;
	}
	return ok;
}

// ends sending
SpikenetComm::Commstate SC_Mem::Flush(uint chip = 0)
{
	LOG4CXX_DEBUG(logger,
	              "SC_Mem::Flush: flushing playback memory (transferring data to hardware system)");

	// first check for memory overflow:
	if (sc->pbradr() > (wsize - wstart)) {
		string msg = "SC_Mem::Flush: playback memory would overflow, NOT flushing!";
		dbg(::Logger::ERROR) << msg << Logger::flush;
		throw std::runtime_error(msg);
	}

	pbTrans(); // send collected events
	// check for redundant flush
	if (sc->pbradr() == wadr) {
		LOG4CXX_WARN(logger, "SC_Mem::Flush: playback memory already flushed!");
		return ok;
	}

	if (insend && read_issued) { // dummy read is only issued if real read commands have been issued
		LOG4CXX_TRACE(
		    logger, "SC_Mem::Flush: inserting dummy read to retrieve result of last read command");
		IData dummy(hw_const->ci_loopbacki(), static_cast<uint64_t>(0));
		sc->pbCI(SpikenetComm::read, dummy); // insert one dummy read to transmit last result before
		                                     // receive starts
		sendidx++;
		read_issued = false; // reset
	}

	// make pbmem run at least as long as last data has been transferred through daisy chain
	sc->pbEvtdel(sc->getMaxChainDelay());

	// odd values are actually impossible since FPGA completes each pbmem cycle with an even number
	// of accesses, automatically!
	if (rstart % 2) {
		string msg = "SC_Mem::Flush: rstart pointer has an odd value! Something must've gone wrong "
		             "in the FPGA!";
		dbg(::Logger::ERROR) << msg << Logger::flush;
		throw std::runtime_error(msg);
	}

	rstart = sc->curwadr() + roffs; // set address of result memory, from where to read data in next
	                                // flush

	sc->setupPlayback(rstart - roffs, wadr, chip);
	savePbContent(); // it's ok to save pointer after setupPlayback() has been called.
	sc->resetPlayback();

	// curwadr is valid after resetPlayback
	LOG4CXX_TRACE(logger, "SC_Mem::Flush: receive addr (curwadr): 0x"
	                          << hex << sc->curwadr() << "; PBM start addr (wadr): 0x" << hex
	                          << wadr << "; PBM read start addr (radr): 0x" << hex << radr);

	flushed = true;
	insend = false;
	inrec = false;
	LOG4CXX_TRACE(logger, "SC_Mem::Flush: unset inrec, insend (and flushed=true)");

	return ok;
}

SpikenetComm::Commstate SC_Mem::Run()
{
	LOG4CXX_DEBUG(logger, "SC_Mem::Run: starting playback memory (triggering network emulation)");

	sc->startPlayback();
	flushed = false;

	/* optional debug functionality:
	// read back number of executed sync commands and the sync_error flag:
	uint syncs(0);
	getSCTRL()->getNumOfSyncs(syncs);
	cout << "*** SC_Mem::Flush: Read number of syncs: " << dec << syncs << ", sync_error flag: " <<
	getSCTRL()->getSyncError() << endl; */

	return ok;
}

// hack!
void SC_Mem::savePbContent()
{
	saved_skip = skip;
	sc->getCurRadr(laststartadr);
	sc->getNumRead(lastnumread);
	// cout << "save lsa " << laststartadr << ", lnr " << lastnumread << endl;
}

void SC_Mem::recallPbContent()
{
	skip = saved_skip;
	sc->setRadr(laststartadr);
	sc->setNumRead(lastnumread);
	// cout << "read lsa " << laststartadr << ", lnr " << lastnumread << endl;
}

void SC_Mem::reFlush()
{
	bool wtim0, readempty, writeinh, writefull;
	bool idle = false;
	while (!idle)
		sc->getCurWadr(rmvadr, wtim0, idle, readempty, writeinh, writefull);

	if (!insend && !inrec) {
		recallPbContent();
		sc->resetPlayback(); // (ag): set all addresses to startup and ramfsm to IDLE.
		sc->startPlayback();

		while (!idle)
			sc->getCurWadr(rmvadr, wtim0, idle, readempty, writeinh, writefull);
	} else {
		LOG4CXX_ERROR(
		    logger, "SC_Mem::reFlush: additional transfers issued before reFlush! Doing nothing!");
	}
}

// resets all queues, unsend and unreceived data is discarded
// to make sure all data is transmitted, call rec until eof is reached
SpikenetComm::Commstate SC_Mem::intClear()
{
	inrec = false;
	insend = false;
	flushed = false;
	read_issued = false;
	readbuf.clear();

	sendidx = 0;
	recidx = 0;
	while (!skip.empty())
		skip.pop();
	skip.push(sendidx); // skip first data package, which is generated by dummy read

	wstart = 0;
	rstart = roffs + adcsize;
	wadr = wstart;
	radr = rstart;
	rmvadr = radr;
	recupto = rmvadr;

	sc->setContIdleOn(); // (ag): the default as no single idle event packets can be sent via pb
	                     // mem.
	sc->setPbradr(wadr);
	sc->setWadr(radr - roffs);
	sc->resetPlayback(); // has been changed in spikey_sei

	LOG4CXX_TRACE(logger, "SC_Mem::intClear: cleared inrec and insend and flushed");
	return ok;
}

// initialize Spikey with correct reset sequence, write optimum delay values
SpikenetComm::Commstate SC_Mem::initChip()
{

	IData d(true, true, false);          // Reset, PLL_Reset, CI_Mode
	IData p(false, false, false, false); // phsel bits

	// turn off idle event packet generation and reset ios
	sc->setContIdleOff();
	sc->spydc->write(0, 0x30000000);

	this->Send(SpikenetComm::ctrl, d);

	d.setPllreset(false);
	this->Send(SpikenetComm::ctrl, d);
	d.setPllreset(true);
	this->Send(SpikenetComm::ctrl, d);
	d.setPllreset(false);
	this->Send(SpikenetComm::ctrl, d);

	d.setReset(false);
	this->Send(SpikenetComm::ctrl, d);

	// set CAL and RST input of IODELAys once to initialize
	sc->spydc->write(0, 0x21000000);
	sc->spydc->write(0, 0x30000000);

	// set phase select bits
	this->Send(SpikenetComm::ctrl, p);

	// set pattern for iodelay tuning
	sc->setDout0(0x15555);
	sc->setDout1(0x15555);
	sc->setDout2(0x15555);
	sc->setDout3(0x15555);

	d.setCimode(true);
	this->Send(SpikenetComm::ctrl, d);

	sc->spydc->write(0, 0x30000000);
	for (uint d = 0; d < sc->getDelayFpgaIn(); d++)
		sc->spydc->write(0, 0x2013ffff);

	d.setCimode(false);
	this->Send(SpikenetComm::ctrl, d);

	sc->spydc->write(0, 0x00000000);

	d.setReset(true);
	this->Send(SpikenetComm::ctrl, d);
	d.setReset(false);
	this->Send(SpikenetComm::ctrl, d);

	LOG4CXX_TRACE(logger, "SC_Mem::initChip: Chip and link have been reset and initialized");
	return ok;
}

// copy pbdat to pbmem according to inci state
inline void SC_Mem::pbTrans()
{
	uint newstime;
	if (!pbdat.empty()) {
		sc->pbEvt(pbdat, newstime, stime());
		pbdat.clear();
		setStime(newstime);
	}
}

void SC_Mem::waitIdle()
{
	bool wtim0, readempty, writeinh, writefull;
	bool idle = false;

	LOG4CXX_TRACE(logger, "Waiting for playback memory to become IDLE...");

	uint polls = 0;
	uint diff_ms_counter = 0;
	boost::posix_time::ptime time_start = boost::posix_time::microsec_clock::local_time();
	while (!idle) {
		sc->getCurWadr(rmvadr, wtim0, idle, readempty, writeinh, writefull);
		polls++;
		if (polls % 100 == 0) {
			boost::posix_time::ptime time_now = boost::posix_time::microsec_clock::local_time();
			boost::posix_time::time_duration diff = time_now - time_start;
			long diff_ms = diff.total_milliseconds();
			if (diff_ms / 1000 > diff_ms_counter) {
				dbg(Logger::WARNING) << "Waited " << diff_ms << "ms (" << polls
				                     << " polls) for FPGA to get idle (this may happen for high "
				                        "throughput of spikes)";
				diff_ms_counter++;
			}
			if (diff_ms >= max_poll_time) {
				string msg = "SC_Mem::waitIdle: Reached max number of polls!";
				dbg(Logger::ERROR) << msg << Logger::flush;
				throw msg;
			}
		}
	}

	wadr = sc->pbradr(); // update pointer to free playback mem

	LOG4CXX_TRACE(logger, "waitIdle: PBM now idle: rmvadr = 0x" << hex << rmvadr << ", radr = 0x"
	                                                            << hex << radr - (roffs + adcsize)
	                                                            << ", wadr = 0x" << hex << wadr);
}
