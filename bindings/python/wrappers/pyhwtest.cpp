#define PY_ARRAY_UNIQUE_SYMBOL hal
#define NO_IMPORT_ARRAY

#include "spikey2_lowlevel_includes.h"
#include "pyhwtest.h"


int getch(void);
int kbhit(void);

using namespace std;
using namespace spikey2;


/** Init objects and reset error */
void PySpikeyTM::initMem(uint busModel, bool withSpikey)
{
	error = "";

	switch (busModel) {
		case 0:
			bus = boost::shared_ptr<SC_SlowCtrl>(new SC_SlowCtrl(startTime));
			break;
		case 1:
			bus = boost::shared_ptr<SC_Mem>(new SC_Mem(startTime));
			break;
		default:
			cout << "initMem(): This busModel is NOT defined." << endl;
			break;
	}

	if (bus->getState() == SpikenetComm::openfailed) {
		cout << "Open failed!" << endl;
	}

	chip.push_back(boost::shared_ptr<Spikenet>(new Spikenet(bus, 0))); // chip number 0

	if (withSpikey)
		sp = boost::shared_ptr<Spikey>(new Spikey(chip[0]->bus, 10, 0));
}


/** Starts all testmodes sequentially and returns false as soon as one failes. */
bool PySpikeyTM::test()
{
	// hier werden die einzelnen Tests aufgerufen

	cout << "PySpikeyTM::test() runs..." << endl;

	/* Vorgehen:
	 * - dinit -r => schrottet laufendes, weil alles resetted
	 * - linktest (bq) => schrottet laufendes
	 * - event loopback - prob bzgl. update config?
	 */

	if (link() == false) {
		cout << "link()-test failed." << endl;
		return false;
	}
	cout << "link()-test succeeded." << endl;

	if (parameterRAM() == false) {
		cout << "parameterRAM()-test failed." << endl;
		return false;
	}
	cout << "parameterRAM()-test succeeded." << endl;

	if (eventLoopback() == false) {
		cout << "eventLoopback()-test failed." << endl;
		return false;
	}
	cout << "eventLoopback()-test succeeded." << endl;

	cout << "PySpikeyTM::test(): All checks passed." << endl;
	return true; // all checks passed
}


/** Checks link.
 *
 *  Schema:
 *         ________________
 *    Din |  _          _  | Dout (== output from FPGA)
 *   <----|-|_| Link 0 |_|-|<----  1 bit FRAMEBIT (needs to change within first 2 bitclks <=> 1 Clk)
 *    8+1 |  _          _  |  8+1  8 bit DATA (Dout0 represents link 0, bitclk 0; Dout1 bitclk 1)
 *   <----|-|_| Link 1 |_|-|<----  same here, link 1
 *    8+1 |________________|  8+1
 *
 * Link 0: 2*(16+2)bit
 *   Dout0: 0x3feff == 1 1111 1111 0 1111 1111 (lower bits, time: right-to-left, 2 bitclk-cyles)
 *   Dout1: 0x    0 == 0 0000 0000 0 0000 0000 (upper bits, same here)
 *
 *   FRAMEBIT:  _'__
 *   DATA:      1100
 *
 * Same holds for link 1. */
