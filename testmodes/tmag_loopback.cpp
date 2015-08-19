// initialized usb board
// 11.04.2012, Andreas Gruebl

#include "common.h" // library includes
#include "idata.h"
#include "sncomm.h"
#include "spikenet.h"
#include "testmode.h" //Testmode and Lister classes

#include "sc_sctrl.h"
#include "sc_pbmem.h"

// only if needed
#include "ctrlif.h"
#include "pram_control.h" //parameter ram etc

class TMAGLoopback : public Testmode
{
public:
	bool test()
	{
		boost::shared_ptr<Loopback> loopback = chip[0]->getLB();

		// SC_Mem *scm = dynamic_cast<SC_Mem *>(bus);

		// perform some loopback checks:
		bool result = true;

		uint lbtests = 0;
		cout << "Enter number of loops:";
		cin >> lbtests;
		uint outerloops = 0;
		cout << "Enter number of outer loops:";
		cin >> outerloops;

		uint64_t pattern = 0;
		uint del = 0;
		for (uint ol = 0; ol < outerloops; ol++) {
			if (ol > 0)
				chip[0]->Clear(); // free playback memory
			uint randomseed = time(NULL);
			srand(randomseed);
			for (uint i = 0; i < lbtests; i++) {
				pattern = rand() + uint64_t((rand() & 0x1fffffULL) << 32);
				// pattern = i*2;
				// pattern = 0x2000;
				del = (rand() % 511) + 1;
				loopback->loopback(pattern, del);
				if (!(i % 1000))
					cout << "\rloop :" << i << flush;
			}

			chip[0]->Flush();
			chip[0]->Run();

			srand(randomseed);
			uint errpos = 0;
			cout << "verifying..." << endl;
			for (uint i = 0; i < lbtests; i++) {
				// if(i==0)sleep(5);	// wait for playback memory;
				pattern = rand() + uint64_t((rand() & 0x1fffffULL) << 32);
				// pattern = i*2;
				// pattern = 0x2000;
				del = (rand() % 511) + 1; // just to reproduce rnds
				if (!loopback->check_test(pattern)) {
					bus->dbg(Logger::DEBUG1) << "Loopback failed with pattern 0x" << hex << pattern
					                         << " in loop " << i << endl;
					if (errpos == 0)
						errpos = i;
					result = false;
				}
			}
			if (!result)
				cout << "loop " << ol << " **** failed **** , errpos: " << errpos << endl;
			else
				cout << "loop " << ol << " **** ok ****" << endl;
		}
		return result;
	};
};


class LteeLoopback : public Listee<Testmode>
{
public:
	LteeLoopback() : Listee<Testmode>(string("loopback"), string("Spikey loopback test")){};
	Testmode* create() { return new TMAGLoopback(); };
} ListeeLoopback;
