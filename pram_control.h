#include "common.h"
#include <cassert>
#include <stdint.h>


namespace spikey2
{

// parameter ram control

struct PramData {
	uint cadr, value, lutadr;
	PramData() {}
	PramData(uint c, uint v, uint l) : cadr(c), value(v), lutadr(l) {}
	bool operator==(PramData const& o) const
	{
		if ((cadr != o.cadr) || (value != o.value) || (lutadr != o.lutadr))
			return false;
		return true;
	}
	bool operator!=(PramData const& o) const { return !(*this == o); }
};

struct ManipulatePram {
	uint cadr, value, lutadr, pos;
	bool moveToEnd;
	ManipulatePram() {}
	ManipulatePram(uint c, uint v, uint l) : cadr(c), value(v), lutadr(l), moveToEnd(false) {}
	ManipulatePram(uint c, uint v, uint l, bool m, uint p)
	    : cadr(c), value(v), lutadr(l), pos(p), moveToEnd(m)
	{
	}
};

struct LutData {
	uint luttime, lutboost, lutrepeat, lutstep;
	LutData(uint a, uint b, uint c, uint d) : luttime(a), lutboost(b), lutrepeat(c), lutstep(d) {}
};


class PramControl : public ControlInterface
{
private:
	uint lastPramCycleCnt;
	// Block
	enum DACblock {
		syn0Blk,
		syn1Blk,
		neuron0Blk,
		neuron1Blk,
		voutlBlk,
		voutrBlk,
		biasBlk,
		outampBlk
	};

	DACblock whereIs(uint cadr);
	bool isVoltage(uint cadr);

public:
	PramControl(Spikenet* s) : ControlInterface(s), lastPramCycleCnt(0) {}
	virtual uint get_cicmd() { return sp->hw_const->ci_paramrami(); } // ci subtype
	enum { pramdefault = 5 }; // $ default delay to avoid errors in parameter_ram.v
	//   synapses         neurons
	// 0-> lower 128    0-> even neurons of left block
	// 1-> upper 128    1-> odd left
	enum outamp {
		left0 = 0,
		left1 = 1,
		left2 = 2,
		left3 = 3,
		right0 = 4,
		right1 = 5,
		right2 = 6,
		right3 = 7,
		itest = 8,
		num_outamp = 9
	};

	// vout block of spikey, voutl connects to right!!! half and vice versa
	enum vout {
		Ei0 = 0,
		Ei1 = 1, // inhibitory reversal potential
		El0 = 2,
		El1 = 3, // leakage reversal potential
		Er0 = 4,
		Er1 = 5, // reset potential
		Ex0 = 6,
		Ex1 = 7,      // excitatory reversal potential
		Vclra = 8,    // storage clear bias synapse array acausal
		              // (higher bias->smaller amount stored on cap)
		Vclrc = 9,    // dito, causal
		Vcthigh = 10, // correlation threshold high
		Vctlow = 11,  // correlation threshold low
		Vfac0 = 12,
		Vfac1 = 13, // short term facilitation reference voltage
		Vstdf0 = 14,
		Vstdf1 = 15, // short term capacitor high potential
		Vt0 = 16,
		Vt1 = 17, // neuron threshold voltage
		Vcasneuron = 18, // neuron input cascode gate voltage
		Vresetdll = 19, // dll reset voltage
		aro_dllvctrl = 20, // dll control voltage readout (only spikey v2)
		aro_pre1b = 21, // spike input buf 1 presyn (only spikey v2)
		aro_selout1hb = 22, // spike input buf 1 selout (only spikey v2)
		probepad = 31, // left vout voltage buffer block of spikey  (only spikey v1)
		num_vout_adj = Vresetdll + 1 // number of adjustable vout buffers
	};

