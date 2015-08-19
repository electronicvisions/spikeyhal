namespace spikey2
{

// necessary forward declarations
class SynapseControl; // synapse_control.h
class PramControl; // pram_control.h
class ChipControl; // pram_control.h
class Loopback; // pram_control.h
class AnalogReadout; // pram_control.h
using ::Logger;

// the Spikenet class implements the lvds-interface level
// the different subunits of the chip use this class for communication
class Spikenet
{
protected:
	::Logger& dbg;

private:
	uint chipid;
	uint rcvnum; // number of outstanding receive packets
	bool checkEvent; // enables event checking
	// subunits
	boost::shared_ptr<SynapseControl> synapse_control;
	boost::shared_ptr<PramControl> pram_control;
	boost::shared_ptr<ChipControl> chip_control;
	boost::shared_ptr<Loopback> loopback;
	boost::shared_ptr<AnalogReadout> analog_readout;
	// communication class
public: // !!! made available temporarily to test bus model from within testmodes !!!
	boost::shared_ptr<SpikenetComm> bus;
	boost::shared_ptr<HardwareConstants> hw_const;

private:
	// last bus control
	IData lastctrl;

public:
	// basic timing values
	enum Timing {
		ctrldel = 4,
		patdel = 2,
		sbdel = 1,
		dacdel = 100,
		adcdel = 100
	}; // sb=side band access
	// control update enumeration
	enum CtrlData {
		off = 0,
		reset = 1,
		pllreset = 2,
		cimode = 4,
		bsmode = 8,
		pllbypass = 16,
		phase = 32
	};

	Spikenet(boost::shared_ptr<SpikenetComm> comm, uint chipid = 0);
	uint chip() { return chipid; };
	boost::shared_ptr<SynapseControl> getSC(void) { return synapse_control; };
	boost::shared_ptr<PramControl> getPC(void) { return pram_control; };
	boost::shared_ptr<ChipControl> getCC(void) { return chip_control; };
	boost::shared_ptr<Loopback> getLB(void) { return loopback; };
	boost::shared_ptr<AnalogReadout> getAR(void) { return analog_readout; };

	// communication with bus
	SpikenetComm::Commstate Send(SpikenetComm::Mode mode, IData data = SpikenetComm::emptydata,
	                             uint del = 0, uint syncoffset = 0)
	{
		return bus->Send(mode, data, del, chip(), syncoffset);
	};
	SpikenetComm::Commstate Receive(SpikenetComm::Mode mode, IData& data)
	{
		return bus->Receive(mode, data, chip());
	};
	SpikenetComm::Commstate Flush() { return bus->Flush(chip()); };
	SpikenetComm::Commstate Run() { return bus->Run(); };
	SpikenetComm::Commstate Clear() { return bus->Clear(); };
	// sideband data
	SpikenetComm::Commstate SendSBD(SpikenetComm::Mode mode, const SBData& data, uint del)
	{
		return bus->SendSBD(mode, data, del, chip());
	};
	// sideband data
	SpikenetComm::Commstate RecSBD(SpikenetComm::Mode mode, SBData& data)
	{
		return bus->RecSBD(mode, data, chip());
	};

	void setCheckEvent(bool ev) { checkEvent = ev; };
	// event IO
	void sendEvent(uint nadr, uint time, uint subtime, uint del = 0)
	{
		IData d(nadr, (((bus->stime() << 5) + (time << 4)) & 0xff0) | (subtime & 0xf));
		Send(SpikenetComm::write, d, del);
	}
	// uses absolute time like in spikey.cpp, can use time intervals greater 256
	void sendEventAbs(uint nadr, uint time, uint del = 0)
	{
		IData d(nadr, time);
		Send(SpikenetComm::write, d, del);
	}
	void sendIdleEvent(uint del = 0)
	{
		IData d(0, (uint)0);
		bus->Send(SpikenetComm::write, d, del, SpikenetComm::maxid);
	}
	bool checkEvents(uint& firsterr); // true ok, false and firsterr: error

	// functions to control the bus/environment of the spikey chip
	void sendCtrl(bool reset, bool pllreset, bool cimode, uint del = ctrldel)
	{
		IData d(reset, pllreset, cimode);
		Send(SpikenetComm::ctrl, d, del);
	}
	// set lvds delay, deladdr from ?_base
	void setDelay(uint deladdr, uint delay, uint del = 4)
	{
		IData d;
		d.setReset(false);
		d.setPllreset(false);
		d.setCimode(true);
		d.setDelay((deladdr << hw_const->ctl_deladdr_pos()) |
		           (delay << hw_const->ctl_delval_pos()));
		Send(SpikenetComm::ctrl, d, del);
	}
	// TODO: no xilinx specific things here, but Spikey!!!
	// set control word using enumeration, mask defines the bits to change
	// FIXME: fix this for flyspi-USB: void setCtrl(...)
	// TODO: What about setChip() and setPhase()?

	// pulse the chips firein lines (SideBand data traffic):
	void setFire(vector<bool>& firein, uint del = sbdel)
	{
		SBData f;
		f.setFirein(firein);
		SendSBD(SpikenetComm::write, f, del);
	}

	// pulse the chips firein lines (SideBand data traffic):
	void setFire(vector<bool>& firein, vector<bool>& firein_late, int delay, uint del = sbdel)
	{
		SBData f;
		f.setFirein(firein, firein_late, delay);
		SendSBD(SpikenetComm::write, f, del);
	}

	// set pattern for link test. Pattern is 36bit wide!
	void testPattern(uint64_t pattern, uint del = patdel)
	{
		IData p;
		p.setBusTestPattern(pattern);
		Send(SpikenetComm::ctrl, p, del);
	}
	// true ok, false bits in errmask, if false
	bool checkPattern(uint64_t pat, uint64_t& errmask);

	// set the dacs on nathan and recha (SideBand data traffic):
	void setDac(SBData::ADtype type, float value, uint del = dacdel)
	{
		SBData f;
		f.setDac(type, value);
		SendSBD(SpikenetComm::write, f, del);
	}

	void setIrefdac(float value = 0, uint del = 0) { setDac(SBData::irefdac, value, del); }
	void setVcasdac(float value = 0, uint del = 0) { setDac(SBData::vcasdac, value, del); }
	void setVm(float value = 0, uint del = 0) { setDac(SBData::vm, value, del); }
	void setVstart(float value = 0, uint del = 0) { setDac(SBData::vstart, value, del); }
	void setVrest(float value = 0, uint del = 0) { setDac(SBData::vrest, value, del); }

	float readAdc(SBData::ADtype type, uint del = adcdel);
};

} // end of namespace spikey2
