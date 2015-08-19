#include <vector>
#include <ostream>

namespace spikey2
{

// ***** encapsulation of the information exchange between spikey and the host software ******
//
// two classes of date exist: lvds data (anything that is transported by the spikey lvds bus)
//														sideband data (data transported parallel to the lvds bus between host
//and spikey)
//

// the internal representation of the lvds data (plus additional bus status and control signals) ->
// for efficiency this could be changed to an enumeration
class IData
{
private:
	uint64_t d = 0; // data field, passed to submodule for control interface data
	uint c = 0; // command field
	uint n = 0; // neuron number for event
	uint t = 0; // target time for event
	bool _reset, _pllreset, _cimode, _vrest_gnd, _vm_gnd, _ibtest_meas, _power_enable,
	    _rx_clk0_phase, _rx_clk1_phase, _rx_dat0_phase, _rx_dat1_phase,
	    _direct_pckt; // bus mode status pins

public:
	enum type { t_empty, t_ci, t_event, t_delay, t_bustest, t_control, t_phsel } _payload;
	// named constructor idiom
	static IData Event(uint nd = 0, uint32_t time = 0) { return IData(nd, time); };
	static IData CI(uint nc = 0, uint64_t nd = 0) { return IData(nc, nd); };

	// simple methods for accessing the internal fields
	IData(uint nc, uint64_t nd)
	    : d(nd), c(nc), _payload(t_ci){}; // construct control_interface packet
	IData(uint nd, uint32_t time) : n(nd), t(time), _payload(t_event){}; // construct event packet
	IData(bool nreset, bool npllreset, bool ncimode) // construct ctrl packet
	    : _reset(nreset),
	      _pllreset(npllreset),
	      _cimode(ncimode),
	      _vrest_gnd(false),
	      _vm_gnd(false),
	      _ibtest_meas(false),
	      _power_enable(true),
	      _direct_pckt(false),
	      _payload(t_control){};
	IData(bool rxc0phsel, bool rxc1phsel, bool rxd0phsel, bool rxd1phsel) // construct phsel packet
	    : _rx_clk0_phase(rxc0phsel),
	      _rx_clk1_phase(rxc1phsel),
	      _rx_dat0_phase(rxd0phsel),
	      _rx_dat1_phase(rxd1phsel),
	      _payload(t_phsel){};
	IData() : _payload(t_empty){};

	type payload(void) const { return _payload; };
	void setPayload(type p) { _payload = p; };

	// added by DB for fast spike train re-allocation
	void setEvent(uint nd, uint32_t time)
	{
		_payload = t_event;
		n = nd;
		t = time;
	};

	void clear() { _payload = t_empty; };
	bool isEmpty() const { return payload() == t_empty; };

	// data and command for ci command packet
	uint64_t data(void) const { return d; };
	uint64_t& setData(void) { return d; };
	uint cmd(void) const { return c; };
	uint& setCmd(void) { return c; };
	void setCmd(uint com) { c = com; };
	bool isCI() const { return payload() == t_ci; };
	void setCI() { setPayload(t_ci); };

	// neuron adr, time, settime for event packet
	uint neuronAdr(void) const { return n; };
	uint& setNeuronAdr(void) { return n; };
	uint time(void) const { return t; };
	uint& setTime(void) { return t; };
	uint clk(void) const { return t >> 4; };
	uint bin(void) const { return t & 0xf; };
	bool event(void) { return isEvent(); }; // obsolete
	bool isEvent(void) const { return payload() == t_event; };
	void setEvent() { setPayload(t_event); };

	// status bits for buscontrol packet
	bool isControl(void) const { return payload() == t_control; };

	bool reset(void) const { return _reset; };
	bool pllreset(void) const { return _pllreset; };
	bool cimode(void) const { return _cimode; };
	bool direct_pckt(void) const { return _direct_pckt; };
	bool vrest_gnd(void) const { return _vrest_gnd; };
	bool vm_gnd(void) const { return _vm_gnd; };
	bool ibtest_meas(void) const { return _ibtest_meas; };
	bool power_enable(void) const { return _power_enable; };

	bool rx_clk0_phase(void) const { return _rx_clk0_phase; };
	bool rx_clk1_phase(void) const { return _rx_clk1_phase; };
	bool rx_dat0_phase(void) const { return _rx_dat0_phase; };
	bool rx_dat1_phase(void) const { return _rx_dat1_phase; };