	// bias block, 0 -> lower 128 synapses of left block,
	//             1 -> upper left,
	//             2 -> lower right
	//             3 -> upper right
	enum biasb {
		Vdtc0 = 0,
		Vdtc1 = 1,
		Vdtc2 = 2,
		Vdtc3 = 3, // short term plasticity time constant
		           // for spike history, higher current->shorter
		           // averaging windowtime
		Vcb0 = 4,
		Vcb1 = 5,
		Vcb2 = 6,
		Vcb3 = 7, // spike driver comparator bias
		Vplb0 = 8,
		Vplb1 = 9,
		Vplb2 = 10,
		Vplb3 = 11, // spike driver pulse length bias,
		            // higher current->shorter internal pulse,
		            // important for short term plasticity
		Ibnoutampba = 12,
		Ibnoutampbb = 13, // both add together to the neuronoutampbias
		Ibcorrreadb = 14, // correlation read out bias
		num_biasb = 15
	};

	// Synapse Driver
	enum syndrv {
		drviout = 0,  // synapse drive output current (also stdf current)
		adjdel = 1,   // delay for pre-synaptic correlation pulse
		drvifall = 2, // bias current for falling edge
		drvirise = 3, // dito, rising edge
		num_syndrv = 4
	};

	// Neuron
	enum neuron {
		neurileak = 0, // neuron leakage ota bias current
		neuricb = 1,   // neuron spike comparator bias current
		num_neuron = 2
	};


	// *** return number of clock cycles for complete parameter RAM refresh
	uint get_pram_upd_cycles(void) { return lastPramCycleCnt; }

	// *** parameter ram with PramData struct
	void write_pram(uint adr, PramData& p, uint del = pramdefault)
	{
		write_pram(adr, p.cadr, p.value, p.lutadr, del);
	}
	bool check_pram(uint adr, PramData& p) { return check_pram(adr, p.cadr, p.value, p.lutadr); }

	// *** parameter ram on chip
	void write_pram(uint adr, uint cadr, uint value, uint lutadr, uint del = pramdefault)
	{
		write_cmd((((uint64_t)adr << sp->hw_const->pr_ramaddr_pos()) |
		           ((uint64_t)cadr << sp->hw_const->pr_paraddr_pos()) |
		           ((uint64_t)value << sp->hw_const->pr_dacval_pos()) |
		           ((uint64_t)lutadr << sp->hw_const->pr_lutadr_pos()) |
		           (sp->hw_const->pr_cmd_ram() << sp->hw_const->pr_cmd_pos())),
		          del);
	}

	void read_pram(uint adr, uint del = pramdefault)
	{
		read_cmd(((adr << sp->hw_const->pr_ramaddr_pos()) |
		          (sp->hw_const->pr_cmd_ram() << sp->hw_const->pr_cmd_pos())),
		         del);
	}

	bool check_pram(uint adr, uint cadr, uint value, uint lutadr)
	{
		uint64_t data = 0, val = (((uint64_t)adr << sp->hw_const->pr_ramaddr_pos()) |
		                          ((uint64_t)cadr << sp->hw_const->pr_paraddr_pos()) |
		                          ((uint64_t)value << sp->hw_const->pr_dacval_pos()) |
		                          ((uint64_t)lutadr << sp->hw_const->pr_lutadr_pos()) |
		                          (sp->hw_const->pr_cmd_ram() << sp->hw_const->pr_cmd_pos())),
		         mask = ((mmw(sp->hw_const->pr_ramaddr_width()) << sp->hw_const->pr_ramaddr_pos()) |
		                 (mmw(sp->hw_const->pr_paraddr_width()) << sp->hw_const->pr_paraddr_pos()) |
		                 (mmw(sp->hw_const->pr_dacval_width()) << sp->hw_const->pr_dacval_pos()) |
		                 (mmw(sp->hw_const->pr_lutadr_width()) << sp->hw_const->pr_lutadr_pos()) |
		                 (mmw(sp->hw_const->pr_cmd_width()) << sp->hw_const->pr_cmd_pos()));
		return check(data, mask, val);
	}

