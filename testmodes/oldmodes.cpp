// old testmodes now with Listees and Testmode encapsulation

#include "common.h" // library includes
#include "idata.h"
#include "sncomm.h"
#include "spikenet.h"
#include "testmode.h" //Testmode and Lister classes

// only if needed
#include "ctrlif.h"
#include "synapse_control.h" //synapse control class
#include "pram_control.h"    //parameter ram etc
#include "sc_sctrl.h"
#include "sc_pbmem.h"


static log4cxx::LoggerPtr logger = log4cxx::Logger::getLogger("TEST.PRAM");

using namespace spikey2;

// detailed startup with changing delays
class TMDelay : public Testmode
{
public:
	bool test()
	{

		chip[0]->sendCtrl(true, true, false, 10); // reset, pllreset & cimode for 10 200M clocks
		chip[0]->sendCtrl(true, false, false, 10); // release pll reset
		chip[0]->sendCtrl(true, false, true, 10);  // set ci_mode pin

		for (uint i = 0; i < 9; i++) {
			chip[0]->setDelay(bus->hw_const->ctl_rx0() + i, 0, 10);
		}
		for (uint i = 0; i < 9; i++) {
			chip[0]->setDelay(bus->hw_const->ctl_rx1() + i, 0, 10);
		}
		for (uint i = 0; i < 9; i++) {
			chip[0]->setDelay(bus->hw_const->ctl_tx0() + i, 7, 10);
		}
		for (uint i = 0; i < 9; i++) {
			chip[0]->setDelay(bus->hw_const->ctl_tx1() + i, 7, 10);
		}

		// write delays chip1
		/*	if(chip.size()>=2){

		        for(uint i=0; i<9; i++){chip[1]->setDelay(ctl_rx0+i,3,10);}
		        for(uint i=0; i<9; i++){chip[1]->setDelay(ctl_rx1+i,3,10);}
		        for(uint i=0; i<9; i++){chip[1]->setDelay(ctl_tx0+i,3,10);}
		        for(uint i=0; i<9; i++){chip[1]->setDelay(ctl_tx1+i,3,10);}
		    }
		*/
		chip[0]->sendCtrl(true, false, false, 50);  // first release ci_mode
		chip[0]->sendCtrl(false, false, false, 50); // start chips

		bus->Send(SpikenetComm::sync);
		return true;
	};
};

// default startup without changing delays
class TMShortStart : public Testmode
{
public:
	bool test()
	{
		chip[0]->sendCtrl(true, true, false, 40); // reset, pllreset for 10 200M clocks
		chip[0]->sendCtrl(true, false, false, 40);  // release pll reset
		chip[0]->sendCtrl(false, false, false, 40); // start chips

		bus->Send(SpikenetComm::sync);
		return true;
	};
};