bool PySpikeyTM::link()
{
	initMem(0, false); // create objects :)

	bool existsError = false;

	// theoretical optimum delay values for spikey's delay lines (from timing analysis)
	// values by Andi (04.02.07) / original (5.3.07) spikey2
	// uint delays[36]={3,3,3,3,4,2,1,3,3,1,2,1,2,3,3,5,5,3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

	boost::shared_ptr<SC_SlowCtrl> sc = boost::dynamic_pointer_cast<SC_SlowCtrl>(chip[0]->bus);

	uint pat18a, pat18b, pat18c, pat18d; // variable storing link 0, 1 and bitclock 0, 1 data


	//	chip[0]->setChip(false, true, false, false, false, 10);
	/* setChip(parameters):
	 *   reset
	 *   cimode: interface testmode (reset after toggle!)
	 *   bsmode
	 *   pllreset
	 *   pllbypass: if pll is non-functional use external pll
	 *   delay: clk-cycles for command (reasonable value: 10 [clks])
	 */


	error += "           LINK-TEST          \n";
	error += "        ________________      \n";
	error += "   Din |  _          _  | Dout\n";
	error += "  <----|-|_| Link 0 |_|-|<----\n";
	error += "   8+1 |  _          _  |  8+1\n";
	error += "  <----|-|_| Link 1 |_|-|<----\n";
	error += "   8+1 |________________|  8+1\n";

	// transmit set cmds;
	sc->setDout0(0x3feff);
	sc->setDout1(0);
	sc->setDout2(0x3feff);
	sc->setDout3(0);

	// transmit read cmds
	sc->getDin0(pat18a);
	sc->getDin1(pat18b);
	sc->getDin2(pat18c);
	sc->getDin3(pat18d);


	// check if sent != recv'd data
	if ((0x3feff != pat18a) || (0 != pat18b) || (0x3feff != pat18c) || (0 != pat18d)) {
		error += "link()-test: sent (Dout) != recv'd (Din) data!\n";
		existsError = true;
	}

	/* now setting inverted data signal / framebit (special handling => XOR within first 2 bitclks)
	 * Data:     0011
	 * Framebit: '___ */
	sc->setDout0(0x100);
	sc->setDout1(0x1feff);
	sc->setDout2(0x100);
	sc->setDout3(0x1feff);

	// read cmds
	sc->getDin0(pat18a);
	sc->getDin1(pat18b);
	sc->getDin2(pat18c);
	sc->getDin3(pat18d);

	if ((0x100 != pat18a) || (0x1feff != pat18b) || (0x100 != pat18c) || (0x1feff != pat18d)) {
		error += "link()-test: sent (Dout) != recv'd (Din) data!\n";
		existsError = true;
	}


	// now checking random data with "random" framebits (constraint: XOR(bitclk0,1) = 1 :)
	uint rpat1, rpat2, rpat3, rpat4, rpat5, rpat6;
	int wrongBits1 = 0, wrongBits2 = 0;

	srand(now); // set random seed to something known

	for (uint i = 0; i < loops[0]; ++i) { // loops[0] = # linktest loops :)
		rpat1 = rand() % 262144;          // link 0 bitclk 0 modulo 18 bit
		rpat2 = rand() % 262144;          // same, bitclk 1
		rpat3 = rand() % 2;               // link 0 upper framebit high
		rpat4 = rand() % 262144;          // link 1 masked by 18 bit
		rpat5 = rand() % 262144;          // same, bitclk 1
		rpat6 = rand() % 2;               // link 1 upper framebit high

		rpat1 = (rpat1 & 0x1feff); // (0) 1111 1111 0 1111 1111 => link 0 bitclk 0 framebits low
		rpat2 = (rpat2 & 0x1feff); // (0) 1111 1111 0 1111 1111 => link 0 bitclk 1 framebits low
		// link 0 framebits ____

		rpat4 = (rpat4 & 0x1feff); // (0) 1111 1111 0 1111 1111 => link 1 bitclk 0 framebits low
		rpat5 = (rpat5 & 0x1feff); // (0) 1111 1111 0 1111 1111 => link 1 bitclk 1 framebits low
		// link 1 framebits ____

		if (rpat3 == 1)
			rpat1 = (rpat1 | 0x20000); // 1 XXXXXXXX X XXXXXXXX => link 0 upper framebit 1
		else
			rpat1 = (rpat1 | 0x100); // X XXXXXXXX 1 XXXXXXXX => link 0 lower framebit 1

		if (rpat6 == 1)
			rpat4 = (rpat4 | 0x20000); // 1 XXXXXXXX X XXXXXXXX => link 1 upper framebit 1
		else
			rpat4 = (rpat4 | 0x100); // X XXXXXXXX 1 XXXXXXXX => link 1 lower framebit 1

		// set bist
		sc->setDout0(rpat1);
		sc->setDout1(rpat2);
		sc->setDout2(rpat4);
		sc->setDout3(rpat5);

		// read cmd
		sc->getDin0(pat18a);
		sc->getDin1(pat18b);
		sc->getDin2(pat18c);
		sc->getDin3(pat18d);

		// mark wrong bits
		if (rpat1 != pat18a)
			wrongBits1 = wrongBits1 | (rpat1 ^ pat18a);
		if (rpat2 != pat18b)
			wrongBits1 = wrongBits1 | (rpat2 ^ pat18b);
		if (rpat4 != pat18c)
			wrongBits2 = wrongBits2 | (rpat4 ^ pat18c);
		if (rpat5 != pat18d)
			wrongBits2 = wrongBits2 | (rpat5 ^ pat18d);
	}

	if ((wrongBits1 != 0) || (wrongBits2 != 0)) {
		std::ostringstream buffer;
		buffer << "Wrong bits link 0: " << wrongBits1 << "\n"
		       << "Wrong bits link 1: " << wrongBits2 << "\n";
		error += buffer.str();
		existsError = true;
	}

	if (existsError)
		return false;
	return true;
} // end of link()


/** Checks parameter RAM.
 * - period register
 * - PRAM containing "Stromspeicher"
 * - LUT containing timing data for "Stromspeicher" */