	// *** time lookup tables on chip
	// $ repeat and step fields added, lutadr moved to beginning for consistency!
	void write_lut(uint lutadr, uint luttime, uint lutboost, uint lutrepeat = 0, uint lutstep = 0,
	               uint del = pramdefault)
	{
		// FIXME: Should take struct LutData, too?
		write_cmd(((uint64_t)(luttime << sp->hw_const->pr_luttime_pos()) |
		           ((uint64_t)lutboost << sp->hw_const->pr_lutboost_pos()) |
		           ((uint64_t)lutrepeat << sp->hw_const->pr_lutrepeat_pos()) |
		           ((uint64_t)lutstep << sp->hw_const->pr_lutstep_pos()) |
		           ((uint64_t)lutadr << sp->hw_const->pr_lutadr_pos()) |
		           (sp->hw_const->pr_cmd_lut() << sp->hw_const->pr_cmd_pos())),
		          del);
	}

	void read_lut(uint64_t lutadr, uint del = 1)
	{
		read_cmd(((lutadr << sp->hw_const->pr_lutadr_pos()) |
		          (sp->hw_const->pr_cmd_lut() << sp->hw_const->pr_cmd_pos())),
		         del);
	}

	bool check_lut(uint lutadr, uint luttime, uint lutboost, uint lutrepeat = 0, uint lutstep = 0)
	{
		uint64_t data = 0, val = ((uint64_t)(luttime << sp->hw_const->pr_luttime_pos()) |
		                          ((uint64_t)lutboost << sp->hw_const->pr_lutboost_pos()) |
		                          ((uint64_t)lutrepeat << sp->hw_const->pr_lutrepeat_pos()) |
		                          ((uint64_t)lutstep << sp->hw_const->pr_lutstep_pos()) |
		                          ((uint64_t)lutadr << sp->hw_const->pr_lutadr_pos()) |
		                          (sp->hw_const->pr_cmd_lut() << sp->hw_const->pr_cmd_pos())),
		         mask =
		             ((mmw(sp->hw_const->pr_luttime_width()) << sp->hw_const->pr_luttime_pos()) |
		              (mmw(sp->hw_const->pr_lutboost_width()) << sp->hw_const->pr_lutboost_pos()) |
		              (mmw(sp->hw_const->pr_lutrepeat_width())
		               << sp->hw_const->pr_lutrepeat_pos()) |
		              (mmw(sp->hw_const->pr_lutstep_width()) << sp->hw_const->pr_lutstep_pos()) |
		              (mmw(sp->hw_const->pr_lutadr_width()) << sp->hw_const->pr_lutadr_pos()) |
		              (mmw(sp->hw_const->pr_cmd_width()) << sp->hw_const->pr_cmd_pos()));
		return check(data, mask, val);
	}

	vector<LutData> get_lut()
	{
		// LutData: uint luttime, lutboost, lutrepeat, lutstep;
		uint /*cadr,*/ normal, boost, repeat, step;
		vector<LutData> lut;
		// bool bug_found = false;

		for (uint i = 0; i < 16; ++i) // 16 lut entries
			read_lut(i, 100);         // lutadr, some delay

		for (uint i = 0; i < 16; ++i) {
			uint64_t data = 0;
			IData* idata;
			// assert(rcv_idata(&idata) == SpikenetComm::ok);
			rcv_idata(&idata);

			data = idata->data();

			// decoding data packet
			// cadr   = (data & (mmw(sp->hw_const->pr_lutadr_width())    <<
			// sp->hw_const->pr_lutadr_pos())   ) >> sp->hw_const->pr_lutadr_pos();
			normal = (data &
			          (mmw(sp->hw_const->pr_luttime_width()) << sp->hw_const->pr_luttime_pos())) >>
			         sp->hw_const->pr_luttime_pos();
			boost = (data &
			         (mmw(sp->hw_const->pr_lutboost_width()) << sp->hw_const->pr_lutboost_pos())) >>
			        sp->hw_const->pr_lutboost_pos();
			repeat = (data & (mmw(sp->hw_const->pr_lutrepeat_width())
			                  << sp->hw_const->pr_lutrepeat_pos())) >>
			         sp->hw_const->pr_lutrepeat_pos();
			step = (data &
			        (mmw(sp->hw_const->pr_lutstep_width()) << sp->hw_const->pr_lutstep_pos())) >>
			       sp->hw_const->pr_lutstep_pos();

			/*
			assert(i == cadr); // else: slow control mixed up -- got wrong results

			if(i != cadr) {
			    if(!bug_found)
			    std::cout << "Slow control is mixed up! Continue reading until all lut entries are
			read." << std::endl;
			    std::cout << "idata->cmd(): " << hex << idata->cmd() << dec << std::endl;
			    bug_found = true;
			}
			*/
			lut.push_back(LutData(normal, boost, repeat, step));

			delete idata;
		}

		/*
		if (bug_found) {
		    lut.push_back(LutData(normal, boost, repeat, step));

		    for(uint i = 0; i < lut.size(); ++i) {
		        printf("LUT%d %d %d %d %d\n", i, lut.at(i).luttime, lut.at(i).lutboost,
		                lut.at(i).lutrepeat, lut.at(i).lutstep);
		    }
		    std::cout << "Let's abort." << std::endl;
		    assert(false); // abort: )
		}
		*/

		return lut;
	}