class TMMixedlong : public Testmode
{
public:
	bool test()
	{
		boost::shared_ptr<SynapseControl> synapse_control =
		    chip[0]->getSC(); // get pointer to synapse control class

		// disable pram auto updates
		boost::shared_ptr<PramControl> pram_control = chip[0]->getPC(); // get pointer to parameter
		                                                                // control class
		pram_control->write_parnum(1); // disable update loop

		uint trow, tcol, tpcsec, tsense, tpcorperiod, tshift;
		// test parameters
		trow = 0;
		tcol = 0;
		tshift = 12;
		tpcsec = 4; // 20 ns@200M
		tsense = 20; // 100 ns@200M
		uint tdel = 6 + tsense + tpcsec;
		uint tdelrc = 10 + tsense + tpcsec;
		tpcorperiod = 2 * tdel;
		uint tda = (0xa << tshift) | 5, tdb = (5 << tshift) | 0xa;

		synapse_control->set_mmask(0x00f00f); // only one block per network block

		// *** initialization of synapse control
		synapse_control->write_time(tsense, tpcsec, tpcorperiod); // time register
		synapse_control->read_time();
		synapse_control->read_time();
		cout << synapse_control->check_time(tsense, tpcsec, tpcorperiod) << endl;

		synapse_control->fill_plut(SynapseControl::linear | SynapseControl::write, 1,
		                           0); // standard correlation lookup table

		bus->dbg(0) << "SynapseControl PLUT test: "
		            << synapse_control->fill_plut(SynapseControl::linear | SynapseControl::verify,
		                                          1, 0) << endl;


		// *** synapse memory
		synapse_control->write_sram(trow, tcol, tda, tdel);
		synapse_control->close();
		synapse_control->write_sram(trow + 1, tcol, tdb, tdel);
		synapse_control->close();

		synapse_control->read_sram(trow, tcol, tdel);
		synapse_control->check_sram(0);
		synapse_control->read_sram(trow, tcol);

		bus->dbg(0) << "SynapseControl SRAM test: " << synapse_control->check_sram(tda) << endl;
		synapse_control->close();
		synapse_control->read_sram(trow + 1, tcol, tdel);
		synapse_control->check_sram(0);
		synapse_control->read_sram(trow + 1, tcol);
		bus->dbg(0) << "SynapseControl SRAM test: " << synapse_control->check_sram(tdb) << endl;
		synapse_control->close();

		// *** rowconfig memory
		uint cd0 = (bus->hw_const->sc_sdd_evin() | bus->hw_const->sc_sdd_senx() |
		            bus->hw_const->sc_sdd_cap2()) |
		           ((bus->hw_const->sc_sdd_evin() | bus->hw_const->sc_sdd_senx() |
		             bus->hw_const->sc_sdd_cap4())
		            << bus->hw_const->sc_blockshift()),
		     cd1 = (bus->hw_const->sc_sdd_evin() | bus->hw_const->sc_sdd_seni() |
		            bus->hw_const->sc_sdd_cap2()) |
		           ((bus->hw_const->sc_sdd_evin() | bus->hw_const->sc_sdd_seni() |
		             bus->hw_const->sc_sdd_cap4())
		            << bus->hw_const->sc_blockshift());
		synapse_control->set_mmask(0xff | (0xff << bus->hw_const->sc_blockshift()));

		synapse_control->write_sram(trow, 1 << bus->hw_const->sc_rowconfigbit(), cd0, tdel);
		synapse_control->close();
		synapse_control->write_sram(trow + 1, 1 << bus->hw_const->sc_rowconfigbit(), cd1, tdel);
		synapse_control->close();

		synapse_control->read_sram(trow, 1 << bus->hw_const->sc_rowconfigbit(), tdel);
		synapse_control->check_sram(0);
		synapse_control->read_sram(trow, 1 << bus->hw_const->sc_rowconfigbit());
		bus->dbg(0) << "SynapseControl rowconfig test: " << synapse_control->check_sram(cd0)
		            << endl;
		synapse_control->close();
		synapse_control->read_sram(trow + 1, 1 << bus->hw_const->sc_rowconfigbit(), tdel);
		synapse_control->check_sram(0);
		synapse_control->read_sram(trow + 1, 1 << bus->hw_const->sc_rowconfigbit());
		bus->dbg(0) << "SynapseControl rowconfig test: " << synapse_control->check_sram(cd1)
		            << endl;
		synapse_control->close();

		// *** neuron config ***
		synapse_control->set_mmask(0x006006); // neuron config bits
		synapse_control->write_sram(
		    99, (1 << bus->hw_const->sc_neuronconfigbit()) | tcol,
		    bus->hw_const->sc_ncd_outamp() | (bus->hw_const->sc_ncd_outamp() << tshift), tdel);
		synapse_control->close();
		synapse_control->write_sram(99, tcol, 0, tdel);
		synapse_control->close();
		synapse_control->read_sram(34, (1 << bus->hw_const->sc_neuronconfigbit()) | tcol, tdel);
		synapse_control->check_sram(0);
		synapse_control->read_sram(34, (1 << bus->hw_const->sc_neuronconfigbit()) | tcol, tdel);
		bus->dbg(0) << "SynapseControl 2. neuron config test: "
		            << synapse_control->check_sram(bus->hw_const->sc_ncd_outamp() |
		                                           (bus->hw_const->sc_ncd_outamp() << tshift))
		            << endl;
		synapse_control->close();

		// *** recheck ram
		synapse_control->set_mmask(0x00f00f); // only one block per network block
		synapse_control->read_sram(trow, tcol, tdel);
		synapse_control->check_sram(0);
		synapse_control->read_sram(trow, tcol);
		bus->dbg(0) << "SynapseControl 2. SRAM test: " << synapse_control->check_sram(tda) << endl;
		synapse_control->close();
		synapse_control->read_sram(trow + 1, tcol, tdel);
		synapse_control->check_sram(0);
		synapse_control->read_sram(trow + 1, tcol);
		bus->dbg(0) << "SynapseControl 2. SRAM test: " << synapse_control->check_sram(tdb) << endl;
		synapse_control->close();

		// *** correlation readout ***
		/* initial values:
		    corrmode: acausal
		    synapse corr measurement capacitors (block row col = causal acausal  Vtlow Vthigh
		   result):
		    0 0 0 = 0.5 0.8  1.0 1.1  -> 0
		    0 1 0 = 0.8 0.5           -> 1
		    1 0 0 = 0.5 0.8  1.0 1.1  -> 0
		    1 1 0 = 0.8 0.5           -> 1
		    bias_block.Ibreadcorrb V=1.1
		    all Vt=1
		*/
		synapse_control->set_mmask((1 << 3) + (1 << 15)); // only one column active per block
		                                                  // (b0:bit15, b1:bit3)
		// causal
		synapse_control->read_corr(trow, tcol + (1 << bus->hw_const->sc_rowconfigbit()), tdelrc);
		synapse_control->check_corr(0);
		synapse_control->read_corr(trow, tcol + (1 << bus->hw_const->sc_rowconfigbit()), 2);
		bus->dbg(0) << "SynapseControl causal correlation readout test: "
		            << synapse_control->check_corr(0) << endl;
		synapse_control->close();
		synapse_control->read_corr(trow + 1, tcol + (1 << bus->hw_const->sc_rowconfigbit()),
		                           tdelrc);
		synapse_control->check_corr(0);
		synapse_control->read_corr(trow + 1, tcol + (1 << bus->hw_const->sc_rowconfigbit()), 2);
		bus->dbg(0) << "SynapseControl causal correlation readout test: "
		            << synapse_control->check_corr((1 << 3) + (1 << 15)) << endl;
		synapse_control->close();
		// acausal
		synapse_control->read_corr(trow, tcol, tdelrc);
		synapse_control->check_corr(0);
		synapse_control->read_corr(trow, tcol, 2);
		bus->dbg(0) << "SynapseControl acausal correlation readout test: "
		            << synapse_control->check_corr((1 << 3) + (1 << 15)) << endl;
		synapse_control->close();
		synapse_control->read_corr(trow + 1, tcol, tdelrc);
		synapse_control->check_corr(0);
		synapse_control->read_corr(trow + 1, tcol, 2);
		bus->dbg(0) << "SynapseControl acausal correlation readout test: "
		            << synapse_control->check_corr(0) << endl;
		synapse_control->close();

		// *** process correlation test ***
		uint startrow = trow, stoprow = trow + 1,
		     // initial synapse values: ssynblk_row
		    ssyn0_0 = 10, ssyn0_1 = 6, ssyn1_0 = 13, ssyn1_1 = 3;

		for (uint i = 0; i < 8; i++) {
			synapse_control->write_sram(trow, i * 8, ssyn1_0 + (ssyn0_0 << 12),
			                            i == 0 ? tdel : SynapseControl::synramdelay);
			synapse_control->write_sram(trow, 7 + i * 8, ssyn1_0 + (ssyn0_0 << 12));
		}
		synapse_control->close();
		for (uint i = 0; i < 8; i++) {
			synapse_control->write_sram(trow + 1, i * 8, ssyn1_1 + (ssyn0_1 << 12),
			                            i == 0 ? tdel : SynapseControl::synramdelay);
			synapse_control->write_sram(trow + 1, 7 + i * 8, ssyn1_1 + (ssyn0_1 << 12));
		}
		synapse_control->close();

		synapse_control->proc_corr(startrow, stoprow, true,
		                           tdel + 2 * (tpcorperiod + 4 * 32)); // with auto-close, 32 is for
		                                                               // the number of hits

		synapse_control->set_mmask(0x00f000); // chekc block one only
		synapse_control->read_sram(trow, tcol, tdelrc);
		synapse_control->check_sram(0);
		synapse_control->read_sram(trow, tcol, 2);
		bus->dbg(0) << "SynapseControl correlation processing test: "
		            << synapse_control->check_sram((ssyn0_0 - 1) << 12) << endl; // acausal should
		                                                                         // be decreased
		synapse_control->close();
		synapse_control->read_sram(trow + 1, tcol, tdelrc);
		synapse_control->check_sram(0);
		synapse_control->read_sram(trow + 1, tcol, 2);
		bus->dbg(0) << "SynapseControl correlation processing test: "
		            << synapse_control->check_sram((ssyn0_1 + 1) << 12) << endl; // causal should be
		                                                                         // increased
		synapse_control->close();
		// now read all
		synapse_control->set_mmask(0x00f00f); // now use both blocks
		for (uint i = 0; i < 8; i++) {
			synapse_control->read_sram(trow, i * 8, i == 0 ? tdel : ControlInterface::synramdelay);
			synapse_control->read_sram(trow, 7 + i * 8);
		}
		synapse_control->close();
		for (uint i = 0; i < 8; i++) {
			synapse_control->read_sram(trow + 1, i * 8,
			                           i == 0 ? tdel : ControlInterface::synramdelay);
			synapse_control->read_sram(trow + 1, 7 + i * 8);
		}
		synapse_control->read_sram(trow + 1, 7 + 7 * 8);
		synapse_control->close();
		// now check
		synapse_control->check_sram(0); // synchronize reads
		bool ok = true;
		for (uint i = 0; i < 8; i++) {
			bus->dbg(3) << (ok &= synapse_control->check_sram((ssyn1_0 - 1) | (ssyn0_0 - 1) << 12));
			bus->dbg(3) << (ok &= synapse_control->check_sram((ssyn1_0 + 1) | (ssyn0_0 - 1) << 12));
		}

		for (uint i = 0; i < 8; i++) {
			bus->dbg(3) << (ok &= synapse_control->check_sram((ssyn1_1 + 1) | (ssyn0_1 + 1) << 12));
			bus->dbg(3) << (ok &= synapse_control->check_sram((ssyn1_1 - 1) | (ssyn0_1 + 1) << 12));
		}
		bus->dbg(3) << endl;
		bus->dbg(0) << "SynapseControl correlation processing test 2: " << ok
		            << endl; // causal should be increased
		return true;
	}; // end TMMixedlong.test()
};