bool PySpikeyTM::parameterRAM()
{
	initMem(1, false); // create objects :)
	bool existsError = false;

	boost::shared_ptr<PramControl> pram_control = chip[0]->getPC(); // get pointer to parameter
	                                                                // control class

	uint del = 4, // worst case delay/distance between ci accesses
	    num = 3072;

	// 1. test
	pram_control->write_period(45); // pr_period_width = 8 bit => TODO: rand() % 256
	pram_control->read_period();

	srand(now);
	// writing to parameterRAM
	for (uint i = 0; i < num; ++i) // i counts to num: 3072 addresses are available
		pram_control->write_pram(
		    i % (1 << bus->hw_const->pr_ramaddr_width()),      // Physical address SRAM/digital
		                                                       // containing data*
		    rand() % (1 << bus->hw_const->pr_paraddr_width()), // for physical address in analog
		                                                       // part.
		    rand() % (1 << bus->hw_const->pr_dacval_width()),  // (*) storage value (Stromspeicher)
		    rand() % (1 << bus->hw_const->pr_lutadr_width()),  // address for LUT, containing timing
		                                                       // data
		    del);                                              // command delay
	// read cmds
	for (uint i = 0; i < num; i++)
		pram_control->read_pram(i, del);

	srand(now);
	// writing to LUT
	for (uint i = 0; i < 16; i++) // 16 luts are available
		pram_control->write_lut(
		    i % (1 << bus->hw_const->pr_lutadr_width()),       // address of lookup table
		    rand() % (1 << bus->hw_const->pr_luttime_width()), // timing data (=> how long a current
		                                                       // is applied)
		    rand() % (1 << bus->hw_const->pr_lutboost_width()),  // lutboost?
		    rand() % (1 << bus->hw_const->pr_lutrepeat_width()), // lutrepeat?
		    rand() % (1 << bus->hw_const->pr_lutstep_width()),   // lutstep?
		    del);                                                // command delay
	// read cmds
	for (uint i = 0; i < 16; i++)
		pram_control->read_lut(i, del);


	// checking read-back values
	if (!pram_control->check_period(45)) {
		error += "parameterRAM(): check_period(45) failed\n";
		existsError = true;
	}

	srand(now);
	uint a;
	// checking pram values
	for (a = 0; a < num; ++a) {
		if (pram_control->check_pram(
		        a % (1 << bus->hw_const->pr_ramaddr_width()),      // phys address (SRAM/digital)
		        rand() % (1 << bus->hw_const->pr_paraddr_width()), // physical address (analog)
		        rand() % (1 << bus->hw_const->pr_dacval_width()),  // storage value
		        rand() % (1 << bus->hw_const->pr_lutadr_width()))) // addrees of LUT
			continue;
		break;
	}

	if (a != num) {
		std::ostringstream buffer;
		buffer << a;
		error += "parameterRAM(): check_pram() at ";
		error += buffer.str();
		error += " failed\n";
		existsError = true;
	}

	srand(now);
	// checking lut values
	for (a = 0; a < 16; ++a) { // 16 luts are available
		if (pram_control->check_lut(a % (1 << bus->hw_const->pr_lutadr_width()),
		                            rand() % (1 << bus->hw_const->pr_luttime_width()),
		                            rand() % (1 << bus->hw_const->pr_lutboost_width()),
		                            rand() % (1 << bus->hw_const->pr_lutrepeat_width()),
		                            rand() % (1 << bus->hw_const->pr_lutstep_width())))
			continue;
		break;
	}

	if (a != 16) {
		std::ostringstream buffer;
		buffer << a;
		error += "parameterRAM(): check_lut() at ";
		error += buffer.str();
		error += " failed\n";
		existsError = true;
	}

	if (existsError)
		return false;
	return true;
} // end of parameterRAM


/** Checks eventLoopback.
 *
 *
 */
