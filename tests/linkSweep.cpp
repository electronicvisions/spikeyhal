#include <gtest/gtest.h>

#include "common.h"

#include "idata.h"
#include "sncomm.h"
#include "sc_sctrl.h"
#include "sc_pbmem.h"

#include "spikenet.h"

#include <numeric>

static log4cxx::LoggerPtr logger = log4cxx::Logger::getLogger("Test.Linksweep");

namespace spikey2
{
TEST(HWTest, linkSweep)
{
	/*
	 * Sweep FPGA input delays and test with random data.
	 * TODO: add asserts
	 */
	boost::shared_ptr<SpikenetComm> bus(new SC_Mem()); // playback memory interface
	boost::shared_ptr<Spikenet> chip(new Spikenet(bus));
	boost::shared_ptr<SC_Mem> mem = boost::dynamic_pointer_cast<SC_Mem>(bus);

	IData d(true, true, true);           // mode pins
	IData p(false, false, false, false); // phsel bits

	// turn off idle and reset IOs
	mem->getSCTRL()->setContIdleOff();
	mem->getSCTRL()->spydc->write(0, 0x10000000);

	d.setPowerenable(true);
	d.setPllreset(false);
	chip->Send(SpikenetComm::ctrl, d);

	d.setCimode(false);
	chip->Send(SpikenetComm::ctrl, d);

	d.setReset(false);
	chip->Send(SpikenetComm::ctrl, d);

	d.setPllreset(true);
	chip->Send(SpikenetComm::ctrl, d);
	d.setPllreset(false);
	chip->Send(SpikenetComm::ctrl, d);

	// ##### start spikey delay set #####
	d.setCimode(true);
	chip->Send(SpikenetComm::ctrl, d);

	d.setCimode(false);
	chip->Send(SpikenetComm::ctrl, d);
	// ##### end spikey delay set #####

	// set CAL and RST input of IOdelays once to initialize
	mem->getSCTRL()->spydc->write(0, 0x01000000);
	mem->getSCTRL()->spydc->write(0, 0x10000000);

	// set phase select bits
	chip->Send(SpikenetComm::ctrl, p);

	uint ipat0 = 0, ipat1 = 0, ipat2 = 0, ipat3 = 0;
	uint opat0 = 0, opat1 = 0, opat2 = 0, opat3 = 0;

	// sweep FPGA input delay lines
	for (uint i = 0; i < 256; i++) {
		uint run;
		uint err = 0;
		uint link0_xor = 0, link1_xor = 0;
		for (run = 0; run < 20; run++) {
			err = 0;
			ipat0 = (rand() & 0xff) << 9 | 0x100 | (rand() & 0xff);
			ipat1 = (rand() & 0xff) << 9 | 0x000 | (rand() & 0xff);
			ipat2 = (rand() & 0xff) << 9 | 0x100 | (rand() & 0xff);
			ipat3 = (rand() & 0xff) << 9 | 0x000 | (rand() & 0xff);
			mem->getSCTRL()->setDout0(ipat0);
			mem->getSCTRL()->setDout1(ipat1);
			mem->getSCTRL()->setDout2(ipat2);
			mem->getSCTRL()->setDout3(ipat3);

			d.setCimode(true);
			chip->Send(SpikenetComm::ctrl, d);

			mem->getSCTRL()->getDin0(opat0);
			mem->getSCTRL()->getDin1(opat1);
			mem->getSCTRL()->getDin2(opat2);
			mem->getSCTRL()->getDin3(opat3);

			if (ipat0 != opat0 || ipat1 != opat1 || ipat2 != opat2 || ipat3 != opat3) {
				// std::cout << dec << run << ", ipat0: 0x" << hex << ipat0 << ", opat0: 0x" << opat0
				// << ", xor: 0x" << (ipat0^opat0) << std::endl;
				// std::cout << dec << run << ", ipat1: 0x" << hex << ipat1 << ", opat1: 0x" << opat1
				// << ", xor: 0x" << (ipat1^opat1) << std::endl;
				// std::cout << dec << run << ", ipat2: 0x" << hex << ipat2 << ", opat2: 0x" << opat2
				// << ", xor: 0x" << (ipat2^opat2) << std::endl;
				// std::cout << dec << run << ", ipat3: 0x" << hex << ipat3 << ", opat3: 0x" << opat3
				// << ", xor: 0x" << (ipat3^opat3) << std::endl << std::endl;;
				link0_xor |= (((ipat0 ^ opat0) & 0x1ff) | (((ipat0 ^ opat0) >> 9) & 0x1ff)) |
							 (((ipat1 ^ opat1) & 0x1ff) | (((ipat1 ^ opat1) >> 9) & 0x1ff));
				link1_xor |= (((ipat2 ^ opat2) & 0x1ff) | (((ipat2 ^ opat2) >> 9) & 0x1ff)) |
							 (((ipat3 ^ opat3) & 0x1ff) | (((ipat3 ^ opat3) >> 9) & 0x1ff));
				err++;
				if (err > 10)
					break;
			}
		}
		if (err > 0) {
			std::cout << "link0: ";
			binout(std::cout, link0_xor, 9);
			std::cout << " | link1: ";
			binout(std::cout, link1_xor, 9);
			std::cout << std::endl;
		}
		std::cout << "FPGA in: del " << i << ", runs: " << run << ", errors: " << (uint)err << std::endl;

		// set pattern for IOdelay tuning
		mem->getSCTRL()->setDout0(0x15555);
		mem->getSCTRL()->setDout1(0x15555);
		mem->getSCTRL()->setDout2(0x15555);
		mem->getSCTRL()->setDout3(0x15555);

		mem->getSCTRL()->spydc->write(0, 0x10000000);

		for (uint d = 0; d <= i; d++)
			mem->getSCTRL()->spydc->write(0, 0x0013ffff);

		d.setCimode(false);
		chip->Send(SpikenetComm::ctrl, d);
	}

	// sweep Spikey input delay lines
	for (uint i = 0; i < 8; i++) {
		uint run;
		uint err = 0;
		uint link0_xor = 0, link1_xor = 0;
		for (run = 0; run < 100; run++) {
			err = 0;
			ipat0 = (rand() & 0xff) << 9 | 0x100 | (rand() & 0xff);
			ipat1 = (rand() & 0xff) << 9 | 0x000 | (rand() & 0xff);
			ipat2 = (rand() & 0xff) << 9 | 0x100 | (rand() & 0xff);
			ipat3 = (rand() & 0xff) << 9 | 0x000 | (rand() & 0xff);
			mem->getSCTRL()->setDout0(ipat0);
			mem->getSCTRL()->setDout1(ipat1);
			mem->getSCTRL()->setDout2(ipat2);
			mem->getSCTRL()->setDout3(ipat3);

			d.setCimode(true);
			chip->Send(SpikenetComm::ctrl, d);

			mem->getSCTRL()->getDin0(opat0);
			mem->getSCTRL()->getDin1(opat1);
			mem->getSCTRL()->getDin2(opat2);
			mem->getSCTRL()->getDin3(opat3);

			if (ipat0 != opat0 || ipat1 != opat1 || ipat2 != opat2 || ipat3 != opat3) {
				// std::cout << dec << run << ", ipat0: 0x" << hex << ipat0 << ", opat0: 0x" << opat0
				// << ", xor: 0x" << (ipat0^opat0) << std::endl;
				// std::cout << dec << run << ", ipat1: 0x" << hex << ipat1 << ", opat1: 0x" << opat1
				// << ", xor: 0x" << (ipat1^opat1) << std::endl;
				// std::cout << dec << run << ", ipat2: 0x" << hex << ipat2 << ", opat2: 0x" << opat2
				// << ", xor: 0x" << (ipat2^opat2) << std::endl;
				// std::cout << dec << run << ", ipat3: 0x" << hex << ipat3 << ", opat3: 0x" << opat3
				// << ", xor: 0x" << (ipat3^opat3) << std::endl << std::endl;;
				link0_xor |= (((ipat0 ^ opat0) & 0x1ff) | (((ipat0 ^ opat0) >> 9) & 0x1ff)) |
							 (((ipat1 ^ opat1) & 0x1ff) | (((ipat1 ^ opat1) >> 9) & 0x1ff));
				link1_xor |= (((ipat2 ^ opat2) & 0x1ff) | (((ipat2 ^ opat2) >> 9) & 0x1ff)) |
							 (((ipat3 ^ opat3) & 0x1ff) | (((ipat3 ^ opat3) >> 9) & 0x1ff));
				err++;
				if (err > 10)
					break;
			}
		}
		if (err > 0) {
			std::cout << "link0: ";
			binout(std::cout, link0_xor, 9);
			std::cout << " | link1: ";
			binout(std::cout, link1_xor, 9);
			std::cout << std::endl;
		}
		std::cout << "Spikey in: del " << i << ", runs: " << run << ", errors: " << (uint)err
			 << std::endl;

		// Spikey delays:
		for (int j = 0; j < 17; j++) {
			chip->setDelay(j, i);
		}

		d.setCimode(false);
		chip->Send(SpikenetComm::ctrl, d);
	}

	d.setReset(true);
	chip->Send(SpikenetComm::ctrl, d);
	d.setReset(false);
	chip->Send(SpikenetComm::ctrl, d);

	mem->getSCTRL()->setDout0(0x00340);
	mem->getSCTRL()->setDout1(0x00000);
	mem->getSCTRL()->setDout2(0x00100);
	mem->getSCTRL()->setDout3(0x1feff);

	d.setDirectpacket(true);
	chip->Send(SpikenetComm::ctrl, d);

	for (uint i = 0; i < 10; i++) {
		opat0 = 0, opat1 = 0, opat2 = 0, opat3 = 0;
		mem->getSCTRL()->getDin0(opat0);
		mem->getSCTRL()->getDin1(opat1);
		mem->getSCTRL()->getDin2(opat2);
		mem->getSCTRL()->getDin3(opat3);
		std::cout << dec << i << ": 0x" << hex << opat0 << ", 0x" << opat1 << ", 0x" << opat2
			 << ", 0x" << opat3 << std::endl;
	}

	d.setDirectpacket(false);
	chip->Send(SpikenetComm::ctrl, d);

	//mem->intClear();
}
}
