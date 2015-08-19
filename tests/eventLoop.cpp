#include <gtest/gtest.h>

#include "common.h"

#include "idata.h"
#include "sncomm.h"
#include "sc_sctrl.h"
#include "sc_pbmem.h"

#include "ctrlif.h"
#include "spikenet.h"

#include "pram_control.h"
#include "synapse_control.h"
#include "spikeyconfig.h"
#include "spikey.h"

#include <exception>
#include <time.h>

static log4cxx::LoggerPtr logger = log4cxx::Logger::getLogger("Tst.Elb");

namespace spikey2
{
uint bool2int(vector<bool> array, uint cr_width_tmp)
{ // copied from tmag_spikeyclass
	uint res = 0;
	for (uint i = 0; i < cr_width_tmp; i++)
		res |= (array[i] << i);
	return res;
}

bool checkEvents(uint& firsterr, SpikeTrain& tx, SpikeTrain& rx, boost::shared_ptr<SC_SlowCtrl> sc)
{ // copied from tmag_spikeyclass
	firsterr = 0; // initialize error position
	const vector<IData>* ev = sc->ev(0); // sent
	const vector<IData>& rcvd = rx.d; // received

	if (ev->size() != 0) {
		uint i, j, neuron, time, start;
		int smallestmatch;
		vector<bool> matched(rcvd.size(), false);
		smallestmatch = -1;

		for (i = 0; i < ev->size() - 1;
		     i++) { // ignore the last event as it can not be received anyway
			neuron = (*ev)[i].neuronAdr();
			time = (*ev)[i].time();
			LOG4CXX_TRACE(logger, "Looking for: n=" << hex << neuron << " t=" << time);

			if (smallestmatch > 0)
				start = (uint)smallestmatch;
			else
				start = 0;
			smallestmatch = -1; // in this run nothing has matched yet

			for (j = start; j < rcvd.size(); j++) {
				if (matched[j])
					continue; // skip already matched entries
				if (smallestmatch < 0)
					smallestmatch = j; // only store once
				const IData d = rcvd[j];
				LOG4CXX_TRACE(logger, "  |" << hex << d.neuronAdr() << ":" << d.time());

				if (time == d.time() && neuron == d.neuronAdr()) { // ok, found correct answer
					matched[j] = true;
					LOG4CXX_TRACE(logger, "  found in index: " << j);
					break;
				}
			}

			if (j >= rcvd.size()) {
				LOG4CXX_TRACE(logger, "  not found!");
				break;
			} // not found in received
		}

		if (i < ev->size() - 1) {
			firsterr = i;
			LOG4CXX_TRACE(logger, "  not found. Exiting.");
			return false;
		} // not every entry matched
		LOG4CXX_INFO(logger, "All events matched!");
		return true; // everything matched
	}
	LOG4CXX_WARN(logger, "No events!");
	return false;
}

TEST(HWTest, eventLoopback)
{
	/*
	 * Sends events to the chip.
	 * These events are inverted by the chip and sent back.
	 * Received events are compared to sent events.
	 */
	// parameters
	uint dist = 50; // maximum distance between spikes, the smaller the more frequent events, 40 is
	                // very small
	uint num = 10000; // number of events
	uint loops = 10; // number of trials

	// misc
	uint time_offset = 0x2000 * 16;
	uint time_event;
	SpikeTrain st_tx, st_rx;

	boost::shared_ptr<SpikenetComm> bus(new SC_Mem()); // playback memory interface
	boost::shared_ptr<Spikenet> chip(new Spikenet(bus));
	boost::shared_ptr<Spikey> sp(new Spikey(chip));

	boost::shared_ptr<SC_Mem> mem(boost::dynamic_pointer_cast<SC_Mem>(chip->bus));
	boost::shared_ptr<SC_SlowCtrl> sc = mem->getSCTRL(); // bus is SC_mem

	boost::shared_ptr<ChipControl> chip_control = chip->getCC(); // get pointer to chip control
	                                                             // class

	vector<bool> spikeystatus(bus->hw_const->cr_width(), false);
	// enable clocks to analog_chip
	spikeystatus[bus->hw_const->cr_anaclken()] = true;
	spikeystatus[bus->hw_const->cr_anaclkhien()] = true;
	chip_control->setCtrl(bool2int(spikeystatus, bus->hw_const->cr_width()), 20);

	unsigned long time_seed = time(NULL);
	LOG4CXX_INFO(logger, "random seed is " << time_seed);
	srand(time_seed);

	for (uint l = 0; l < loops; l++) {
		time_event = 0x200 << 5; // time offset
		// time_event = (sc->getSyncMax()+sc->getEvtLatency())<<5; //without any commands inbetween
		// this would be smallest time for first event

		st_tx.d.clear();
		st_rx.d.clear();
		// free playback memory
		if (l > 0)
			mem->Clear();
		// fill input vectors with events
		for (uint i = 0; i < num; i++) {
			time_event += rand() % dist;
			uint neuron_index = rand() % 192 + (rand() % 2 * 256);
			st_tx.d.push_back(IData::Event(neuron_index, time_event));
		}

		st_tx.d.push_back(IData::Event(0, time_event + time_offset)); // determines duration of
		                                                              // experiment

		// set event fifo resets before any event processing starts
		for (uint i = 0; i < bus->hw_const->event_outs(); i++)
			spikeystatus[bus->hw_const->cr_eout_rst() + i] = true;
		for (uint i = 0; i < bus->hw_const->event_ins(); i++)
			spikeystatus[bus->hw_const->cr_einb_rst() + i] = true;
		for (uint i = 0; i < bus->hw_const->event_ins(); i++)
			spikeystatus[bus->hw_const->cr_ein_rst() + i] = true;
		chip_control->setCtrl(bool2int(spikeystatus, bus->hw_const->cr_width()),
		                      bus->hw_const->fifo_reset_delay());

		// enable internal event loopback
		chip_control->setEvLb((1 << bus->hw_const->el_enable_pos()),
		                      bus->hw_const->el_depth() + 1); // turn on event loopback

		// release event fifo resets
		for (uint i = 0; i < bus->hw_const->event_outs(); i++)
			spikeystatus[bus->hw_const->cr_eout_rst() + i] = false;
		for (uint i = 0; i < bus->hw_const->event_ins(); i++)
			spikeystatus[bus->hw_const->cr_einb_rst() + i] = false;
		for (uint i = 0; i < bus->hw_const->event_ins(); i++)
			spikeystatus[bus->hw_const->cr_ein_rst() + i] = false;
		chip_control->setCtrl(bool2int(spikeystatus, bus->hw_const->cr_width()),
		                      bus->hw_const->fifo_reset_delay());

		// send events
		sp->sendSpikeTrain(st_tx);
		sp->Flush();
		sp->Run();
		sp->waitPbFinished();

		// receive events
		LOG4CXX_INFO(logger, "Starting recSpikeTrain()");
		sp->recSpikeTrain(st_rx);
		LOG4CXX_INFO(logger, "Sent " << st_tx.d.size() << ", and received " << st_rx.d.size()
		                             << " events.");

		// check events
		uint error = 0;
		LOG4CXX_INFO(logger, "Checking " << (st_tx.d.size() - 1) << " events in loop " << l << ":");
		bool success = checkEvents(error, st_tx, st_rx, sc);
		if (!success)
			LOG4CXX_ERROR(logger, "  failed at event no. " << error);
		EXPECT_EQ(true, success);
	}
}
}