bool PySpikeyTM::eventLoopback()
{
	initMem(1, true); // create objects :)

	boost::shared_ptr<SC_Mem> mem = boost::dynamic_pointer_cast<SC_Mem>(chip[0]->bus);
	boost::shared_ptr<SC_SlowCtrl> scl = mem->getSCTRL(); // bus is SC_Mem

	boost::shared_ptr<ChipControl> chip_control = chip[0]->getCC(); // get pointer to chip control
	                                                                // class


	// enable clocks to analog_chip
	sp->statusreg[bus->hw_const->cr_anaclken()] = true;
	sp->statusreg[bus->hw_const->cr_anaclkhien()] = true;
	sp->writeCC(); // write statusreg-changes :)


	uint dist = 200, // falls zu niedrig: FIFO->Spikey UnderRuns o.ä.
	    num = 2000;  // => dauer des exp.
	// mit 200 gehen 10.000 oder sowas => test auf die UnderRuns

	srand(now);
	// loops[2] of (event) loopback test
	for (uint l = 0; l < loops[2]; ++l) {
		uint time = 0x200 << 5; // 400MHz and timebins

		// time=(scl->getSyncMax() + scl->getEvtLatency())<<5; // 400MHz and timebins

		SpikeTrain t, r; // transmit & receive spiketrains
		t.d.clear();
		r.d.clear();

		// don't erase initialization commands (in first run)... have to be flushed
		if (l > 0)
			mem->Clear(); // free playback memory

		for (uint i = 0; i < num; i++) {
			time += rand() % dist; // trigger for left (0) and right (1) block
			// neuron addresses: 0-191 + 256-511...
			t.d.push_back(IData::Event(rand() % 192 + (rand() % 2 * 256), time));
		}

		t.d.push_back(IData::Event(0, time + 0x2000 * 16)); // detmines length of experiment

		// set event fifo resets before any event processing starts
		for (uint i = 0; i < bus->hw_const->event_outs(); i++)
			sp->statusreg[bus->hw_const->cr_eout_rst() + i] = true; // bus->hw_const->cr_eout_rst()
			                                                        // => all off (400MHz)

		for (uint i = 0; i < bus->hw_const->event_ins(); i++)
			sp->statusreg[bus->hw_const->cr_einb_rst() + i] = true; // bus->hw_const->cr_ein_rst()
			                                                        // Pos. => all of clkb off
		for (uint i = 0; i < bus->hw_const->event_ins(); i++)
			sp->statusreg[bus->hw_const->cr_ein_rst() + i] = true; // all of clk off
		sp->writeCC(); // "write" (flush needed ;)) changes to chip

		// enable internal event loopback and reset event fifos
		chip_control->setEvLb((1 << bus->hw_const->el_enable_pos()), 20);

		// release event fifo resets
		for (uint i = 0; i < bus->hw_const->event_outs(); i++)
			sp->statusreg[bus->hw_const->cr_eout_rst() + i] = false;
		for (uint i = 0; i < bus->hw_const->event_ins(); i++)
			sp->statusreg[bus->hw_const->cr_einb_rst() + i] = false;
		for (uint i = 0; i < bus->hw_const->event_ins(); i++)
			sp->statusreg[bus->hw_const->cr_ein_rst() + i] = false;
		sp->writeCC(); // "write" changes to chip

		sp->sendSpikeTrain(t);
		sp->recSpikeTrain(r);

		uint firstError;
		if (!checkEvents(firstError, t, r, scl)) {
			error += "eventLoopback(): Failed at event no. ";
			error += firstError;
			error += ".\n";
		}
	}

	if (error != "")
		return false;

	return true;
} // end of PySpikeyTM::eventLoopback()


/** Converts a boolian (read binary) vector to int.
 * This function assists eventLoopback. */
uint PySpikeyTM::bool2int(vector<bool> array)
{
	uint res = 0;
	for (uint i = 0; i < bus->hw_const->cr_width(); i++)
		res |= (array[i] << i);
	return res;
}

/** Converts a boolian (read binary) vector to int.
 * This function assists eventLoopback. */
bool PySpikeyTM::checkEvents(uint& firsterr, SpikeTrain& tx, SpikeTrain& rx,
                             boost::shared_ptr<SC_SlowCtrl> sct)
{
	static_cast<void>(tx);
	firsterr = 0; // initialize error position

	const vector<IData>& rcvd = rx.d;
	const vector<IData>* ev = sct->ev(0); // to get "real" sent data

	if (ev->size() != 0) {
		uint i, j, neuron, time, start;
		int smallestmatch;

		vector<bool> matched(rcvd.size(), false);
		// for(j=0;j<rcvd->size();j++)matched[j]=false; // clear all matches

		smallestmatch = -1;

		for (i = 0; i < ev->size() - 1;
		     ++i) { // ignore the last event as this can't be recv'd anyway
			neuron = (*ev)[i].neuronAdr();
			time = (*ev)[i].time();

			if (smallestmatch > 0)
				start = (uint)smallestmatch;
			else
				start = 0;

			smallestmatch = -1; // in this run nothing has matched yet

			for (j = start; j < rcvd.size(); ++j) {

				if (matched[j])
					continue; // skip already matched entries

				if (smallestmatch < 0)
					smallestmatch = j; // only store once

				const IData d = rcvd[j];

				if (time == d.time() && neuron == d.neuronAdr()) { // ok, found correct answer
					matched[j] = true;
					break;
				}
			}

			if (j >= rcvd.size())
				break; // not found in received
		}

		if (i < ev->size() - 1) {
			firsterr = i;
			cout << "checkEvents(): ...not found. exiting." << endl;
			return false;
		} // not every entry matched

		return true; // everything matched
	}

	return false; // ev->size() == 0
} // end of PySpikeyTM::checkEvents()
