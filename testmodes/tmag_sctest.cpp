// Johannes' testmodes for test of SPIKEY1

#include "common.h" // library includes
#include <numeric>

// functions defined in getch.c
int getch(void);
int kbhit(void);

#include "idata.h"
#include "sncomm.h"
#include "sc_sctrl.h"
#include "sc_pbmem.h"

#include "ctrlif.h"
#include "spikenet.h"
#include "testmode.h"        //Testmode and Lister classes
#include "pram_control.h"    //needed for constant definitions
#include "synapse_control.h" //needed for constant definitions
#include "spikeyconfig.h"
#include "spikey.h"


using namespace spikey2;


// test synapse control
class TMSCTest : public Testmode
{
public:
	// calibration
	uint period;
	uint clockper;
	uint tsense;
	uint tpcsec;
	uint tdel;
	uint tpcorperiod;
	float slope;
	float iref, imax;

	TMSCTest()
	{
		// timing parameters
		clockper = 10; // nanosecs

		tsense = 100 / clockper; // 100 ns
		tpcsec = 20 / clockper; // 20ns
		tdel = 6 + tsense + tpcsec;
		tpcorperiod = 2 * tdel;
	};


	bool test()
	{
		boost::shared_ptr<SynapseControl> sc = chip[0]->getSC(); // get pointer to synapse control
		                                                         // class

		uint64_t status = 0;
		vector<bool> spikeystatus(32, false);
		for (uint i = 0; i < bus->hw_const->event_outs(); i++)
			spikeystatus[bus->hw_const->cr_eout_rst() + i] = false;
		for (uint i = 0; i < bus->hw_const->event_ins(); i++)
			spikeystatus[bus->hw_const->cr_einb_rst() + i] = false;
		for (uint i = 0; i < bus->hw_const->event_ins(); i++)
			spikeystatus[bus->hw_const->cr_ein_rst() + i] = false;
		spikeystatus[bus->hw_const->cr_pram_en()] = true;
		spikeystatus[bus->hw_const->cr_anaclken()] = true;
		spikeystatus[bus->hw_const->cr_anaclkhien()] = true;
		for (uint i = 0; i < 32; i++)
			status |= spikeystatus[i] << i;

		const uint defdel = 4; // default delay
		const uint dacdel = 100 * 10;
		char tm;
		int i;
		// as what to do...
		do {
			cout << "Test what?" << endl;
			cout << "4: Read row/column config bits" << endl;
			cout << "5: Initialize config bits and synapse ram" << endl;
			cout << "9: Set individual row/column config bits" << endl;
			cout << "0: Test synapse control" << endl;
			cout << "x: exit" << endl;
			uint trow, loops, errors;
			uint64_t d;

			cin >> dec >> tm;
			bool execute_cmd;
			do { // commandloop (non interactive)
				execute_cmd = false;
				switch (tm) {
					case '4':
						for (uint r = 0; r < 256; ++r) {
							sc->read_sram(r, 1 << bus->hw_const->sc_rowconfigbit());
							sc->close();
						}
						for (uint c = 0; c < 64; ++c) {
							sc->read_sram(0, (1 << bus->hw_const->sc_neuronconfigbit()) | c);
						}
						sc->close();

						chip[0]->Flush();
						chip[0]->Run();

						cout << "Row config information:" << endl;
						for (uint r = 0; r < 256; ++r) {
							sc->rcv_data(d);
							d = (d >> (bus->hw_const->sc_aw() + bus->hw_const->sc_commandwidth())) &
							    mmw(bus->hw_const->sc_dw());
							cout << hex << setw(6) << d;
							if ((r + 1) % 8)
								cout << " ";
							else
								cout << endl;
						}

						cout << "Neuron config information:" << endl;
						for (uint c = 0; c < 64; ++c) {
							sc->rcv_data(d);
							d = (d >> (bus->hw_const->sc_aw() + bus->hw_const->sc_commandwidth())) &
							    mmw(bus->hw_const->sc_dw());
							cout << hex << setw(6) << d;
							if ((c + 1) % 8)
								cout << " ";
							else
								cout << endl;
						}
						break;
					case '5': {
						cout << "row config pattern + syn config pattern (hex)? ";
						uint rcval, nsval, ncval = 0;
						cin >> hex >> rcval >> nsval; //(sc_sdd_evin|sc_sdd_senx|sc_sdd_cap2);
						// d=0;for(uint i=0;i<6;i++)d|=(rcval&15)<<(i*4);
						for (uint r = 0; r < 256; ++r) {
							sc->write_sram(r, 1 << bus->hw_const->sc_rowconfigbit(),
							               (rcval & 0xff) |
							                   ((rcval & 0xff) << bus->hw_const->sc_blockshift()));
							sc->close();
						}
						d = 0;
						for (uint i = 0; i < 6; i++)
							d |= (ncval & 15) << (i * 4);
						for (uint c = 0; c < 64; ++c) {
							sc->write_sram(0, (1 << bus->hw_const->sc_neuronconfigbit()) | c, d);
						}
						sc->close();
						for (uint r = 0; r < 256; ++r) {
							for (uint c = 0; c < 64; ++c) {
								d = 0;
								for (uint i = 0; i < 6; i++)
									d |= (nsval & 15) << (i * 4);
								sc->write_sram(r, c, d);
							}
							sc->close();
						}
					}
						tm = '4';
						execute_cmd = true;
						break;
					case '9': {
						cout << "d(efault) 0 0, r +row+ row config pattern(hex), c +col+ neuron "
						        "config pattern(hex), n + startn + c(lear 0/1)? ";
						uint val, rowcol, d = 0;
						char m;
						cin >> m >> dec >> rowcol >> hex >>
						    val; //(sc_sdd_evin|sc_sdd_senx|sc_sdd_cap2);
						switch (m) {
							case 'd':
								rowcol = 0;
								d = 1;
							case 'n':
								if (d) { // clear
									for (uint c = 0; c < 64; ++c)
										sc->write_sram(
										    0, (1 << bus->hw_const->sc_neuronconfigbit()) | c, d);
								}
								sc->write_sram(0,
								               (1 << bus->hw_const->sc_neuronconfigbit()) | rowcol,
								               0x0f00f0);
								sc->write_sram(0, (1 << bus->hw_const->sc_neuronconfigbit()) |
								                      (rowcol + 1),
								               0x0f00f0);
								sc->write_sram(0, (1 << bus->hw_const->sc_neuronconfigbit()) |
								                      (rowcol + 2),
								               0x0f00f0);
								sc->write_sram(0, (1 << bus->hw_const->sc_neuronconfigbit()) |
								                      (rowcol + 3),
								               0x0f00f0);
								sc->close();
								break;
							case 'r':
								sc->write_sram(rowcol, (1 << bus->hw_const->sc_rowconfigbit()),
								               (val & 0xff) | ((val & 0xff)
								                               << bus->hw_const->sc_blockshift()));
								sc->close();
								break;
							case 'c':
								sc->write_sram(0,
								               (1 << bus->hw_const->sc_neuronconfigbit()) | rowcol,
								               (val & 0xfff) | ((val & 0xfff) << 12));
								sc->close();
								break;
						}
					}
						tm = '4';
						execute_cmd = true;
						break;
					default:
						if (tm != 'x')
							cout << "False input!!!" << endl;
				} // end switch
			} while (execute_cmd);
		} while (tm != 'x');
		return true;
	}; // end test() method

}; // end testmode


class LteeSCTest : public Listee<Testmode>
{
public:
	LteeSCTest()
	    : Listee<Testmode>(string("sctest"), string("Test synapse control with real spikey")){};
	Testmode* create() { return new TMSCTest(); };
} ListeeSCTest;