class TMEvent : public Testmode
{
public:
	bool test()
	{
		boost::shared_ptr<ChipControl> chip_control = chip[0]->getCC(); // get pointer to chip
		                                                                // control class
		//	boost::shared_ptr<ChipControl> chip_control1 = chip[1]->getCC();		//get pointer to
		//chip control class

		chip_control->setCtrl((1 << bus->hw_const->cr_anaclken()) +
		                          (1 << bus->hw_const->cr_anaclkhien()) +
		                          (63UL << bus->hw_const->cr_eout_rst()),
		                      2); // start evin clock and keep resets
		chip_control->setCtrl((1 << bus->hw_const->cr_anaclken()) +
		                          (1 << bus->hw_const->cr_anaclkhien()),
		                      2); // remove fifo resets
		//	chip_control1->setCtrl((1<<bus->hw_const->cr_anaclken())|(1<<(bus->hw_const->cr_anaclkhien())));

		//	uint nadr=0; //neuron address: 64 bits within block, 3 bits block number
		uint time = 0x20; //,time2=0x60; //target cycle to deliver event (buscycle it was send is
		                  //time==0)

		//	uint subtime=7; //1/16th withing target cycle
		//	for(uint i=0;i<16;i++)chip[0]->sendEvent(nadr+i,time+i&1,15-i);
		//	for(uint i=0;i<16;i++)chip[0]->sendEvent(nadr+i,time2+i&1,15-i);

		for (uint i = 0; i < 2000; i++) {
			chip[0]->sendEvent(rand() % 192 + (rand() % 2 * 256), time + rand() % 2, rand() % 16,
			                   rand() % 2);
		}
		//	for(uint
		//i=0;i<500;i++){chip[1]->sendEvent(rand()%192+(rand()%2*256),time2+rand()%2,rand()%16,rand()%2);}
		//	chip->sendEvent(10,time,5,4);
		//	chip1->sendEvent(10,time,5,4);

		//	for(uint i=0;i<32;i++){chip[0]->sendEvent(10, time, rand()%16,0);}

		// empty evout buffers
		chip[0]->sendIdleEvent(512);
		//	chip[1]->sendIdleEvent(512);
		for (uint i = 0; i < 60; i++)
			chip[0]->sendIdleEvent(0);
		//	for(uint i=0;i<60;i++)chip[1]->sendIdleEvent(0);
		//	for(uint i=0;i<16;i++)chip[0]->sendEvent(nadr+i,time,15-i);
		uint errnum;
		cout << chip[0]->checkEvents(errnum) << " event number:" << errnum << endl;
		return true;
	}; // end TMEvent.test()
};


// Listees
class LteeEvent : public Listee<Testmode>
{
public:
	LteeEvent() : Listee<Testmode>(string("event"), string("test random events with modelsim")){};
	Testmode* create() { return new TMEvent(); };
} ListeeEvent;

class LteeDelay : public Listee<Testmode>
{
public:
	LteeDelay() : Listee<Testmode>(string("delay"), string("test delays with modelsim")){};
	Testmode* create() { return new TMDelay(); };
} ListeeDelay;

class LteeMixedlong : public Listee<Testmode>
{
public:
	LteeMixedlong()
	    : Listee<Testmode>(string("mixedlong"), string("long synapse control test with spectre")){};
	Testmode* create() { return new TMMixedlong(); };
} ListeeMixedlong;

class LteeShortStart : public Listee<Testmode>
{
public:
	LteeShortStart()
	    : Listee<Testmode>(string("shortstart"), string("short start for mixedmode simulation")){};
	Testmode* create() { return new TMShortStart(); };
} ListeeShortStart;
