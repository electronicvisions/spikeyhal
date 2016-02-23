#include <gtest/gtest.h>

#include "common.h"

#include "idata.h"
#include "sncomm.h"
#include "sc_sctrl.h"
#include "sc_pbmem.h"

#include "spikenet.h"

#include <numeric>
#include <vector>

static log4cxx::LoggerPtr logger = log4cxx::Logger::getLogger("Tool.CalibLinks");

using namespace spikey2;
uint randomseed = 42;

int main(int argc, char* argv[])
{
	/*
	 * Link test with random data.
	 * Initially programmed for testing stability of high speed data links.
	 */

	// ##### begin init
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

	// spikey delay set
	d.setCimode(true);
	chip->Send(SpikenetComm::ctrl, d);

	d.setCimode(false);
	chip->Send(SpikenetComm::ctrl, d);

	// set CAL and RST input of IOdelays once to initialize
	mem->getSCTRL()->spydc->write(0, 0x01000000);
	mem->getSCTRL()->spydc->write(0, 0x10000000);

	// set phase select bits
	chip->Send(SpikenetComm::ctrl, p);

	uint ipat0 = 0, ipat1 = 0, ipat2 = 0, ipat3 = 0;
	uint opat0 = 0, opat1 = 0, opat2 = 0, opat3 = 0;
	// ##### end init

	// test link forever
	uint delay_fpga = 0;
	uint number_bits_per_link = 9;
	int temperature = int(std::round(mem->getSCTRL()->getTemp()));
	bool calib_success = false;

	while (not calib_success) {
		// ##### begin configure delay lines
		// set pattern for IOdelay tuning
		mem->getSCTRL()->setDout0(0x15555);
		mem->getSCTRL()->setDout1(0x15555);
		mem->getSCTRL()->setDout2(0x15555);
		mem->getSCTRL()->setDout3(0x15555);

		mem->getSCTRL()->spydc->write(0, 0x10000000);

		for (uint d = 0; d <= delay_fpga; d++)
			mem->getSCTRL()->spydc->write(0, 0x0013ffff);

		d.setCimode(false);
		chip->Send(SpikenetComm::ctrl, d);
		// ##### end configure delay lines

		//XOR to trigger errors
		uint link0_xor = 0, link1_xor = 0;
		//AND to trigger stuck bits == 1
		uint link0_and = (1 << number_bits_per_link) - 1, link1_and = (1 << number_bits_per_link) - 1;
		//OR to trigger stuck bits == 0
		uint link0_or = 0, link1_or = 0;

		for (uint run = 0; run < 100; run++) {
			ipat0 = (rand() & 0xff) << number_bits_per_link | 0x100 | (rand() & 0xff);
			ipat1 = (rand() & 0xff) << number_bits_per_link | 0x000 | (rand() & 0xff);
			ipat2 = (rand() & 0xff) << number_bits_per_link | 0x100 | (rand() & 0xff);
			ipat3 = (rand() & 0xff) << number_bits_per_link | 0x000 | (rand() & 0xff);
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

			// test stuck bits
			//opat0 |= 1 << 3;
			//opat0 |= 1 << (3 + number_bits_per_link);
			//opat1 |= 1 << 3;
			//opat1 |= 1 << (3 + number_bits_per_link);
			//opat2 &= ~(1 << 4);
			//opat2 &= ~(1 << (4 + number_bits_per_link));
			//opat3 &= ~(1 << 4);
			//opat3 &= ~(1 << (4 + number_bits_per_link));

			if (ipat0 != opat0 || ipat1 != opat1 || ipat2 != opat2 || ipat3 != opat3) {
				link0_xor |= (((ipat0 ^ opat0) & 0x1ff) | (((ipat0 ^ opat0) >> number_bits_per_link) & 0x1ff)) | //TODO: ask Andi! only 18bit?
							 (((ipat1 ^ opat1) & 0x1ff) | (((ipat1 ^ opat1) >> number_bits_per_link) & 0x1ff));
				link1_xor |= (((ipat2 ^ opat2) & 0x1ff) | (((ipat2 ^ opat2) >> number_bits_per_link) & 0x1ff)) |
							 (((ipat3 ^ opat3) & 0x1ff) | (((ipat3 ^ opat3) >> number_bits_per_link) & 0x1ff));

				//binout(std::cout, link0_xor, number_bits_per_link);
				//binout(std::cout, link1_xor, number_bits_per_link);
			}

			link0_and &= (opat0 & 0x1ff) & ((opat0 >> number_bits_per_link) & 0x1ff) & (opat1 & 0x1ff) & ((opat1 >> number_bits_per_link) & 0x1ff);
			link1_and &= (opat2 & 0x1ff) & ((opat2 >> number_bits_per_link) & 0x1ff) & (opat3 & 0x1ff) & ((opat3 >> number_bits_per_link) & 0x1ff);
			link0_or |= (opat0 & 0x1ff) | ((opat0 >> number_bits_per_link) & 0x1ff) | (opat1 & 0x1ff) | ((opat1 >> number_bits_per_link) & 0x1ff);
			link1_or |= (opat2 & 0x1ff) | ((opat2 >> number_bits_per_link) & 0x1ff) | (opat3 & 0x1ff) | ((opat3 >> number_bits_per_link) & 0x1ff);
		}

		std::cout << "delay: " << delay_fpga << " ## link0 XOR: ";
		binout(std::cout, link0_xor, number_bits_per_link);
		std::cout << " | link1: ";
		binout(std::cout, link1_xor, number_bits_per_link);

		std::cout << " ## link0 AND: ";
		binout(std::cout, link0_and, number_bits_per_link);
		std::cout << " | link1: ";
		binout(std::cout, link1_and, number_bits_per_link);

		std::cout << " ## link0 OR: ";
		binout(std::cout, link0_or, number_bits_per_link);
		std::cout << " | link1: ";
		binout(std::cout, link1_or, number_bits_per_link);
		std::cout << std::endl;

		//if no error, no bit can be stuck
		if(link0_xor == 0) {
			assert(!(link0_and != 0 || link0_or != ((1 << number_bits_per_link) - 1)));
		}
		if(link1_xor == 0) {
			assert(!(link1_and != 0 || link1_or != ((1 << number_bits_per_link) - 1)));
		}

		//TODO: continue here
		if(delay_fpga > 60){
			calib_success = true;
		}

		delay_fpga += 1;
	}
}
