// old testmodes now with Listees and Testmode encapsulation
#include "common.h" // library includes
#include "idata.h"
#include "sncomm.h"
#include "spikenet_base.h" //verilog definitions created with cconst.pl
#include "spikenet.h"
#include "testmode.h" //Testmode and Lister classes

// only if needed
#include "ctrlif.h"
#include "synapse_control.h" //synapse control class
#include "pram_control.h"    //parameter ram etc


using namespace spikey2;

// disables the parameter ram FSM for one chip by setting number of parameters to
// zero and enabling the update procedure.
class TMDisablePram : public Testmode
{
public:
	bool test()
	{
		PramControl* pc = chip[0]->getPC(); // get pointer to parameter control class
		ChipControl* cc = chip[0]->getCC(); // get pointer to chip control class

		pc->write_parnum(0, 10);
		cc->setCtrl((1 << (cr_pram_en)), 100);

		return true;
	};
};

// asserts reset during operation and reconfigures delaylines.
class TMReconfig : public Testmode
{
public:
	bool test()
	{

		chip[0]->sendCtrl(true, false, true, 10);   // set reset and ci_mode pin
		chip[0]->sendCtrl(true, false, false, 10);  // release ci_mode pin
		chip[0]->sendCtrl(false, false, false, 10); // release reset pin

		for (uint i = 0; i < 8; i++) {
			chip[0]->sendCtrl(false, false, true, 10); // set ci_mode pin
			chip[0]->setDelay(18, i, 10);
			chip[0]->setDelay(32, i, 10);
			chip[0]->sendCtrl(false, false, false, 10); // release ci_mode pin
		}

		return true;
	};
};

// bombs a single buffer with events until overflow.
// Error detection feature of chip is enabled.
class TMEvoverflow : public Testmode
{
public:
	bool test()
	{

		chip[0]->sendCtrl(true, false, true, 10);   // set reset and ci_mode pin
		chip[0]->sendCtrl(true, false, false, 10);  // release ci_mode pin
		chip[0]->sendCtrl(false, false, false, 10); // release reset pin

		bus->Send(SpikenetComm::sync);

		return true;
	};
};

// tests event interface for four chips. Three events
// are generated for each chip to have a maximum bus loading.
class TMEvmultichip : public Testmode
{
public:
	bool test()
	{
		ChipControl* cc0 = chip[0]->getCC(); // get pointer to chip control class
		ChipControl* cc1 = chip[1]->getCC(); // get pointer to chip control class
		ChipControl* cc2 = chip[2]->getCC(); // get pointer to chip control class
		ChipControl* cc3 = chip[3]->getCC(); // get pointer to chip control class

		// event time offsets for each chip:
		int numchips = 4;
		uint time[numchips];
		time[0] = 0xb0, time[1] = 0x20, time[2] = 0x30, time[3] = 0x40;

		// switch on update on valid ram entries, enable clocks to analog_chip
		cc0->setCtrl((1 << cr_anaclken) | (1 << (cr_anaclkhien)) | (1 << cr_err_out_en));
		cc1->setCtrl((1 << cr_anaclken) | (1 << (cr_anaclkhien)) | (1 << cr_err_out_en));
		cc2->setCtrl((1 << cr_anaclken) | (1 << (cr_anaclkhien)) | (1 << cr_err_out_en));
		cc3->setCtrl((1 << cr_anaclken) | (1 << (cr_anaclkhien)) | (1 << cr_err_out_en));

		/*	for(uint i=0;i<512;i++){
		        uint chip=rand()%4;
		        for(uint j=0;j<3;j++){
		            chip[chip]->sendEvent(rand()%192+(rand()%2*256),time[chip]+rand()%2,rand()%16,rand()%2);
		        }
		    }
		*/
		return true;
	};
};

