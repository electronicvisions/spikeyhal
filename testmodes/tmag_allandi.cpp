// old testmodes now with Listees and Testmode encapsulation
#include "common.h" // library includes
#include <numeric>
// functions defined in getch.c
int getch(void);
int kbhit(void);

#include "idata.h"
#include "sncomm.h"
#include "spikenet.h"
#include "testmode.h" //Testmode and Lister classes

#include "sc_sctrl.h"
#include "sc_pbmem.h"

// only if needed
#include "ctrlif.h"
#include "synapse_control.h" //synapse control class
#include "pram_control.h"    //parameter ram etc


using namespace spikey2;

class TMAllAndi : public Testmode
{
public:
	uint64_t bool2int(vector<bool> array)
	{
		uint64_t res = 0;
		for (uint i = 0; i < bus->hw_const->cr_width(); i++)
			res |= ((uint64_t)array[i] << i);
		return res;
	}

	bool test()
	{
		boost::shared_ptr<PramControl> pram_control = chip[0]->getPC(); // get pointer to parameter
		                                                                // control class
		boost::shared_ptr<ChipControl> chip_control = chip[0]->getCC(); // get pointer to chip
		                                                                // control class
		boost::shared_ptr<Loopback> loopback = chip[0]->getLB();
		boost::shared_ptr<Loopback> loopback = chip[1]->getLB();

		vector<bool> spikeystatus(bus->hw_const->cr_width(), false);
		for (uint i = 0; i < bus->hw_const->cr_eout_depth_width(); i++)
			spikeystatus[i + bus->hw_const->cr_eout_depth()] =
			    (bool)(((uint)bus->hw_const->cr_eout_depth_resval() >> i) & 0x1);
		uint64_t spstatus;
		IData dummy;

		uint parnum = 200;
		uint lbtests = 500;
		uint evnum = 2000;
		uint everrnum = 0;

		bool evl;
		cout << "Event loopback on orf off (1/0)? ";
		cin >> evl;

		// ***********************************************************************************
		// initialize chip (basically only removes reset...)
		chip[0]->setCtrl(Spikenet::off, Spikenet::reset | Spikenet::pllbypass | Spikenet::pllreset |
		                                    Spikenet::bsmode | Spikenet::cimode,
		                 10);

		// ***********************************************************************************
		// first initialize the parameter ram's fsm with values needed for automatic update
		pram_control->write_period(45);
		pram_control->write_parnum(parnum);


		for (uint i = 0; i < parnum; i++)
			pram_control->write_pram(i, i % parnum, i % (1 << bus->hw_const->pr_dacval_width()), 0);

		// write lookuptable 0
		uint lutrepeat = 0;
		uint lutstep = 0;
		uint luttime = 5;
		uint lutboost = 0;
		pram_control->write_lut(0, luttime, lutboost, lutrepeat, lutstep, 4);

		// enable parameter ram update and clocks to analog_chip
		spikeystatus[bus->hw_const->cr_pram_en()] = true;
		spikeystatus[bus->hw_const->cr_anaclken()] = true;
		spikeystatus[bus->hw_const->cr_anaclkhien()] = true;
		chip_control->setCtrl(bool2int(spikeystatus), 10);

		// change delay values on-the-fly to have optimum delay for the submitted spikey2 chip:
		chip[0]->sendCtrl(false, false, true, 10); // set ci_mode pin
		//	for(uint i=0; i<9; i++){chip[0]->setDelay(ctl_rx0+i,0,100);}
		//	for(uint i=0; i<9; i++){chip[0]->setDelay(ctl_rx1+i,0,100);}
		//	for(uint i=0; i<9; i++){chip[0]->setDelay(ctl_tx0+i,7,100);}
		//	for(uint i=0; i<9; i++){chip[0]->setDelay(ctl_tx1+i,7,100);}
		chip[0]->setDelay(bus->hw_const->ctl_rx0() + 4, 3, 100);
		chip[0]->setDelay(bus->hw_const->ctl_rx0() + 7, 4, 100);
		chip[0]->setDelay(bus->hw_const->ctl_rx1() + 0, 2, 100);
		chip[0]->setDelay(bus->hw_const->ctl_rx1() + 2, 2, 100);
		chip[0]->setDelay(bus->hw_const->ctl_rx1() + 5, 4, 100);
		chip[0]->setDelay(bus->hw_const->ctl_rx1() + 6, 4, 100);
		chip[0]->setDelay(bus->hw_const->ctl_rx1() + 7, 5, 100);

		chip[0]->sendCtrl(false, false, false, 10); // release ci_mode pin
		chip[0]->sendCtrl(false, false, true, 10);  // set ci_mode pin

		SC_SlowCtrl* sc = dynamic_cast<SC_SlowCtrl*>(chip[0]->bus);
		if (sc != NULL) {
			uint pat18 = 0x12345;
			sc->setDout0(pat18);
			sc->setDout1(pat18);
			sc->setDout2(pat18);
			sc->setDout3(pat18);
			sc->getDin0(pat18);
			cout << hex << pat18 << endl;
			sc->getDin1(pat18);
			cout << hex << pat18 << endl;
			sc->getDin2(pat18);
			cout << hex << pat18 << endl;
			sc->getDin3(pat18);
			cout << hex << pat18 << endl;
		}

		chip[0]->sendCtrl(false, false, false, 20); // release ci_mode pin

		chip[0]->sendCtrl(true, false, false, 20);  // release ci_mode pin
		chip[0]->sendCtrl(false, false, false, 20); // release ci_mode pin

		pram_control->write_parnum(parnum, 10);

		chip[0]->Send(SpikenetComm::sync, dummy, 250);

		// read back and check parameters
		//	pram_control->read_pram(0, 50);
		for (uint i = 0; i < parnum; i++) {
			pram_control->read_pram(i);
			if (!pram_control->check_pram(i, i % parnum,
			                              i % (1 << bus->hw_const->pr_dacval_width()), 0)) {
				bus->dbg(0) << "Pram check failed for chip0, address " << hex << i << endl;
			}
		}


		// perform some loopback checks:
		uint64_t pattern = 0;
		for (uint i = 0; i < lbtests; i++) {
			//		pattern+=0x200000;
			pattern = rand() + uint64_t((rand() & 0x1fffffULL) << 32);
			loopback->loopback(pattern, 0);
			cout << "Loopback test chip0 with pattern 0x" << hex << pattern << ": "
			     << loopback->check_test(pattern) << endl;
			pattern = rand() + uint64_t((rand() & 0x1fffffULL) << 32);
			loopback->loopback(pattern, 0);
			cout << "Loopback test chip1 with pattern 0x" << hex << pattern << ": "
			     << loopback->check_test(pattern) << endl;
		}

		// set event fifo resets before any event processing starts
		for (uint i = 0; i < bus->hw_const->event_outs(); i++)
			spikeystatus[bus->hw_const->cr_eout_rst() + i] = true;
		for (uint i = 0; i < bus->hw_const->event_ins(); i++)
			spikeystatus[bus->hw_const->cr_einb_rst() + i] = true;
		for (uint i = 0; i < bus->hw_const->event_ins(); i++)
			spikeystatus[bus->hw_const->cr_ein_rst() + i] = true;
		chip_control->setCtrl(bool2int(spikeystatus), 10);

		// enable internal event loopback and reset event fifos
		if (evl)
			chip_control->setEvLb((1 << bus->hw_const->el_enable_pos()), 10); // turn on event
			                                                                  // loopback:

		// release event fifo resets
		for (uint i = 0; i < bus->hw_const->event_outs(); i++)
			spikeystatus[bus->hw_const->cr_eout_rst() + i] = false;
		for (uint i = 0; i < bus->hw_const->event_ins(); i++)
			spikeystatus[bus->hw_const->cr_einb_rst() + i] = false;
		for (uint i = 0; i < bus->hw_const->event_ins(); i++)
			spikeystatus[bus->hw_const->cr_ein_rst() + i] = false;
		chip_control->setCtrl(bool2int(spikeystatus), 10);

		// read back status and control registers
		chip_control->getCtrl();
		chip_control->rcv_data(spstatus);
		chip_control->getCtrl();
		chip_control->rcv_data(spstatus);
		cout << "This Chip's revision: " << dec
		     << ((spstatus >> (bus->hw_const->cr_width() + bus->hw_const->cr_pos())) & 0xf);
		cout << ".  Status register:";
		binout(cout, (spstatus >> bus->hw_const->cr_pos()), bus->hw_const->cr_width());
		cout << endl;

		chip_control->read_clkhistat();
		chip_control->rcv_data(spstatus);
		chip_control->read_clkhistat();
		chip_control->rcv_data(spstatus);
		cout << "clkhi status:" << endl;
		binout(cout,
		       (spstatus >> (bus->hw_const->cr_pos() + bus->hw_const->ev_timemsb_width() +
		                     bus->hw_const->ev_clkpos_width() + (4 * bus->hw_const->event_outs()))),
		       1);
		cout << " ";
		binout(cout, (spstatus >> (bus->hw_const->cr_pos() + bus->hw_const->ev_clkpos_width() +
		                           (4 * bus->hw_const->event_outs()))),
		       bus->hw_const->ev_timemsb_width());
		cout << " ";
		binout(cout, (spstatus >> (bus->hw_const->cr_pos() + (4 * bus->hw_const->event_outs()))),
		       bus->hw_const->ev_clkpos_width());
		cout << " ";
		for (uint i = bus->hw_const->event_outs(); i > 0; i--) {
			binout(cout, (spstatus >> (bus->hw_const->cr_pos() + 4 * (i - 1))), 4);
			cout << " "; // hard coded 4 status bits of clkhi fifos!
		}
		cout << endl;

		chip_control->read_clkstat();
		chip_control->rcv_data(spstatus);
		chip_control->read_clkstat();
		chip_control->rcv_data(spstatus);
		cout << "clk status:" << endl;
		binout(cout, (spstatus >> (bus->hw_const->cr_pos() + (4 * bus->hw_const->event_ins()))),
		       bus->hw_const->ev_clkpos_width() - 1);
		cout << " ";
		for (uint i = bus->hw_const->event_ins(); i > 0; i--) {
			binout(cout, (spstatus >> (bus->hw_const->cr_pos() + 4 * (i - 1))), 4);
			cout << " "; // hard coded 4 status bits of clk fifos!
		}
		cout << endl;

		chip_control->read_clkbstat();
		chip_control->rcv_data(spstatus);
		chip_control->read_clkbstat();
		chip_control->rcv_data(spstatus);
		cout << "clkb status:" << endl;
		binout(cout, (spstatus >> (bus->hw_const->cr_pos() + (4 * bus->hw_const->event_ins()))),
		       bus->hw_const->ev_clkpos_width() - 1);
		cout << " ";
		for (uint i = bus->hw_const->event_ins(); i > 0; i--) {
			binout(cout, (spstatus >> (bus->hw_const->cr_pos() + 4 * (i - 1))), 4);
			cout << " "; // hard coded 4 status bits of clkb fifos!
		}
		cout << endl;


		// finally test events
		uint errnadr = 0; // neuron address to provocate errors on
		uint time = 0x20; //,time2=0x60; //target cycle to deliver event (buscycle it was send is
		                  //time==0)

		srand(randomseed);

		// send regular events
		uint evtime;
		for (uint i = 0; i < evnum; i++) {
			evtime = ((time + 2 * i + (rand() % 2)) << 4) + rand() % 16;
			chip[0]->sendEventAbs(rand() % 192 + (rand() % 2 * 256), evtime, rand() % 2);
			//		evtime = ((time+5*i)<<4);
			//		chip[0]->sendEventAbs(0,evtime,2);
		}

		for (uint i = 0; i < 2; i++) {
			chip[0]->sendEventAbs(0, (evtime + 4 * i + 0) << 4, 3);
			chip[0]->sendEventAbs(0, (evtime + 4 * i + 1) << 4, 3);
			chip[0]->sendEventAbs(1, (evtime + 4 * i + 2) << 4, 3);
			chip[0]->sendEventAbs(1, (evtime + 4 * i + 3) << 4, 3);
		}

		// send idle events to empty out event buffers
		chip[0]->sendIdleEvent(512); // wait
		for (uint i = 0; i < 100; i++)
			chip[0]->sendIdleEvent(4);

		// eneble event error detection and produce some errors...
		chip_control->setCtrl((1 << bus->hw_const->cr_err_out_en()) +
		                          (1 << bus->hw_const->cr_anaclken()) +
		                          (1 << bus->hw_const->cr_anaclkhien()),
		                      2); // enable event error out:

		time = 0x200; //,time2=0x60; //target cycle to deliver event (buscycle it was send is
		              //time==0)
		// send events to provocate fifo overflow.
		for (uint i = 0; i < everrnum; i++) {
			chip[0]->sendEvent(errnadr, time -= 2, rand() % 16, 0);
		}


		// empty evout buffers
		chip[0]->sendIdleEvent(100);
		for (uint i = 0; i < everrnum; i++)
			chip[0]->sendIdleEvent(0);

		uint errnum = 12;
		bool result = chip[0]->checkEvents(errnum);
		cout << result << " event number:" << dec << errnum << endl;

		return true;

	}; // end TMParam.test
};

