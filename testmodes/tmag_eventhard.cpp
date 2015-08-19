// old testmodes now with Listees and Testmode encapsulation

#include "common.h" // library includes
#include "math.h"
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

class TMEventHard : public Testmode
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
		boost::shared_ptr<ChipControl> chip_control = chip[0]->getCC(); // get pointer to chip
		                                                                // control class
		vector<bool> spikeystatus(bus->hw_const->cr_width(), false);

		uint evnum = 300;
		// uint everrnum = 80;

		// start event clocks
		spikeystatus[bus->hw_const->cr_anaclken()] = true;
		spikeystatus[bus->hw_const->cr_anaclkhien()] = true;
		chip_control->setCtrl(bool2int(spikeystatus), 2);

		// enable internal event loopback and reset event fifos
		//	chip_control->setEvLb((1<<el_enable_pos), 10);		// turn on event loopback:

		// set event fifo resets
		for (uint i = 0; i < bus->hw_const->event_outs(); i++)
			spikeystatus[bus->hw_const->cr_eout_rst() + i] = true;
		for (uint i = 0; i < bus->hw_const->event_ins(); i++)
			spikeystatus[bus->hw_const->cr_einb_rst() + i] = true;
		for (uint i = 0; i < bus->hw_const->event_ins(); i++)
			spikeystatus[bus->hw_const->cr_ein_rst() + i] = true;
		chip_control->setCtrl(bool2int(spikeystatus), 2);
		// release event fifo resets
		for (uint i = 0; i < bus->hw_const->event_outs(); i++)
			spikeystatus[bus->hw_const->cr_eout_rst() + i] = false;
		for (uint i = 0; i < bus->hw_const->event_ins(); i++)
			spikeystatus[bus->hw_const->cr_einb_rst() + i] = false;
		for (uint i = 0; i < bus->hw_const->event_ins(); i++)
			spikeystatus[bus->hw_const->cr_ein_rst() + i] = false;
		chip_control->setCtrl(bool2int(spikeystatus), 2);


		// uint errnadr=0; // neuron address to provocate errors on
		uint time = 0x20; //,time2=0x60; //target cycle to deliver event (buscycle it was send is
		                  //time==0)

		// send regular events
		for (uint i = 0; i < evnum; i++) {
			chip[0]->sendEvent(rand() % 64 + (rand() % 2 * 256), time + rand() % 2, rand() % 16,
			                   rand() % 2);
		}
		//	for(uint i=0;i<evnum;i++){chip[0]->sendEvent(rand()%64,time+4,rand()%16,0);}
		chip[0]->sendIdleEvent(40); // wait and empty out buffers
		for (uint i = 0; i < evnum; i++)
			chip[0]->sendIdleEvent(4);
		chip[0]->sendIdleEvent(40); // wait and empty out buffers

		/*
		    chip_control->setCtrl((1<<cr_err_out_en)+(1<<cr_anaclken)+(1<<cr_anaclkhien),2);//
		enable event error out:

		    time=0x200;//,time2=0x60; //target cycle to deliver event (buscycle it was send is
		time==0)
		    // send events to provocate fifo overflow.
		    for(uint i=0;i<everrnum;i++){chip[0]->sendEvent(errnadr,time-=2,rand()%16,0);}


		// empty evout buffers
		    chip[0]->sendIdleEvent(100);
		//	chip[1]->sendIdleEvent(512);
		    for(uint i=0;i<everrnum;i++)chip[0]->sendIdleEvent(0);
		//	for(uint i=0;i<60;i++)chip[1]->sendIdleEvent(0);
		//	for(uint i=0;i<16;i++)chip[0]->sendEvent(nadr+i,time,15-i);
		*/

		uint errnum;
		cout << chip[0]->checkEvents(errnum) << " event number:" << dec << errnum << endl;
		return true;
	}; // end TMEventHard.test()
};