class TMAllAndi : public Testmode
{
public:
	bool test()
	{
		PramControl* pc = chip[0]->getPC(); // get pointer to parameter control class
		ChipControl* cc = chip[0]->getCC(); // get pointer to chip control class
		PramControl* pc1 = chip[1]->getPC(); // get pointer to parameter control class
		ChipControl* cc1 = chip[1]->getCC(); // get pointer to chip control class

		uint time = 0xb0, time2 = 0x20; // target cycle to deliver event (buscycle it was send is
		                                // time==0)

		// first initialize the parameter ram's fsm with values needed for automatic update
		pc->write_period(45);
		pc->write_parnum(512);
		pc1->write_period(67);
		pc1->write_parnum(512);

		for (uint i = 0; i < 512; i++) {
			pc->write_pram(i, i % 512, i / 4, i % 16);
			pc1->write_pram(i, i % 512, i / 2, (i * 3) % 16);
		}

		// write lookuptables
		for (uint i = 0; i < 16; i++) {
			pc->write_lut(i, i, i);
			pc1->write_lut(i, i, i);
		}

		pc->write_pram(3000, 0, 0, 0, 20); // to have one packet with more than minimum delay
		// the last one of the above loop won't be delivered, else

		// change delay values on-the-fly
		chip[0]->sendCtrl(false, false, true, 10); // set ci_mode pin
		for (uint i = 0; i < 9; i++) {
			chip[0]->setDelay(ctl_rx0 + i, 3, 10);
		}
		for (uint i = 0; i < 9; i++) {
			chip[0]->setDelay(ctl_rx1 + i, 3, 10);
		}
		for (uint i = 0; i < 9; i++) {
			chip[0]->setDelay(ctl_tx0 + i, 0, 10);
		}
		for (uint i = 0; i < 9; i++) {
			chip[0]->setDelay(ctl_tx1 + i, 0, 10);
		}
		chip[0]->sendCtrl(false, false, false, 10); // release ci_mode pin

		// read back and check parameters
		pc->read_pram(0, 50);
		pc1->read_pram(0, 50);
		for (uint i = 0; i < 512; i++) {
			pc->read_pram(i + 1);
			if (!pc->check_pram(i, i % 512, i / 4, i % 16)) {
				dbg(0) << "Pram check failed for chip0, address " << hex << i << endl;
			}
			pc1->read_pram(i + 1);
			if (!pc1->check_pram(i, i % 512, i / 2, (i * 3) % 16)) {
				dbg(0) << "Pram check failed for chip1, address " << hex << i << endl;
			}
		}

		// switch on update on valid ram entries, enable clocks to analog_chip
		cc->setCtrl((1 << cr_anaclken) | (1 << (cr_anaclkhien)) | (1 << (cr_pram_en)) |
		            (1 << cr_err_out_en));
		cc1->setCtrl((1 << cr_anaclken) | (1 << (cr_anaclkhien)) | (1 << (cr_pram_en)) |
		             (1 << cr_err_out_en));

		// start sending events
		for (uint i = 0; i < 200; i++) {
			chip[0]->sendEvent(10, time, rand() % 16); // buffer bombardment
/*		chip[0]->sendEvent(rand()%192+(rand()%2*256),time+rand()%2,rand()%16,rand()%2);
		chip[0]->sendEvent(rand()%192+(rand()%2*256),time+rand()%2,rand()%16,rand()%2);
		chip[0]->sendEvent(rand()%192+(rand()%2*256),time+rand()%2,rand()%16,rand()%2);
		chip[1]->sendEvent(rand()%192+(rand()%2*256),time2+rand()%2,rand()%16,rand()%2);
		chip[1]->sendEvent(rand()%192+(rand()%2*256),time2+rand()%2,rand()%16,rand()%2);
		chip[1]->sendEvent(rand()%192+(rand()%2*256),time2+rand()%2,rand()%16,rand()%2);
		chip[0]->sendEvent(rand()%192+(rand()%2*256),time+rand()%2,rand()%16,rand()%2);
		chip[0]->sendEvent(rand()%192+(rand()%2*256),time+rand()%2,rand()%16,rand()%2);
		chip[1]->sendEvent(rand()%192+(rand()%2*256),time2+rand()%2,rand()%16,rand()%2);
		chip[1]->sendEvent(rand()%192+(rand()%2*256),time2+rand()%2,rand()%16,rand()%2);
		chip[0]->sendEvent(rand()%192+(rand()%2*256),time+rand()%2,rand()%16,rand()%2);
		chip[1]->sendEvent(rand()%192+(rand()%2*256),time2+rand()%2,rand()%16,rand()%2);*/}

		// empty evout buffers
		chip[0]->sendIdleEvent(0);
		chip[1]->sendIdleEvent(512);
		for (uint i = 0; i < 50; i++)
			chip[0]->sendIdleEvent(0);
		for (uint i = 0; i < 50; i++)
			chip[1]->sendIdleEvent(0);

		// check, if errors are present.
		uint errnum;
		cout << chip[0]->checkEvents(errnum) << " chip 0, event number:" << errnum << endl;
		cout << chip[1]->checkEvents(errnum) << " chip 1, event number:" << errnum << endl;

		return true;
	}; // end TMParam.test
};

// Listees
class LteeEvmultichip : public Listee<Testmode>
{
public:
	LteeEvmultichip()
	    : Listee<Testmode>(string("evmultichip"), string("tests event interfaces of 4 chips")){};
	Testmode* create() { return new TMEvmultichip(); };
} ListeeEvmultichip;
class LteeEvoverflow : public Listee<Testmode>
{
public:
	LteeEvoverflow()
	    : Listee<Testmode>(string("evoverflow"), string("tests buffer rates until overflow.")){};
	Testmode* create() { return new TMEvoverflow(); };
} ListeeEvoverflow;
class LteeReconfig : public Listee<Testmode>
{
public:
	LteeReconfig()
	    : Listee<Testmode>(string("reconfig"), string("sweep delay of tx0_0 and tx1_5")){};
	Testmode* create() { return new TMReconfig(); };
} ListeeReconfig;
class LteeAllAndi : public Listee<Testmode>
{
public:
	LteeAllAndi()
	    : Listee<Testmode>(string("allandi"), string("random test for digital part but synctl")){};
	Testmode* create() { return new TMAllAndi(); };
} ListeeAllAndi;
class LteeDisablePram : public Listee<Testmode>
{
public:
	LteeDisablePram()
	    : Listee<Testmode>(string("disablepram"),
	                       string("random test for digital part but synctl")){};
	Testmode* create() { return new TMDisablePram(); };
} ListeeDisablePram;
