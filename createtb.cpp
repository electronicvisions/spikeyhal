// testbench generation for spikenet simulations

#include "logger.h"
#include "common.h" // library includes
#include "idata.h"
#include "sncomm.h"
#include "sc_sctrl.h"
#include "sc_pbmem.h"
#include "ctrlif.h"
#include "spikenet.h"
#include "testmode.h"

#include <log4cxx/logger.h>
#include <log4cxx/basicconfigurator.h>
#include <log4cxx/fileappender.h>
#include <log4cxx/simplelayout.h>

static log4cxx::LoggerPtr logger = log4cxx::Logger::getLogger("HAL.CTB");

using namespace spikey2;

const char usagestr[] =
    "usage: createtb [-option [Argument]]\n"
    "options: -w workstation        use specified workstation (with playback memory bus model)\n"
    "         -s starttime          starttime for testbench (used to sync chips)\n"
    "         -sr seed              seed value for random number generation\n"
    "         -l                    list testmodes\n"
    "         -m name               uses testmode\n"
    "         -v / -d               verbose output / debug output\n";

template <>
Listee<Testmode>* Listee<Testmode>::first = NULL; // this allows access for all Listees

uint randomseed = 42;

int main(int argc, char* argv[])
{
	// configure logger
	std::string loggerFilename = "logfile_testmodes.txt";
	log4cxx::LayoutPtr layout = new log4cxx::SimpleLayout();
	log4cxx::AppenderPtr file = new log4cxx::FileAppender(layout, loggerFilename, false);
	log4cxx::BasicConfigurator::configure(file);
	static log4cxx::LoggerPtr logger = log4cxx::Logger::getLogger("Tst");
	logger->setLevel(log4cxx::Level::getInfo());

	// check for options
	enum options { none, opthelp, opttime, sr, optmode, listmodes, nathan } actopt = none;
	// available bus models
	enum busmodels { pbusb } busmodel = pbusb;
	uint starttime = 0;
	std::string workstation = "";

	vector<string> modes; // testmodes to use
	size_t loglevel = Logger::INFO;
	for (int j = 1; j <= argc - 1; j++) {
		//		printf("%d argv[%d]:%s\n",actopt,j,argv[j]);
		if ((actopt == none) && !strcmp(argv[j], "-v")) {
			loglevel = Logger::DEBUG0;
			continue;
		}
		if ((actopt == none) && !strcmp(argv[j], "-d")) {
			loglevel = Logger::DEBUG3;
			continue;
		}
		if ((actopt == none) && !strcmp(argv[j], "-s")) {
			actopt = opttime;
			continue;
		}
		if ((actopt == none) && !strcmp(argv[j], "-?")) {
			actopt = opthelp;
		}
		if ((actopt == none) && !strcmp(argv[j], "-h")) {
			actopt = opthelp;
		}
		if ((actopt == none) && !strcmp(argv[j], "-sr")) {
			actopt = sr;
			continue;
		}
		if ((actopt == none) && !strcmp(argv[j], "-w")) {
			busmodel = pbusb;
			if (j < argc - 1)
				actopt = nathan;
			continue;
		}
		if ((actopt == none) && !strcmp(argv[j], "-m")) {
			actopt = optmode;
			continue;
		}
		if (actopt == opttime) {
			sscanf(argv[j], "%ud", &starttime);
			actopt = none;
			continue;
		}
		if (actopt == optmode) {
			modes.push_back(string(argv[j]));
			actopt = none;
			continue;
		} // store testmode
		if (actopt == sr) {
			if (argv[j][0] == 't')
				randomseed = time(NULL);
			else
				randomseed = atoi(argv[j]);
			srand(randomseed);
			srandom(randomseed);
			actopt = none;
			continue;
		}
		if ((actopt == none) && !strcmp(argv[j], "-l")) {
			Listee<Testmode>* t = Listee<Testmode>::first;
			cout << "Available Testmodes:" << endl;
			while (t != NULL) {
				cout << "\"" << t->name() << "\" \"" << t->comment() << "\"" << endl;
				t = t->getNext();
			}
			exit(EXIT_SUCCESS);
		}

		if (actopt == nathan) {
			if (argv[j][0] != '-') {
				workstation = string(argv[j]);
			} else
				--j;

			actopt = none;
			continue;
		}

		if (actopt == none)
			cout << "Illegal options." << endl;
		cout << usagestr << endl;
		if (actopt != opthelp)
			exit(EXIT_FAILURE);
		else
			exit(EXIT_SUCCESS);
	}
	if (actopt != none) {
		cout << "Missing argument (use -? for more information)." << endl;
		exit(EXIT_FAILURE);
	}

	// if logger has not been initialized because of -v or -d option,
	// we'll initialize it here with INFO level. If it has been initialized,
	// the next line actually changes nothing
	// Logger& dbg = Logger::instance(loglevel,"../output/logfile.txt",true);
	Logger& dbg = Logger::instance("main", loglevel);
	dbg(Logger::INFO) << "Changed loglevel to " << loglevel;

	vector<boost::shared_ptr<SpikenetComm>> busv;
	boost::shared_ptr<SpikenetComm> bus;
	vector<boost::shared_ptr<Spikenet>> chip;
	vector<boost::shared_ptr<Spikenet>> chipv;

	// ECM: removed multiple chips support here...

	// single chip
	switch (busmodel) {
		case pbusb:
			bus = boost::shared_ptr<SC_Mem>(new SC_Mem(starttime, workstation)); // playbackmem
			                                                                     // interface module
			                                                                     // using msgqueue
			break;
	}

	busv.push_back(bus);

	chip.push_back(boost::shared_ptr<Spikenet>(new Spikenet(bus, 0))); // chip number 0
	chip.push_back(boost::shared_ptr<Spikenet>(new Spikenet(bus, 1))); // chip number 1
	chipv.push_back(chip[0]);

	// ***** start testmodes ****

	for (vector<string>::iterator num = modes.begin(); num != modes.end(); ++num) {
		Testmode* t = Listee<Testmode>::first->createNew(*num); // try to create testmode
		if (t == NULL) {
			cout << "Unknown testmode! Use -l for list." << endl;
			return (EXIT_FAILURE);
		}
		// initialize testmode
		t->chip = chip;
		t->bus = bus;
		t->busv = busv;
		t->chipv = chipv;
		bool res = t->test(); // perform tests
		LOG4CXX_INFO(logger, "Testmode: " << *num << " result:" << res << endl);
		delete t;
	}

	// ***** end test *****
	return (EXIT_SUCCESS);
}
