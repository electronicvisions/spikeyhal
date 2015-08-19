// Andi's testmodes for test of SPIKEY1

#include "common.h" // library includes
#include <numeric>

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

class TMSpikeyClassAG : public Testmode
{
public:
	boost::shared_ptr<Spikey> sp;

	uint bool2int(vector<bool> array)
	{
		uint res = 0;
		for (uint i = 0; i < bus->hw_const->cr_width(); i++)
			res |= (array[i] << i);
		return res;
	}

	bool checkEvents(uint& firsterr, SpikeTrain& tx, SpikeTrain& rx,
	                 boost::shared_ptr<SC_SlowCtrl> sct)
	{
		firsterr = 0; // initialize error position
		const vector<IData>& rcvd = rx.d;
		// const vector<IData> & ev=tx.d;
		const vector<IData>* ev = sct->ev(0);
		if (ev->size() != 0) {
			uint i, j, neuron, time, start;
			int smallestmatch;
			vector<bool> matched(rcvd.size(), false);
			//		for(j=0;j<rcvd->size();j++)matched[j]=false;//clear all matches
			smallestmatch = -1;
			for (i = 0; i < ev->size() - 1;
			     i++) { // ignore the last event as this can not be received anyway
				neuron = (*ev)[i].neuronAdr();
				time = (*ev)[i].time();
				bus->dbg(3) << "Looking for: n=" << hex << neuron << " t=" << time;
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
					bus->dbg(3) << "|" << hex << d.neuronAdr() << ":" << d.time();
					if (time == d.time() && neuron == d.neuronAdr()) // ok, found correct answer
					{
						matched[j] = true;
						bus->dbg(3) << "...found in index: " << j << endl;
						break;
					}
				}
				if (j >= rcvd.size()) {
					bus->dbg(3) << "...not found!";
					break;
				} // not found in received
			}
			if (i < ev->size() - 1) {
				firsterr = i;
				bus->dbg(3) << "...not found. exiting." << endl;
				return false;
			} // not every entry matched
			return true; // everything matched
		}
		return false;
	}

	bool test()
	{
		// param file name
		stringstream s;
		boost::shared_ptr<SC_SlowCtrl> scl;
		// Spikey object
		sp = boost::shared_ptr<Spikey>(new Spikey(chip[0]));
		boost::shared_ptr<SC_Mem> mem(boost::dynamic_pointer_cast<SC_Mem>(chip[0]->bus));
		if (mem)
			scl = mem->getSCTRL(); // bus is sc_mem
		else
			scl = boost::dynamic_pointer_cast<SC_SlowCtrl>(chip[0]->bus);
		s << "./components/SpikeyHAL/spikeyconfig0";
		//		if(scl)s<<scl->nathanNum();
		s << ".dat";
		ofstream co("spikeyconfig_allfire.dat");
		string paramname(s.str());
		boost::shared_ptr<SpikeyConfig> cfg(new SpikeyConfig(bus->hw_const)); // must be valid all
		                                                                      // the time
		boost::shared_ptr<SpikeyConfig> cfg2(new SpikeyConfig(bus->hw_const)); // must be valid all
		                                                                       // the time

		boost::shared_ptr<ChipControl> chip_control = chip[0]->getCC(); // get pointer to chip
		                                                                // control class
		boost::shared_ptr<SynapseControl> sc = chip[0]->getSC(); // get pointer to synapse control
		                                                         // class
		boost::shared_ptr<AnalogReadout> analog_readout = chip[0]->getAR(); // get pointer to analog
		                                                                    // readout class

		char c;
		bool cont = true;
		do {
			cout << "1	check calibration (needs spikeyconfigx.dat and testconfig.dat)" << endl;
			cout << "2	update config" << endl;
			cout << "3	calibrate to 2mV" << endl;
			cout << "4	send single event" << endl;
			cout << "5	sample poisson spike train" << endl;
			cout << "6	gen. nice membrane trace" << endl;
			cout << "7	(re-)set analog readout chain" << endl;
			cout << "8	test Spikey2 event loopback" << endl;
			cout << "9	determine maximum event rate with random events" << endl;
			cout << "a	like 9, with automatics" << endl;
			cout << "b	check (new 20.07.07) software event dropping" << endl;
			cout << "c	produce maximum possible output rate" << endl;
			cout << "d	search for erroneous events" << endl;
			cout << "e	set anamux" << endl;
			cout << "f	set/unset control reg" << endl;
			cout << "g	set/unset dll and neuron reset" << endl;
			cout << "h	loop testing all time bins (on synapse row 1 -> pre/sel1)" << endl;
			cout << "i	read back one synapse ram address" << endl;
			cout << "j	tune all neurons of one block to fire once per one input" << endl;
			cout << "x	exit" << endl;
			cin >> c;
			switch (c) {
				case 'j': {
					uint block;
					SpikeTrain t, r;
					uint starttime = 0x10000;
					uint disttime = 0x1000;

					// a "valid" configuration sets rows 0, 64, 128, etc. to excitatory and sets
					// some
					// weight on the according synapses. For ease of use, all neurons should
					// be disabled. The analog parameters should be set in a way that it is possible
					// to fire all neurons with the simultaneous input of four neurons.
					cout << "Please ensure that a valid config has been loaded!" << endl;
					cout << "Tune which block (physically: 2,1,0|3,4,5)?";
					cin >> block;
					uint startneuron =
					    (block >= SpikeyConfig::num_prienc)
					        ? (SpikeyConfig::num_neurons +
					           ((block - SpikeyConfig::num_prienc) * SpikeyConfig::num_perprienc))
					        : (block * SpikeyConfig::num_perprienc);

					// in the following loop, the weight of the synapses connected to one
					// neuron can be manipulated. i-increase; d-decrease; n-next neuron
					bool quit = false;
					for (uint n = startneuron; n < startneuron + SpikeyConfig::num_perprienc; n++) {
						// enable membrane voltage out
						scl->setAnaMux(block >= SpikeyConfig::num_prienc ? (4 + (n % 4)) : (n % 4));

						for (uint k = 0; k < SpikeyConfig::num_neurons * 2; ++k) {
							if (k == n)
								cfg->neuron[k].config = 0x6; // enable
							else
								cfg->neuron[k].config = 0x0; // disable
						}
						uint weight = 36; // this is the sum of all 4 weights
						cout << "Testing neuron " << n << ", select="
						     << (block >= SpikeyConfig::num_prienc ? (4 + (n % 4)) : (n % 4))
						     << endl;
						while (true) {
							// set weights of column according to current neuron
							uint iw = weight;
							cout << "Total weight value: " << iw << endl;
							valarray<ubyte> syn(SpikeyConfig::num_presyns);
							cout << "weights in column " << n << ": ";
							for (uint w = 0; w < 256; w++) {
								if (!(w % 64)) {
									uint sw = (iw > 15) ? 15 : iw;
									if (iw >= 15)
										iw -= 15;
									else
										iw = 0;
									syn[w] = sw;
									cout << w << "=" << (uint)syn[w] << " -- ";
								} else
									syn[w] = 0;
							}
							cout << endl;
							if (block < 3)
								cfg->col((block >= SpikeyConfig::num_prienc ? 1 : 0), n) = syn;
							else
								cfg->col((block >= SpikeyConfig::num_prienc ? 1 : 0),
								         n - SpikeyConfig::num_neurons) = syn;
							sp->config(cfg); // send config to chip

							// generate spiketrains
							t.d.clear();
							r.d.clear();
							for (uint i = 0; i < 4; i++)
								t.d.push_back(
								    IData::Event((block < 3 ? 0 : 256) + i * 64, starttime));
							for (uint i = 0; i < 4; i++)
								t.d.push_back(IData::Event((block < 3 ? 0 : 256) + i * 64,
								                           starttime + disttime));
							t.d.push_back(
							    IData::Event(255, starttime + disttime + 0x10000)); // end reference

							sp->sendSpikeTrain(t);
							sp->Flush();
							sp->Run();
							sp->waitPbFinished();

							sp->recSpikeTrain(r);
							uint rec = 0;
							for (uint i = 0; i < r.d.size(); i++) {
								if (block < 3) {
									if (r.d[i].neuronAdr() == n)
										rec++;
								} else {
									if (r.d[i].neuronAdr() == n + 0x40)
										rec++;
								}
							}
							cout << "received " << rec << " events from neuron " << n << "."
							     << endl;
							sp->Clear(); // brevent pointer wraps

							bool next = false;
							if (kbhit()) {
								switch (getch()) {
									case 'i':
										weight++;
										break;
									case 'd':
										weight--;
										break;
									case 'n':
										next = true;
										break;
									case 'x':
										quit = true;
										break;
								}
							}
							if (next || quit)
								break;
						}
						if (quit)
							break;
					}

					co << cfg;
					co.close();

				} break;
				case 'i': {
					uint row, col;
					uint64_t dd;

					cout << "Read row, col? ";
					cin >> row >> col;

					sc->read_sram(row, col, 100);
					sc->close();

					sc->rcv_data(dd);
					dd = (dd >> (bus->hw_const->sc_aw() + bus->hw_const->sc_commandwidth())) &
					     mmw(bus->hw_const->sc_dw());

					cout << "row,col: " << row << "," << col << " read:	" << hex << dd << endl;

				} break;
				case 'h': {
					SpikeTrain t, r;
					t.d.clear();
					r.d.clear();
					uint starttime = 0x10000;
					uint disttime = 0x1000;

					// generate events for input 1 and 257
					t.d.push_back(IData::Event(1, starttime));
					t.d.push_back(IData::Event(257, starttime));
					starttime += 2 * disttime;

					for (uint i = 0; i < 16; i++) {
						t.d.push_back(IData::Event(1, starttime + i));
						t.d.push_back(IData::Event(257, starttime + i));
						starttime += disttime;
					}

					starttime += disttime;
					t.d.push_back(IData::Event(1, starttime));
					t.d.push_back(IData::Event(257, starttime));

					cout << "Sending:" << endl
					     << t << endl;

					while (true) {
						sp->sendSpikeTrain(t);
						sp->Flush();
						sp->Run();
						sp->waitPbFinished();

						sp->recSpikeTrain(r);
						if (kbhit())
							break;
					}
					// cout<<endl<<t<<"Received: "<<dec<<r.d.size()<<endl<<r<<endl;
				} break;
				case 'g': {
					bool dr, nr;
					cout << "DLL reset (0/1)? ";
					cin >> dr;
					cout << "Neuron reset (0/1)? ";
					cin >> nr;
					sp->dllreset = dr;
					sp->neuronreset = nr;

					sp->writeSCtl();
					sp->Flush();
				} break;
				case 'f': {
					char k;
					cout << "Stop/start event clocks and pram enable" << endl;
					cout << "3: pram, 2:clk, 1:clkhi - 7: all on, 3: both clk on, 0: both off)?";
					cin >> k;

					sp->statusreg[bus->hw_const->cr_pram_en()] = (k & 0x3) >> 2;
					sp->statusreg[bus->hw_const->cr_anaclken()] = (k & 0x2) >> 1;
					sp->statusreg[bus->hw_const->cr_anaclkhien()] = k & 0x1;
					sp->writeCC();
					sp->Flush();

					cout << "wrote control reg: ";
					for (uint j = 32; j > 0; j--)
						cout << sp->statusreg[j - 1];
					cout << endl;
					cout << "wrote control reg: " << hex << sp->statusregToUint64() << endl;
				} break;
				case 'e': {
					uint select;
					cout << "Please give Vmem to select (0-7, 8=IBTEST)...";
					cin >> select;
					scl->setAnaMux(select);
				} break;
				case '1': {
					cout << "Input Precision, nadc, nmess, ndist: ";
					float prec;
					uint nmess, ndist, nadc;
					cin >> prec >> nadc >> nmess >> ndist;
					// initialize spikey config
					bus->dbg(7) << "Now loading " << paramname << endl;
					if (!cfg->readParam(paramname)) {
						bus->dbg(0) << "Reading SpikeyConfig " << paramname << " failed " << endl;
						return false;
					}
					if (!cfg2->readParam("testconfig.dat")) {
						bus->dbg(0) << "Reading SpikeyConfig testconfig.dat failed " << endl;
						break;
					}

					bus->dbg(7) << "successfull" << endl;
					// config spikey
					bus->dbg(7) << cfg;

					vector<float> ref0, ref1, cal0, cal1, nocal0, nocal1, f;
					cout.precision(3);
					float f0, f1;

					for (uint i = 0; i < 100; ++i) {
						if (rand() % 2)
							sp->config(cfg);
						else
							sp->config(cfg);
						f = sp->calibParam(prec, nadc, nmess, ndist); // calib to 1mV
						cout << "calib: " << f[0] << " " << f[1] << " res:" << f[2]
						     << " num:" << setw(5) << f[3] << endl;
						if (f[3] < 0)
							continue; // calib unsuccessfull, ignore for statistics
						ref0.push_back(f[0]);
						ref1.push_back(f[1]);
						mess(f0, f1, nadc);
						cal0.push_back(f0);
						cal1.push_back(f1);
						if (rand() % 2)
							sp->config(cfg);
						else
							sp->config(cfg);
						for (uint d = 0; d < ndist; d++)
							chip[0]->getPC()->write_pram(3071, 0, 0, 0); // disturb param update
						mess(f0, f1, nadc);
						nocal0.push_back(f0);
						nocal1.push_back(f1);
						if (kbhit()) {
							if (getch() == 'x')
								break; // exit loop
						}
					}
					cout << "Mean: cal0: " << mean(cal0) << " nocal0: " << mean(nocal0)
					     << " cal1: " << mean(cal1) << " nocal1: " << mean(nocal1) << endl;
					cout << "Sdev: cal0: " << sdev(cal0) << " nocal0: " << sdev(nocal0)
					     << " cal1: " << sdev(cal1) << " nocal1: " << sdev(nocal1) << endl;
				} break;
				case '2': {
					bus->dbg(7) << "Now loading " << paramname << endl;
					if (!cfg->readParam(paramname)) {
						cout << "Reading SpikeyConfig " << paramname << " failed " << endl;
					}
					bus->dbg(7) << "successfull" << endl;
					// config spikey
					bus->dbg(7) << cfg;
					sp->config(cfg);
					/*				float f1,f2;
					                mess(f1,f2);
					*/
					vector<float> v;
					v.clear();
					//				for(int
					//i=0;i<10;++i)v.push_back(chip[0]->readAdc(SBData::voutl));
					for (int i = 0; i < 10; ++i)
						v.push_back(chip[0]->readAdc(SBData::ibtest));
					cout << "ADC read: " << mean(v);
				} break;
				case '3': {
					vector<float> f = sp->calibParam(0.002); // calib to 1mV
					cout << "calib: " << f[0] << " " << f[1] << " res:" << f[2]
					     << " num:" << setw(5) << f[3] << endl;
					float f1, f2;
					mess(f1, f2);
				} break;
				case '4': {
					cout << "neuron number?";
					uint n, time;
					cin >> n;
					SpikeTrain t, r;
					time = 0x800 << 5; // 400MHz and timebins
					//				time=(scl->getSyncMax()+scl->getEvtLatency())<<5;//400MHz and
					//timebins
					t.d.push_back(IData::Event(n, time));
					t.d.push_back(IData::Event(n + 1, time));
					t.d.push_back(IData::Event(n + 2, time));
					t.d.push_back(IData::Event(n + 3, time));

					t.d.push_back(IData::Event(n + 64, time));
					t.d.push_back(IData::Event(n + 128, time));
					t.d.push_back(IData::Event(n + 192, time));

					t.d.push_back(IData::Event(300, time + 0x6000));

					t.d.push_back(IData::Event(n, time + 0x8000));
					t.d.push_back(IData::Event(n + 1, time + 0x8000));
					t.d.push_back(IData::Event(n + 2, time + 0x8000));
					t.d.push_back(IData::Event(n + 3, time + 0x8000));
					t.d.push_back(IData::Event(n + 64, time + 0x8000));
					t.d.push_back(IData::Event(n + 128, time + 0x8000));
					t.d.push_back(IData::Event(n + 192, time + 0x8000));

					/*				t.d.push_back(IData::Event(n+1,time));
					                t.d.push_back(IData::Event(n+2,time));
					                t.d.push_back(IData::Event(n,time+=(0x800<<4)));
					                t.d.push_back(IData::Event(n,time));
					                t.d.push_back(IData::Event(n+1,time));
					                t.d.push_back(IData::Event(n+2,time));
					                t.d.push_back(IData::Event(n,time+=(0x800<<4)));
					                t.d.push_back(IData::Event(n+1,time) );
					                t.d.push_back(IData::Event(n+2,time) );*/
					t.d.push_back(IData::Event(250, time + 0x10000 * 16)); // detmines length of
					                                                       // experiment
					while (!kbhit()) {
						sp->sendSpikeTrain(t, NULL, false);
						sp->Flush();
						sp->Run();
						sp->waitPbFinished();

						//				t.d.pop_back();//for better visibility
						sp->recSpikeTrain(r);
						sleep(1);
						/*				for(uint i=0;i<30;i++)chip_control->read_clkhistat();
						                for(uint i=0;i<30;i++){
						                    if(chip_control->rcv_data(spstatus)!=SpikenetComm::ok){bus->dbg(0)<<"read
						   error!"<<endl;break;}
						                    cout<<"clkhi status:  ";
						                    cout<<hex<<spstatus<<" ";
						                    binout(cout,(spstatus>>(bus->hw_const->cr_pos()+ev_timemsb_width+ev_clkpos_width+(4*event_outs))),1);cout<<"
						   ";
						                    binout(cout,(spstatus>>(bus->hw_const->cr_pos()+ev_clkpos_width+(4*event_outs))),ev_timemsb_width);cout<<"
						   ";
						                    binout(cout,(spstatus>>(bus->hw_const->cr_pos()+(4*event_outs))),ev_clkpos_width);cout<<"
						   ";
						                    for(uint i=event_outs;i>0;i--){
						                        binout(cout,(spstatus>>(bus->hw_const->cr_pos()+4*(i-1))),4);cout<<"
						   ";	// hard coded 4 status bits of clkhi fifos!
						                    }cout<<endl;
						                }*/

						//			for(uint i=0;i<r.d.size();++i)cout<<r.d[i];
						cout << endl
						     << t << "Received: " << dec << r.d.size() << endl
						     << r << endl;
					}
				} break;
				case '6': {
					cout << "distance between spikes?";
					uint time, dist;
					uint exc = 0;
					uint inh = 64;
					cin >> dist;
					SpikeTrain t, r;
					time = 0x200 << 5; // 400MHz and timebins
					//				time=(scl->getSyncMax()+scl->getEvtLatency())<<5;//400MHz and
					//timebins
					for (uint i = 0; i < 40; i++)
						t.d.push_back(IData::Event(exc + i, time += dist));
					for (uint i = 0; i < 30; i++)
						t.d.push_back(IData::Event(inh + i, time += dist));
					for (uint i = 0; i < 6; i++)
						t.d.push_back(IData::Event(exc + i, time += dist));

					t.d.push_back(IData::Event(exc, time + 0x3600 * 16)); // detmines length of
					                                                      // experiment
					sp->sendSpikeTrain(t);
					sp->Flush();
					sp->Run();
					sp->waitPbFinished();


					sp->recSpikeTrain(r);
					cout << endl
					     << t << "Received: " << dec << r.d.size() << endl
					     << r << endl;
				} break;
				case '8': {
					std::cout << "this test was moved to gtests tests/eventLoop.cpp" << std::endl;
				} break;
				case 'd': {
					vector<bool> spikeystatus(bus->hw_const->cr_width(), false);
					// enable clocks to analog_chip
					spikeystatus[bus->hw_const->cr_anaclken()] = true;
					spikeystatus[bus->hw_const->cr_anaclkhien()] = true;
					chip_control->setCtrl(bool2int(spikeystatus), 20);

					cout << "Max. distance between spikes and number of spikes?";
					uint time, dist, num, loops;
					cin >> dist >> num;
					cout << "Number of loops? ";
					cin >> loops;
					SpikeTrain t, r;
					srand(1);
					for (uint l = 0; l < loops; l++) {
						time = 0x200 << 5; // 400MHz and timebins
						//					time=(scl->getSyncMax()+scl->getEvtLatency())<<5;//400MHz and
						//timebins
						t.d.clear();
						r.d.clear();
						mem->Clear(); // free playback memory
						for (uint i = 0; i < num; i++) {
							time += rand() % dist;
							t.d.push_back(IData::Event(rand() % 192 + (rand() % 2 * 256), time));
						}

						t.d.push_back(IData::Event(0, time + 0x2000 * 16)); // detmines length of
						                                                    // experiment

						// set event fifo resets before any event processing starts
						for (uint i = 0; i < bus->hw_const->event_outs(); i++)
							spikeystatus[bus->hw_const->cr_eout_rst() + i] = true;
						for (uint i = 0; i < bus->hw_const->event_ins(); i++)
							spikeystatus[bus->hw_const->cr_einb_rst() + i] = true;
						for (uint i = 0; i < bus->hw_const->event_ins(); i++)
							spikeystatus[bus->hw_const->cr_ein_rst() + i] = true;
						chip_control->setCtrl(bool2int(spikeystatus), 20);

						// enable internal event loopback and reset event fifos
						chip_control->setEvLb((1 << bus->hw_const->el_enable_pos()),
						                      20); // turn on event loopback:

						// release event fifo resets
						for (uint i = 0; i < bus->hw_const->event_outs(); i++)
							spikeystatus[bus->hw_const->cr_eout_rst() + i] = false;
						for (uint i = 0; i < bus->hw_const->event_ins(); i++)
							spikeystatus[bus->hw_const->cr_einb_rst() + i] = false;
						for (uint i = 0; i < bus->hw_const->event_ins(); i++)
							spikeystatus[bus->hw_const->cr_ein_rst() + i] = false;
						chip_control->setCtrl(bool2int(spikeystatus), 20);


						sp->sendSpikeTrain(t);
						sp->Flush();
						sp->Run();
						sp->waitPbFinished();


						sp->recSpikeTrain(r);

						if (r.d.size() != num) {
							cout << "Received " << r.d.size() << " events! SpikeTrain..." << endl;
							cout << r << endl;
						}

						uint error = 0;
						cout << "Checking " << (t.d.size() - 1) << " events in loop " << l
						     << "... ";
						if (checkEvents(error, t, r, scl))
							cout << "ok :-)" << endl;
						else
							cout << "failed at event no. " << error << endl;
					}
				} break;
				case '9': {
					vector<uint> neurons(384, 0);
					for (uint i = 0; i < 192; i++) {
						neurons[i] = i;
						neurons[i + 192] = i + 256;
					}
					vector<bool> spikeystatus(bus->hw_const->cr_width(), false);
					// enable clocks to analog_chip
					spikeystatus[bus->hw_const->cr_anaclken()] = true;
					spikeystatus[bus->hw_const->cr_anaclkhien()] = true;
					chip_control->setCtrl(bool2int(spikeystatus), 20);

					uint time, dist, loops;
					double maxrate;

					cout << "Duration of one test?";
					cin >> dist;
					cout << "Max. rate and number of loops? ";
					cin >> maxrate >> loops;

					SpikeTrain t, tt, r;
					srand(1);
					for (uint l = 0; l < loops; l++) {
						time = 0x400;
						//					time=(scl->getSyncMax()+scl->getEvtLatency())<<5;//400MHz and
						//timebins
						t.d.clear();
						r.d.clear();
						// don't erase initialization commands...
						if (l > 0)
							mem->Clear(); // free playback memory

						// spiketrain, neurons, start, end, recovery, meanrate
						poissonTrain(t, neurons, time, time + dist, 0,
						             maxrate * (l + 1) / (loops)); // 20 Hz

						// add event at the end of the Spiketrain to catch all previous events in
						// FPGA.
						t.d.push_back(IData::Event(0, (time + dist + 0x8000) << 4)); // detmines
						                                                             // length of
						                                                             // experiment
						                                                             // (give time
						                                                             // in tbins)

						// set event fifo resets before any event processing starts
						for (uint i = 0; i < bus->hw_const->event_outs(); i++)
							spikeystatus[bus->hw_const->cr_eout_rst() + i] = true;
						for (uint i = 0; i < bus->hw_const->event_ins(); i++)
							spikeystatus[bus->hw_const->cr_einb_rst() + i] = true;
						for (uint i = 0; i < bus->hw_const->event_ins(); i++)
							spikeystatus[bus->hw_const->cr_ein_rst() + i] = true;
						chip_control->setCtrl(bool2int(spikeystatus), 20);

						// enable internal event loopback and reset event fifos
						chip_control->setEvLb((1 << bus->hw_const->el_enable_pos()),
						                      20); // turn on event loopback:

						// release event fifo resets
						for (uint i = 0; i < bus->hw_const->event_outs(); i++)
							spikeystatus[bus->hw_const->cr_eout_rst() + i] = false;
						for (uint i = 0; i < bus->hw_const->event_ins(); i++)
							spikeystatus[bus->hw_const->cr_einb_rst() + i] = false;
						for (uint i = 0; i < bus->hw_const->event_ins(); i++)
							spikeystatus[bus->hw_const->cr_ein_rst() + i] = false;
						chip_control->setCtrl(bool2int(spikeystatus), 20);

						tt.d.clear();
						sp->sendSpikeTrain(t, &tt);
						sp->Flush();
						sp->Run();
						sp->waitPbFinished();


						if (tt.d.size())
							cout << "dropped events: " << tt.d.size() << endl
							     << endl;

						sp->recSpikeTrain(r);
						bus->dbg(6) << endl
						            << t << "Received: " << dec << r.d.size() << endl
						            << r << endl;

						cout << "sent " << t.d.size() << ", received " << r.d.size() << "events."
						     << endl;
						uint error = 0;
						cout << "Checking " << (t.d.size() - 1) << " events in loop " << l
						     << "... ";
						if (checkEvents(error, t, r, scl))
							cout << "ok :-)" << endl;
						else
							cout << "failed at event no. " << error << endl;
					}
				} break;
				case 'c': {
					vector<uint> neurons(384, 0);
					for (uint i = 0; i < 192; i++) {
						neurons[i] = i;
						neurons[i + 192] = i + 256;
					}
					vector<bool> spikeystatus(bus->hw_const->cr_width(), false);
					// enable clocks to analog_chip
					spikeystatus[bus->hw_const->cr_anaclken()] = true;
					spikeystatus[bus->hw_const->cr_anaclkhien()] = true;
					chip_control->setCtrl(bool2int(spikeystatus), 20);

					uint time, dist, outneuron;

					cout << "Duration of one test?";
					cin >> dist;
					cout << "Start output neuron to bombard (within one 64-block)? ";
					cin >> outneuron;

					SpikeTrain t, tt, r;
					srand(1);

					time = 0x4000;
					//					time=(scl->getSyncMax()+scl->getEvtLatency())<<5;//400MHz and
					//timebins
					t.d.clear();
					r.d.clear();
					//					mem->Clear();//free playback memory

					// spiketrain, neurons, start, end, recovery, meanrate
					for (uint i = 0; i < dist; i++) {
						for (uint j = 0; j < bus->hw_const->event_outs(); j++) {
							uint offset = (j < 3) ? 0 : 64;
							t.d.push_back(IData::Event((outneuron + j * 64 + offset), time));
						}
						time += 0x10;
						outneuron++;
						outneuron %= 64;
					}

					t.d.push_back(IData::Event(0, (time + dist + 0x8000) << 4)); // detmines length
					                                                             // of experiment
					                                                             // (give time in
					                                                             // tbins)

					// set event fifo resets before any event processing starts
					for (uint i = 0; i < bus->hw_const->event_outs(); i++)
						spikeystatus[bus->hw_const->cr_eout_rst() + i] = true;
					for (uint i = 0; i < bus->hw_const->event_ins(); i++)
						spikeystatus[bus->hw_const->cr_einb_rst() + i] = true;
					for (uint i = 0; i < bus->hw_const->event_ins(); i++)
						spikeystatus[bus->hw_const->cr_ein_rst() + i] = true;
					chip_control->setCtrl(bool2int(spikeystatus), 20);

					// enable internal event loopback and reset event fifos
					chip_control->setEvLb((1 << bus->hw_const->el_enable_pos()), 20); // turn on
					                                                                  // event
					                                                                  // loopback:

					// release event fifo resets
					for (uint i = 0; i < bus->hw_const->event_outs(); i++)
						spikeystatus[bus->hw_const->cr_eout_rst() + i] = false;
					for (uint i = 0; i < bus->hw_const->event_ins(); i++)
						spikeystatus[bus->hw_const->cr_einb_rst() + i] = false;
					for (uint i = 0; i < bus->hw_const->event_ins(); i++)
						spikeystatus[bus->hw_const->cr_ein_rst() + i] = false;
					chip_control->setCtrl(bool2int(spikeystatus), 20);

					tt.d.clear();
					sp->sendSpikeTrain(t, &tt);
					sp->Flush();
					sp->Run();
					sp->waitPbFinished();


					if (tt.d.size())
						cout << "dropped events: " << tt.d.size() << endl
						     << endl;

					sp->recSpikeTrain(r);
					bus->dbg(6) << endl
					            << t << "Received: " << dec << r.d.size() << endl
					            << r << endl;

					cout << "sent " << t.d.size() << ", received " << r.d.size() << "events."
					     << endl;
					uint error = 0;
					cout << "Checking " << (t.d.size() - 1) << " events... ";
					if (checkEvents(error, t, r, scl))
						cout << "ok :-)" << endl;
					else
						cout << "failed at event no. " << error << endl;

				} break;
				case 'b': {
					vector<uint> neurons(384, 0);
					for (uint i = 0; i < 192; i++) {
						neurons[i] = i;
						neurons[i + 192] = i + 256;
					}
					vector<bool> spikeystatus(bus->hw_const->cr_width(), false);
					// enable clocks to analog_chip
					spikeystatus[bus->hw_const->cr_anaclken()] = true;
					spikeystatus[bus->hw_const->cr_anaclkhien()] = true;
					chip_control->setCtrl(bool2int(spikeystatus), 20);

					uint time, dist, reps;
					double maxrate, minrate;

					cout << "Duration of one test?";
					cin >> dist;
					cout << "Max. rate, min. rate, repetitions? ";
					cin >> maxrate >> minrate >> reps;

					SpikeTrain tt, t, r;
					srand(1);

					time = 0x400;
					//					time=(scl->getSyncMax()+scl->getEvtLatency())<<5;//400MHz and
					//timebins
					tt.d.clear();
					t.d.clear();
					r.d.clear();
					//					mem->Clear();//free playback memory

					// spiketrain, neurons, start, end, recovery, meanrate
					for (uint i = 0; i < reps; i++) {
						tt.d.clear();
						poissonTrain(tt, neurons, time, time + dist, 0,
						             (i % 2 ? maxrate : minrate));
						time += dist;
						t.d.insert(t.d.end(), tt.d.begin(), tt.d.end());
					}

					// cout<<t<<endl;

					t.d.push_back(IData::Event(0, (time + 0x8000) << 4)); // detmines length of
					                                                      // experiment (give time
					                                                      // in tbins)

					// set event fifo resets before any event processing starts
					for (uint i = 0; i < bus->hw_const->event_outs(); i++)
						spikeystatus[bus->hw_const->cr_eout_rst() + i] = true;
					for (uint i = 0; i < bus->hw_const->event_ins(); i++)
						spikeystatus[bus->hw_const->cr_einb_rst() + i] = true;
					for (uint i = 0; i < bus->hw_const->event_ins(); i++)
						spikeystatus[bus->hw_const->cr_ein_rst() + i] = true;
					chip_control->setCtrl(bool2int(spikeystatus), 20);

					// enable internal event loopback and reset event fifos
					chip_control->setEvLb((1 << bus->hw_const->el_enable_pos()), 20); // turn on
					                                                                  // event
					                                                                  // loopback:

					// release event fifo resets
					for (uint i = 0; i < bus->hw_const->event_outs(); i++)
						spikeystatus[bus->hw_const->cr_eout_rst() + i] = false;
					for (uint i = 0; i < bus->hw_const->event_ins(); i++)
						spikeystatus[bus->hw_const->cr_einb_rst() + i] = false;
					for (uint i = 0; i < bus->hw_const->event_ins(); i++)
						spikeystatus[bus->hw_const->cr_ein_rst() + i] = false;
					chip_control->setCtrl(bool2int(spikeystatus), 20);

					tt.d.clear();
					sp->sendSpikeTrain(t, &tt);
					sp->Flush();
					sp->Run();
					sp->waitPbFinished();


					if (tt.d.size())
						cout << "dropped events: " << tt.d.size() << endl
						     << endl;

					// sleep(10);

					sp->recSpikeTrain(r);
					bus->dbg(6) << endl
					            << t << "Received: " << dec << r.d.size() << endl
					            << r << endl;

					cout << "sent " << t.d.size() << ", received " << r.d.size() << "events."
					     << endl;
					uint error = 0;
					cout << "Checking " << (t.d.size() - 1) << " events... ";
					if (checkEvents(error, t, r, scl))
						cout << "ok :-)" << endl;
					else
						cout << "failed at event no. " << error << endl;

				} break;
				case 'a': {
					vector<uint> neurons(384, 0);
					for (uint i = 0; i < 192; i++) {
						neurons[i] = i;
						neurons[i + 192] = i + 256;
					}
					vector<bool> spikeystatus(bus->hw_const->cr_width(), false);
					// enable clocks to analog_chip
					spikeystatus[bus->hw_const->cr_anaclken()] = true;
					spikeystatus[bus->hw_const->cr_anaclkhien()] = true;
					chip_control->setCtrl(bool2int(spikeystatus), 20);

					uint time, durc, durf;
					double lastrate, startrate, stoprate;
					double csteps, fsteps;

					cout << "Duration of coarse and fine test? ";
					cin >> durc >> durf;
					cout << "Start rate and stop rate? ";
					cin >> startrate >> stoprate;
					cout << "Coarse and fine step sizes? ";
					cin >> csteps >> fsteps;

					SpikeTrain t, r;
					double crate = startrate;
					lastrate = crate;
					bool coarse = true;

					uint dist = durc;
					srand(1);
					while (true) {
						time = 0x400;
						//					time=(scl->getSyncMax()+scl->getEvtLatency())<<5;//400MHz and
						//timebins
						t.d.clear();
						r.d.clear();
						chip[0]->Flush();
						mem->Clear(); // free playback memory

						// spiketrain, neurons, start, end, recovery, meanrate
						poissonTrain(t, neurons, time, time + dist, 0, crate); // 20 Hz

						t.d.push_back(IData::Event(0, (time + dist + 0x8000) << 4)); // detmines
						                                                             // length of
						                                                             // experiment
						                                                             // (give time
						                                                             // in tbins)

						// set event fifo resets before any event processing starts
						for (uint i = 0; i < bus->hw_const->event_outs(); i++)
							spikeystatus[bus->hw_const->cr_eout_rst() + i] = true;
						for (uint i = 0; i < bus->hw_const->event_ins(); i++)
							spikeystatus[bus->hw_const->cr_einb_rst() + i] = true;
						for (uint i = 0; i < bus->hw_const->event_ins(); i++)
							spikeystatus[bus->hw_const->cr_ein_rst() + i] = true;
						chip_control->setCtrl(bool2int(spikeystatus), 20);

						// enable internal event loopback and reset event fifos
						chip_control->setEvLb((1 << bus->hw_const->el_enable_pos()),
						                      20); // turn on event loopback:

						// release event fifo resets
						for (uint i = 0; i < bus->hw_const->event_outs(); i++)
							spikeystatus[bus->hw_const->cr_eout_rst() + i] = false;
						for (uint i = 0; i < bus->hw_const->event_ins(); i++)
							spikeystatus[bus->hw_const->cr_einb_rst() + i] = false;
						for (uint i = 0; i < bus->hw_const->event_ins(); i++)
							spikeystatus[bus->hw_const->cr_ein_rst() + i] = false;
						chip_control->setCtrl(bool2int(spikeystatus), 20);


						sp->sendSpikeTrain(t);
						sp->Flush();
						sp->Run();
						sp->waitPbFinished();


						// sleep(10);

						sp->recSpikeTrain(r);
						bus->dbg(6) << endl
						            << t << "Received: " << dec << r.d.size() << endl
						            << r << endl;

						cout << "sent " << t.d.size() << ", received " << r.d.size() << "events."
						     << endl;
						uint error = 0;
						cout << "Checking " << (t.d.size() - 1) << " events at rate " << crate
						     << "... ";
						if (checkEvents(error, t, r, scl)) {
							cout << "ok :-)" << endl;
							if (crate >= stoprate) {
								break;
								cout << "No errors detected." << endl;
							} else {
								lastrate = crate;
								if (coarse)
									crate += csteps;
								else
									crate += fsteps;
								cout << "Increased rate to " << crate << endl;
							}
							continue;
						} else {
							cout << "failed at event no. " << error << endl;
							if (coarse) {
								coarse = false;
								crate = lastrate;
							} else {
								cout << "Resulting maximum rate: " << lastrate << endl;
								break;
							}
						}
					}
				} break;
				case '7': {
					cout << "Set or reset readout chain? ([set]/reset)";
					string answer;
					cin >> answer;
					if (answer == "reset") {
						analog_readout->clear(0);
						analog_readout->clear(1);
					} else {
						uint chain = 0, addr = 0;
						cout << "please input chain (0/1) and vout address: " << endl;
						cin >> chain >> addr;
						analog_readout->set(
						    (chain == 0 ? analog_readout->voutl : analog_readout->voutr), addr,
						    true); // set with verify

						sp->Flush();
						sp->Run();

						if (analog_readout->check_last(addr, false) == 1)
							bus->dbg(::Logger::INFO) << "Areadout check: positive";
						else
							bus->dbg(::Logger::ERROR) << "Areadout check: negative";

						vector<float> v;
						v.clear();
						//				for(int
						//i=0;i<10;++i)v.push_back(chip[0]->readAdc(SBData::voutl));
						for (int i = 0; i < 10; ++i)
							v.push_back(chip[0]->readAdc(SBData::ibtest));
						cout << "ADC read: " << mean(v);
					}

				} break;
				case '5': {
					SpikeTrain t, r;
					uint n[] = {5, 6, 7, 8, 9};
					vector<uint> neurons(n, n + 5);
					poissonTrain(t, neurons, 17, 255, 3, 0.01); // 20 Hz
					sp->sendSpikeTrain(t);
					sp->Flush();
					sp->Run();
					sp->waitPbFinished();

					cout << t;
					sp->recSpikeTrain(r);
					cout << endl
					     << t << "Received: " << dec << r.d.size() << endl
					     << r << endl;
				} break;
				case 'x':
					cont = false;
					break;
			}
		} while (cont);
		return true;
	};

	void mess(float& f1, float& f2, uint nmess = 10)
	{
		vector<float> v;
		for (uint i = 0; i < nmess; ++i)
			v.push_back(sp->readAdc(SBData::vout0));
		f1 = mean(v);
		cout << " Vout0 mean(" << nmess << "): " << f1 << " sdev: " << sdev(v);
		v.clear();
		for (uint i = 0; i < nmess; ++i)
			v.push_back(sp->readAdc(SBData::vout4));
		f2 = mean(v);
		cout << " Vout4 mean(" << nmess << "): " << f2 << " sdev: " << sdev(v) << endl;
	};

	// create a poisson distributed spike train (is appended to st), containing neurons out of
	// 'neurons' vector
	// meanfrate is mean firing rate in time units
	// time is measured in 400MHz clk cycles

	void poissonTrain(SpikeTrain& st, const vector<uint>& neurons, uint starttime, uint endtime,
	                  uint rec, float meanfrate)
	{
		vector<int> lastftime(neurons.size(), -1); // remember last firing time
		for (uint t = starttime << 4; t < (endtime << 4); ++t) { // endtime is not included
			// loop over neurons
			for (uint j = 0; j < neurons.size(); ++j) {
				// check for recovery period
				if (lastftime[j] < 0 || (t - lastftime[j]) > (rec << 4)) {
					// rnd test
					if (((float)rand() / (float)RAND_MAX) <= (meanfrate / 16)) { // create event
						st.d.push_back(IData::Event(neurons[j], t));
						lastftime[j] = t;
					}
				}
			}
		}
	}
};

class LteeSpikeyClassAG : public Listee<Testmode>
{
public:
	LteeSpikeyClassAG()
	    : Listee<Testmode>(string("spikeyclassag"),
	                       string("Test spikey class for spikey chip access")){};
	Testmode* create() { return new TMSpikeyClassAG(); };
} ListeeSpikeyClassAG;