	// *** high cycle clock count for voutclkx
	void write_period(uint period, uint del = pramdefault)
	{
		write_cmd(((period << sp->hw_const->pr_period_pos()) |
		           (sp->hw_const->pr_cmd_period() << sp->hw_const->pr_cmd_pos())),
		          del);
	}

	void read_period(uint del = pramdefault)
	{
		read_cmd(((sp->hw_const->pr_cmd_period() << sp->hw_const->pr_cmd_pos())), del);
	}

	bool check_period(uint period)
	{
		uint64_t data = 0, val = ((period << sp->hw_const->pr_period_pos()) |
		                          (sp->hw_const->pr_cmd_period() << sp->hw_const->pr_cmd_pos())),
		         mask = ((mmw(sp->hw_const->pr_period_width()) << sp->hw_const->pr_period_pos()) |
		                 (mmw(sp->hw_const->pr_cmd_width()) << sp->hw_const->pr_cmd_pos()));
		return check(data, mask, val);
	}

	// *** total number of parameters (including redundants) minus one
	void write_parnum(uint parnum, uint del = pramdefault)
	{
		write_cmd((((parnum - 1) << sp->hw_const->pr_num_pos()) |
		           (sp->hw_const->pr_cmd_num() << sp->hw_const->pr_cmd_pos())),
		          del);
	}

	void read_parnum(uint del = pramdefault)
	{
		read_cmd(((sp->hw_const->pr_cmd_num() << sp->hw_const->pr_cmd_pos())), del);
	}

	bool check_parnum(uint parnum)
	{
		uint64_t data = 0, val = (((parnum - 1) << sp->hw_const->pr_num_pos()) |
		                          (sp->hw_const->pr_cmd_num() << sp->hw_const->pr_cmd_pos())),
		         mask = ((mmw(sp->hw_const->pr_paraddr_width()) << sp->hw_const->pr_num_pos()) |
		                 (mmw(sp->hw_const->pr_cmd_width()) << sp->hw_const->pr_cmd_pos()));
		return check(data, mask, val);
	}

	// *** support for parameter ram management ***
	void sort_pd(vector<PramData>& in);
	void sort_pd_triangle(vector<PramData>& in);
	void blocksort_pd(vector<PramData>& in);
	int lookUpLUT(uint, uint, uint, vector<LutData>&);
	void check_pd_timing(vector<PramData>& in, vector<LutData>& lut);
	void write_pd(vector<PramData>& in, uint del, vector<LutData> lut);
};

// chip control register
class ChipControl : public ControlInterface
{
public:
	ChipControl(Spikenet* s) : ControlInterface(s) {}
	virtual uint get_cicmd() { return sp->hw_const->ci_controli(); } // ci subtype