	void setReset(bool d) { _reset = d; };
	void setPllreset(bool d) { _pllreset = d; };
	void setCimode(bool d) { _cimode = d; };
	void setDirectpacket(bool d) { _direct_pckt = d; };
	void setVrestgnd(bool d) { _vrest_gnd = d; };
	void setVmgnd(bool d) { _vm_gnd = d; };
	void setIbtestmeas(bool d) { _ibtest_meas = d; };
	void setPowerenable(bool d) { _power_enable = d; };

	void setRxclk0phase(bool d) { _rx_clk0_phase = d; };
	void setRxclk1phase(bool d) { _rx_clk1_phase = d; };
	void setRxdat0phase(bool d) { _rx_dat0_phase = d; };
	void setRxdat1phase(bool d) { _rx_dat1_phase = d; };

	bool isPhsel(void) const { return payload() == t_phsel; };

	bool isDelay(void) const { return payload() == t_delay; }; // setdelay packet
	// for a setDelay command, the reset/ci_mode/pll_reset pins have to be set to 0/1/0
	void setDelay(uint delay)
	{
		d = delay;
		setPayload(t_delay);
	};
	uint64_t delay(void) const { return d; };

	bool isBusTestPattern(void) const { return payload() == t_bustest; }; // bustest pattern packet
	void setBusTestPattern(uint64_t pattern)
	{
		d = pattern;
		setPayload(t_bustest);
	};
	uint64_t busTestPattern(void) const { return d; };


	bool operator==(IData const& b) const
	{
		if (b.d == d && b.c == c && b.n == n && b.t == t && b._reset == _reset &&
		    b._pllreset == _pllreset && b._cimode == _cimode && b._vrest_gnd == _vrest_gnd &&
		    b._vm_gnd == _vm_gnd && b._ibtest_meas == _ibtest_meas &&
		    b._power_enable == _power_enable && b._rx_clk0_phase == _rx_clk0_phase &&
		    b._rx_clk1_phase == _rx_clk1_phase)
			return true;
		return false;
	}
};

// sideband data -> implements fire in and external control voltages
class SBData
{
public:
	enum ADtype {
		irefdac,
		vcasdac,
		vm,
		vstart,
		vrest,
		vout0,
		vout4,
		ibtest,
		tempsense
	}; // vout0/4 adc readback of out
	enum type { t_empty, t_fire, t_dac, t_adc } _payload;

private:
	ADtype _adtype;
	float _advalue;

	static std::vector<bool>& emptyfire(); // static const vector emptyfire replaced by this
	                                       // function, by DB
	std::vector<bool> _firein;
	std::vector<bool> _firein_late;
	int _delay;

public:
	SBData() : _payload(t_empty){}; // default constructor clears all
	type payload(void) const { return _payload; };
	void setPayload(type p) { _payload = p; };

	const std::vector<bool>& firein(void) const
	{
		return _firein;
	}; // pattern for early fireinxx lines
	const std::vector<bool>& fireinLate(void) const
	{
		return _firein_late;
	}; // pattern for late fireinxx lines
	int delay() const { return _delay; }; // returns time between early and late fire pulses

	// fi: firein early, fil: firein late, del: time in clk/2 between firein early and late, min is
	// 4
	void setFirein(std::vector<bool>& fi, const std::vector<bool>& fil = emptyfire(), int del = 0)
	{
		setPayload(t_fire);
		_firein = fi;
		_firein_late = fil;
		_delay = del;
	};

	bool isFire(void) const { return payload() == t_fire; };

	// dacs
	bool isDac(void) const { return payload() == t_dac; };
	bool isAdc(void) const { return payload() == t_adc; };

	void setDac(ADtype type, float value)
	{
		setPayload(t_dac);
		_advalue = value;
		_adtype = type;
	};
	void setAdc(ADtype type)
	{
		setPayload(t_adc);
		_adtype = type;
	};
	void setTemp(ADtype type) { _adtype = type; }
	void setADvalue(float v) { _advalue = v; };
	float ADvalue() const { return _advalue; };
	ADtype getADtype() const { return _adtype; };
};

// stream idata structs
std::ostream& operator<<(std::ostream& o, const SBData& d);
// stream sbdata structs
std::ostream& operator<<(std::ostream& o, const IData& d);

std::istream& operator>>(std::istream& i, IData& d);

} // end of namespace spikey2