class TMAllAndi2 : public Testmode
{
public:
	uint bool2int(vector<bool> array)
	{
		uint res = 0;
		for (uint i = 0; i < bus->hw_const->cr_width(); i++)
			res |= (array[i] << i);
		return res;
	}

	bool test()
	{
		boost::shared_ptr<PramControl> pram_control = chip[0]->getPC(); // get pointer to parameter
		                                                                // control class
		boost::shared_ptr<ChipControl> chip_control = chip[0]->getCC(); // get pointer to chip
		                                                                // control class
		boost::shared_ptr<Loopback> loopback = chip[0]->getLB();

		vector<bool> spikeystatus(bus->hw_const->cr_width(), false);
		for (uint i = 0; i < bus->hw_const->cr_eout_depth_width(); i++)
			spikeystatus[i + bus->hw_const->cr_eout_depth()] =
			    (bool)(((uint)bus->hw_const->cr_eout_depth_resval() >> i) & 0x1);
		uint64_t spstatus;
		IData dummy;

		uint parnum = 10;
		uint lbtests = 9;
		uint evnum = 10;
		uint everrnum = 0;
		uint idleevents = 400;

		// ***********************************************************************************
		// initialize chip (basically only removes reset...)
		chip[0]->setCtrl(Spikenet::off, Spikenet::reset | Spikenet::pllbypass | Spikenet::pllreset |
		                                    Spikenet::bsmode | Spikenet::cimode,
		                 10);

		// ***********************************************************************************
		// first initialize the parameter ram's fsm with values needed for automatic update
		pram_control->write_period(45);
		pram_control->write_parnum(parnum);

		// write lookuptable 0
		uint lutrepeat = 0;
		uint lutstep = 0;
		uint luttime = 5;
		uint lutboost = 0;
		pram_control->write_lut(0, luttime, lutboost, lutrepeat, lutstep, 4);

		// ***********************************************************************************
		// change delay values on-the-fly to have optimum delay for the submitted spikey2 chip:
		chip[0]->sendCtrl(false, false, true, 10); // set ci_mode pin
		//	for(uint i=0; i<9; i++){chip[0]->setDelay(ctl_rx0+i,0,10);}
		//	for(uint i=0; i<9; i++){chip[0]->setDelay(ctl_rx1+i,0,10);}
		//	for(uint i=0; i<9; i++){chip[0]->setDelay(ctl_tx0+i,7,10);}
		//	for(uint i=0; i<9; i++){chip[0]->setDelay(ctl_tx1+i,7,10);}
		chip[0]->setDelay(bus->hw_const->ctl_rx0() + 4, 3, 50);
		chip[0]->setDelay(bus->hw_const->ctl_rx0() + 7, 4, 50);
		chip[0]->setDelay(bus->hw_const->ctl_rx1() + 0, 2, 50);
		chip[0]->setDelay(bus->hw_const->ctl_rx1() + 2, 2, 50);
		chip[0]->setDelay(bus->hw_const->ctl_rx1() + 5, 4, 50);
		chip[0]->setDelay(bus->hw_const->ctl_rx1() + 6, 4, 50);
		chip[0]->setDelay(bus->hw_const->ctl_rx1() + 7, 5, 50);

		chip[0]->sendCtrl(false, false, false, 10); // release ci_mode pin
		chip[0]->sendCtrl(false, false, true, 10);  // set ci_mode pin

		SC_SlowCtrl* sc = dynamic_cast<SC_SlowCtrl*>(chip[0]->bus);
		if (sc != NULL) {
			uint pat18 = 0x12345;
			sc->setDout0(pat18);
			sc->setDout1(pat18);
			sc->setDout2(pat18);
			sc->setDout3(pat18);
			sc->getDin0(pat18);
			cout << hex << pat18 << endl;
			sc->getDin1(pat18);
			cout << hex << pat18 << endl;
			sc->getDin2(pat18);
			cout << hex << pat18 << endl;
			sc->getDin3(pat18);
			cout << hex << pat18 << endl;
		}

		chip[0]->sendCtrl(false, false, false, 20); // release ci_mode pin

		chip[0]->sendCtrl(true, false, false, 20);  // release ci_mode pin
		chip[0]->sendCtrl(false, false, false, 20); // release ci_mode pin


		chip[0]->Send(SpikenetComm::sync, dummy, 10);

		// ***********************************************************************************
		// perform some loopback checks:
		uint64_t pattern = 0;
		for (uint i = 0; i < lbtests; i++) {
			//		pattern+=0x200000;
			pattern = rand() + uint64_t((rand() & 0x1fffffULL) << 32);
			loopback->loopback(pattern, 5);
			cout << "Loopback test with pattern 0x" << hex << pattern << ": "
			     << loopback->check_test(pattern) << endl;
		}

		// set event fifo resets before any event processing starts
		for (uint i = 0; i < bus->hw_const->event_outs(); i++)
			spikeystatus[bus->hw_const->cr_eout_rst() + i] = true;
		for (uint i = 0; i < bus->hw_const->event_ins(); i++)
			spikeystatus[bus->hw_const->cr_einb_rst() + i] = true;
		for (uint i = 0; i < bus->hw_const->event_ins(); i++)
			spikeystatus[bus->hw_const->cr_ein_rst() + i] = true;
		chip_control->setCtrl(bool2int(spikeystatus), 10);

		// enable internal event loopback and reset event fifos
		chip_control->setEvLb((1 << bus->hw_const->el_enable_pos()), 10); // turn on event loopback:

		// release event fifo resets
		for (uint i = 0; i < bus->hw_const->event_outs(); i++)
			spikeystatus[bus->hw_const->cr_eout_rst() + i] = false;
		for (uint i = 0; i < bus->hw_const->event_ins(); i++)
			spikeystatus[bus->hw_const->cr_einb_rst() + i] = false;
		for (uint i = 0; i < bus->hw_const->event_ins(); i++)
			spikeystatus[bus->hw_const->cr_ein_rst() + i] = false;
		chip_control->setCtrl(bool2int(spikeystatus), 10);

		// read back status and control registers
		chip_control->getCtrl();
		chip_control->rcv_data(spstatus);
		chip_control->getCtrl();
		chip_control->rcv_data(spstatus);
		cout << "This Chip's revision: " << dec
		     << ((spstatus >> (bus->hw_const->cr_width() + bus->hw_const->cr_pos())) & 0xf);
		cout << ".  Status register:";
		binout(cout, (spstatus >> bus->hw_const->cr_pos()), bus->hw_const->cr_width());
		cout << endl;

		chip_control->read_clkhistat();
		chip_control->rcv_data(spstatus);
		chip_control->read_clkhistat();
		chip_control->rcv_data(spstatus);
		cout << "clkhi status:" << endl;
		binout(cout,
		       (spstatus >> (bus->hw_const->cr_pos() + bus->hw_const->ev_timemsb_width() +
		                     bus->hw_const->ev_clkpos_width() + (4 * bus->hw_const->event_outs()))),
		       1);
		cout << " ";
		binout(cout, (spstatus >> (bus->hw_const->cr_pos() + bus->hw_const->ev_clkpos_width() +
		                           (4 * bus->hw_const->event_outs()))),
		       bus->hw_const->ev_timemsb_width());
		cout << " ";
		binout(cout, (spstatus >> (bus->hw_const->cr_pos() + (4 * bus->hw_const->event_outs()))),
		       bus->hw_const->ev_clkpos_width());
		cout << " ";
		for (uint i = bus->hw_const->event_outs(); i > 0; i--) {
			binout(cout, (spstatus >> (bus->hw_const->cr_pos() + 4 * (i - 1))), 4);
			cout << " "; // hard coded 4 status bits of clkhi fifos!
		}
		cout << endl;

		chip_control->read_clkstat();
		chip_control->rcv_data(spstatus);
		chip_control->read_clkstat();
		chip_control->rcv_data(spstatus);
		cout << "clk status:" << endl;
		binout(cout, (spstatus >> (bus->hw_const->cr_pos() + (4 * bus->hw_const->event_ins()))),
		       bus->hw_const->ev_clkpos_width() - 1);
		cout << " ";
		for (uint i = bus->hw_const->event_ins(); i > 0; i--) {
			binout(cout, (spstatus >> (bus->hw_const->cr_pos() + 4 * (i - 1))), 4);
			cout << " "; // hard coded 4 status bits of clk fifos!
		}
		cout << endl;

		chip_control->read_clkbstat();
		chip_control->rcv_data(spstatus);
		chip_control->read_clkbstat();
		chip_control->rcv_data(spstatus);
		cout << "clkb status:" << endl;
		binout(cout, (spstatus >> (bus->hw_const->cr_pos() + (4 * bus->hw_const->event_ins()))),
		       bus->hw_const->ev_clkpos_width() - 1);
		cout << " ";
		for (uint i = bus->hw_const->event_ins(); i > 0; i--) {
			binout(cout, (spstatus >> (bus->hw_const->cr_pos() + 4 * (i - 1))), 4);
			cout << " "; // hard coded 4 status bits of clkb fifos!
		}
		cout << endl;


		// ***********************************************************************************
		// enable parameter ram update and clocks to analog_chip

		// finally test events
		uint errnadr = 0; // neuron address to provocate errors on
		uint time = 0x20; //,time2=0x60; //target cycle to deliver event (buscycle it was send is
		                  //time==0)

		// send regular events mixed with pram accesses
		uint maxevbetwpramacc = 5;
		uint pars = 0;
		uint lastiwithpram = 0;
		uint nexti = 0;
		uint evtime;

		srand(randomseed);

		for (uint i = 0; i < evnum; i++) {
			if (pars < 2 * parnum) {
				if (i == nexti) {
					if (pars < parnum)
						pram_control->write_pram(pars, pars % parnum,
						                         pars % (1 << bus->hw_const->pr_dacval_width()), 0);
					else
						pram_control->read_pram(pars - parnum);
					pars++;
				} else if (i > nexti) {
					nexti += rand() % maxevbetwpramacc;
				}
			} else if (pars == 2 * parnum) {
				spikeystatus[bus->hw_const->cr_pram_en()] = true;
				spikeystatus[bus->hw_const->cr_anaclken()] = true;
				spikeystatus[bus->hw_const->cr_anaclkhien()] = true;
				chip_control->setCtrl(bool2int(spikeystatus), 10);
				pars++;
			}
			//		evtime = ((time+2*i+(rand()%2))<<4)+rand()%16;
			//		chip[0]->sendEventAbs(rand()%192+(rand()%2*256),evtime,rand()%2);
			evtime = ((time + 4 * i) << 4);
			chip[0]->sendEventAbs(0, evtime, 2);
		}
		pram_control->read_pram(0); // final dummy read

		// check parameters
		for (uint i = 0; i < parnum; i++) {
			if (!pram_control->check_pram(i, i % parnum,
			                              i % (1 << bus->hw_const->pr_dacval_width()), 0)) {
				bus->dbg(0) << "Pram check failed for chip0, address " << hex << i << endl;
			}
		}

		// send idle events to empty out event buffers
		chip[0]->sendIdleEvent(512); // wait
		for (uint i = 0; i < idleevents; i++)
			chip[0]->sendIdleEvent(4);

		// eneble event error detection and produce some errors...
		chip_control->setCtrl((1 << bus->hw_const->cr_err_out_en()) +
		                          (1 << bus->hw_const->cr_anaclken()) +
		                          (1 << bus->hw_const->cr_anaclkhien()),
		                      2); // enable event error out:

		time = 0x200; //,time2=0x60; //target cycle to deliver event (buscycle it was send is
		              //time==0)
		// send events to provocate fifo overflow.
		for (uint i = 0; i < everrnum; i++) {
			chip[0]->sendEvent(errnadr, time -= 2, rand() % 16, 0);
		}


		// empty evout buffers
		chip[0]->sendIdleEvent(100);
		for (uint i = 0; i < everrnum; i++)
			chip[0]->sendIdleEvent(0);

		uint errnum;
		cout << chip[0]->checkEvents(errnum) << " event number:" << dec << errnum << endl;

		return true;

	}; // end TMAllAndi2.test
};

class LteeAllAndi : public Listee<Testmode>
{
public:
	LteeAllAndi()
	    : Listee<Testmode>(string("allandi"), string("random test for digital part but synctl")){};
	Testmode* create() { return new TMAllAndi(); };
} ListeeAllAndi;

class LteeAllAndi2 : public Listee<Testmode>
{
public:
	LteeAllAndi2()
	    : Listee<Testmode>(
	          string("allandi2"),
	          string("random test for digital part but synctl with mixed event and ci access")){};
	Testmode* create() { return new TMAllAndi2(); };
} ListeeAllAndi2;