class TMEventComp : public Testmode
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
		IData dummy;

		boost::shared_ptr<ChipControl> chip_control = chip[0]->getCC(); // get pointer to chip
		                                                                // control class
		vector<bool> spikeystatus(bus->hw_const->cr_width(), false);
		uint64_t spstatus;

		uint evnum = 1000;
		// uint everrnum = 80;

		chip[0]->Send(SpikenetComm::sync, dummy, 100);
		chip_control->read_clkhistat();
		chip_control->read_clkhistat();
		chip_control->rcv_data(spstatus);

		cout << "Read back clkhi_pos counter value: " << hex
		     << (0xff & (spstatus >> (bus->hw_const->cr_clkhipos_pos() + bus->hw_const->cr_pos())))
		     << endl;

		chip[0]->Send(SpikenetComm::sync, dummy, 100);

		// read back clkhi counter value for later printout (2 reads needed)
		chip_control->read_clkhistat();
		chip_control->read_clkhistat();
		chip_control->rcv_data(spstatus);
		chip_control->rcv_data(spstatus);

		cout << "Read back clkhi_pos counter value: " << hex
		     << (0xff & (spstatus >> (bus->hw_const->cr_clkhipos_pos() + bus->hw_const->cr_pos())))
		     << endl;

		// start event clocks
		spikeystatus[bus->hw_const->cr_anaclken()] = true;
		spikeystatus[bus->hw_const->cr_anaclkhien()] = true;
		chip_control->setCtrl(bool2int(spikeystatus), 2);
		// set event fifo resets
		for (uint i = 0; i < bus->hw_const->event_outs(); i++)
			spikeystatus[bus->hw_const->cr_eout_rst() + i] = true;
		for (uint i = 0; i < bus->hw_const->event_ins(); i++)
			spikeystatus[bus->hw_const->cr_einb_rst() + i] = true;
		for (uint i = 0; i < bus->hw_const->event_ins(); i++)
			spikeystatus[bus->hw_const->cr_ein_rst() + i] = true;
		chip_control->setCtrl(bool2int(spikeystatus), 2);

		// enable internal event loopback and reset event fifos
		chip_control->setEvLb((1 << bus->hw_const->el_enable_pos()), 10); // turn on event loopback:

		// release event fifo resets
		for (uint i = 0; i < bus->hw_const->event_outs(); i++)
			spikeystatus[bus->hw_const->cr_eout_rst() + i] = false;
		for (uint i = 0; i < bus->hw_const->event_ins(); i++)
			spikeystatus[bus->hw_const->cr_einb_rst() + i] = false;
		for (uint i = 0; i < bus->hw_const->event_ins(); i++)
			spikeystatus[bus->hw_const->cr_ein_rst() + i] = false;
		chip_control->setCtrl(bool2int(spikeystatus), 2);


		// uint errnadr=0; // neuron address to provocate errors on
		uint time = 0x20; //,time2=0x60; //target cycle to deliver event (buscycle it was send is
		                  //time==0)

		// send regular events
		uint evtime;
		for (uint i = 0; i < evnum; i++) {
			evtime = ((time + 2 * i + (rand() % 2)) << 4) + rand() % 16;
			chip[0]->sendEventAbs(rand() % 192 + (rand() % 2 * 256), evtime, rand() % 2);
		}

		// send idle events if transfers file is used:
		chip[0]->sendIdleEvent(512); // wait and empty out buffers
		for (uint i = 0; i < 500; i++)
			chip[0]->sendIdleEvent(4);
		chip[0]->sendIdleEvent(40); // wait and empty out buffers

		/*
		    chip_control->setCtrl((1<<cr_err_out_en)+(1<<cr_anaclken)+(1<<cr_anaclkhien),2);//
		enable event error out:

		    time=0x200;//,time2=0x60; //target cycle to deliver event (buscycle it was send is
		time==0)
		    // send events to provocate fifo overflow.
		    for(uint i=0;i<everrnum;i++){chip[0]->sendEvent(errnadr,time-=2,rand()%16,0);}


		// empty evout buffers
		    chip[0]->sendIdleEvent(100);
		    for(uint i=0;i<everrnum;i++)chip[0]->sendIdleEvent(0);
		*/
		uint errnum;
		cout << chip[0]->checkEvents(errnum) << " event number:" << dec << errnum << endl;
		return true;
	}; // end TMEventHard.test()
};

class TMStart : public Testmode
{
public:
	bool test()
	{
		chip[0]->sendCtrl(true, true, false, 10); // reset, pllreset for 10 200M clocks
		chip[0]->sendCtrl(true, false, false, 10);  // release pll reset
		chip[0]->sendCtrl(false, false, false, 40); // start chips

		// 	boost::shared_ptr<ChipControl> chip_control = chip[0]->getCC();
		// 	uint64_t rdata;
		// 	chip_control->getCtrl();
		// 	chip_control->rcv_data(rdata);
		// 	cout << "control reg: " << hex << (rdata>>bus->hw_const->cr_pos()) << endl;

		return true; // does need some return value
	};
};


// Listees
class LteeEventHard : public Listee<Testmode>
{
public:
	LteeEventHard()
	    : Listee<Testmode>(string("eventhard"),
	                       string("test random events with sc_trans and produce event errors")){};
	Testmode* create() { return new TMEventHard(); };
} ListeeEventHard;

class LteeEventComp : public Listee<Testmode>
{
public:
	LteeEventComp()
	    : Listee<Testmode>(string("eventcomp"),
	                       string("produces transfers file events for pb mem for compare")){};
	Testmode* create() { return new TMEventComp(); };
} ListeeEventComp;

class LteeStart : public Listee<Testmode>
{
public:
	LteeStart() : Listee<Testmode>(string("start"), string("start chip - release resets only")){};
	Testmode* create() { return new TMStart(); };
} ListeeStart;