	// *** write control register
	void setCtrl(uint64_t creg, uint del = 0) // basedelay=1
	{
		write_cmd(((creg << sp->hw_const->cr_pos()) |
		           (sp->hw_const->cr_sel_control() << sp->hw_const->cr_sel_pos())),
		          del);
	}

	void getCtrl() { read_cmd((sp->hw_const->cr_sel_control() << sp->hw_const->cr_sel_pos()), 0); }

	bool checkCtrl(uint64_t creg)
	{
		uint64_t data = 0, val = ((creg << sp->hw_const->cr_pos()) |
		                          (sp->hw_const->cr_sel_control() << sp->hw_const->cr_sel_pos())),
		         mask = (mmw(sp->hw_const->cr_width() << sp->hw_const->cr_pos()) |
		                 mmw(sp->hw_const->cr_sel_width() << sp->hw_const->cr_sel_pos()));
		return check(data, mask, val);
	}

	void read_clkstat(uint del = 0)
	{
		read_cmd((sp->hw_const->cr_sel_clkstat() << sp->hw_const->cr_sel_pos()), del);
	}

	void read_clkbstat(uint del = 0)
	{
		read_cmd((sp->hw_const->cr_sel_clkbstat() << sp->hw_const->cr_sel_pos()), del);
	}

	void read_clkhistat(uint del = 0)
	{
		read_cmd((sp->hw_const->cr_sel_clkhistat() << sp->hw_const->cr_sel_pos()), del);
	}


	// Acess to the event loopback register
	void setEvLb(uint64_t cmd, uint del = 0)
	{
		IData d(sp->hw_const->ci_evloopbacki(), cmd); // create data packet
		Spikenet* s = getSp();
		s->Send(SpikenetComm::write, d, cidelay + del);
	}
};

// loopback register
class Loopback : public ControlInterface
{
public:
	Loopback(Spikenet* s) : ControlInterface(s) {}
	virtual uint get_cicmd() { return sp->hw_const->ci_loopbacki(); } // ci subtype

	// *** write control register
	void loopback(uint64_t lbdata, uint del) { read_cmd(lbdata, del); }

	bool check_test(uint64_t lbdata)
	{
		uint64_t data = 0, val = (~lbdata) & mmm(52),
		         mask = mmm(52); // no correct constant in spikenet_base
		return check(data, mask, val);
	}
};


// set analog readback source
class AnalogReadout : public ControlInterface
{
private:
	int scdelay;

public:
	AnalogReadout(Spikenet* s)
	    : ControlInterface(s),
	      scdelay(30 + 2 * (sp->hw_const->ar_maxlen())),
	      voutl(sp->hw_const->ar_voutl()),
	      voutr(sp->hw_const->ar_voutr())
	{
	}
	virtual uint get_cicmd() { return sp->hw_const->ci_areadouti(); } // ci subtype

	// numbers inside chain are identical to parameter_ram definitions above
	const int voutl;
	const int voutr;

	// *** enable analog readback for chain c element num
	void set(int c, uint num, bool verify = false) { set(c, num, verify, scdelay); };
	void set(int c, uint num, bool verify, uint del);
	// *** enable analog readback for chain c, multiple elements voutsInt
	void setMultiple(int c, uint64_t voutsInt, bool verify = false)
	{
		setMultiple(c, voutsInt, verify, scdelay);
	}
	void setMultiple(int c, uint64_t voutsInt, bool verify, uint del)
	{
		assert(del >= scdelay);
		voutsInt = voutsInt | (c & mmw(sp->hw_const->ar_chainnumberwidth()));
		if (verify)
			read_cmd(voutsInt, del);
		else
			write_cmd(voutsInt, del);

		for (int i = sp->hw_const->ar_maxlen() - 1 + sp->hw_const->ar_chainnumberwidth(); i >= 0;
		     i--)
			std::cout << ((1ULL << i) & voutsInt ? "1" : "0");
		std::cout << std::endl;
	};


	// *** disable readback for chain c
	void clear(int c) { clear(c, scdelay); };
	void clear(int c, uint del);

	bool check_last(uint num, bool iszero);
};

} // end of namespace spikey2
