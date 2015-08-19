namespace spikey2
{

// synapse control corresponds to the functionality in synapse_control.v

class SynapseControl : public ControlInterface
{
	uint mixedmask; // Mask for mixed-signal simulation to ignore empty cell views
	std::vector<int> lut; // LUT in index notation: acausal [0..15] and causal [16..31]
	const uint tbmax; // number or LUT rows

public:
	SynapseControl(Spikenet* s) : ControlInterface(s), tbmax((1 << sp->hw_const->sc_plut_res()) - 1)
	{
		set_mmask(0xffffffff);
		lut.resize(2 * (tbmax + 1));
		// fill lut linear
		for (int i = 0; i < (tbmax + 1); i++) { // acausal
			lut[i] = i - 1;
			if (lut[i] < 0) {
				lut[i] = 0;
			}
		}
		for (int i = 0; i < (tbmax + 1); i++) { // causal
			lut[tbmax + 1 + i] = i + 1;
			if (lut[tbmax + 1 + i] > tbmax) {
				lut[tbmax + 1 + i] = tbmax;
			}
		}
	};

	uint get_cicmd() { return sp->hw_const->ci_synrami(); }; // ci subtype
	void set_mmask(uint m) { mixedmask = m; };
	uint get_mmask(void) { return mixedmask; };

	enum plutmode {
		none,
		linear = 1,
		custom = 2,
		write = 0x100,
		verify = 0x200
	}; // fill modes for plut table generation (none=0)

	// *** synapse ram
	void write_sram(uint trow, uint tcol, uint val, uint del = synramdelay)
	{
		write_cmd(sp->hw_const->sc_cmd_syn() | (((trow << (sp->hw_const->sc_colmsb() + 1)) | tcol |
		                                         ((uint64_t)val << sp->hw_const->sc_aw()))
		                                        << sp->hw_const->sc_commandwidth()),
		          del);
	};
	void read_sram(uint trow, uint tcol, uint del = synramdelay)
	{
		read_cmd(sp->hw_const->sc_cmd_syn() | (((trow << (sp->hw_const->sc_colmsb() + 1)) | tcol)
		                                       << sp->hw_const->sc_commandwidth()),
		         del);
	};
	bool check_sram(uint v)
	{
		uint64_t data = 0,
		         val = ((uint64_t)v << sp->hw_const->sc_aw()) << sp->hw_const->sc_commandwidth(),
		         mask = (mmw(sp->hw_const->sc_aw()) & get_mmask())
		                << sp->hw_const->sc_aw() << sp->hw_const->sc_commandwidth();
		return check(data, mask, val);
	}
	void close(uint del = 0) { write_cmd(sp->hw_const->sc_cmd_close(), del); }

	// *** read correlation
	void read_corr(uint trow, uint tcol, uint del = synramdelay)
	{
		read_cmd(sp->hw_const->sc_cmd_cor_read() |
		             (((trow << (sp->hw_const->sc_colmsb() + 1)) | tcol)
		              << sp->hw_const->sc_commandwidth()),
		         del);
	};
	bool check_corr(uint v)
	{
		uint64_t data = 0,
		         val = ((uint64_t)v << sp->hw_const->sc_aw()) << sp->hw_const->sc_commandwidth(),
		         mask = (mmw(sp->hw_const->sc_aw()) & get_mmask())
		                << sp->hw_const->sc_aw() << sp->hw_const->sc_commandwidth();
		return check(data, mask, val);
	};

	// *** process correlation
	// Using close=false the STDP controller is still connected to the synapses, this can fasten
	// sequent reads etc.
	// Default of close is true.
	void proc_corr(uint startrow, uint stoprow, bool close, uint del = synramdelay)
	{
		write_cmd((close ? sp->hw_const->sc_cmd_pcorc() : sp->hw_const->sc_cmd_pcor()) |
		              ((startrow | ((uint64_t)stoprow << sp->hw_const->sc_aw()))
		               << sp->hw_const->sc_commandwidth()),
		          del);
	};

	// *** time reg
	void write_time(uint tsense, uint tpcsec, uint trow, bool readwait = false, uint del = 0)
	{
		write_cmd(sp->hw_const->sc_cmd_time() |
		              (readwait ? (1 << sp->hw_const->sc_pcreadlenpos()) : 0) |
		              (((((trow << (sp->hw_const->sc_pcsecdelaymsb() + 1)) | tpcsec)
		                 << (sp->hw_const->sc_csensedelaymsb() + 1)) |
		                tsense)
		               << sp->hw_const->sc_commandwidth()),
		          del);
	};
	void read_time(uint del = 0) { read_cmd(sp->hw_const->sc_cmd_time(), del); };
	bool check_time(uint tsense, uint tpcsec, uint trow, bool readwait = false)
	{
		uint64_t data = 0, val = (readwait ? (1 << sp->hw_const->sc_pcreadlenpos()) : 0) |
		                         (((((trow << (sp->hw_const->sc_pcsecdelaymsb() + 1)) | tpcsec)
		                            << (sp->hw_const->sc_csensedelaymsb() + 1)) |
		                           tsense)
		                          << sp->hw_const->sc_commandwidth()),
		         mask = mmw(sp->hw_const->sc_tregw()) << sp->hw_const->sc_commandwidth();
		return check(data, mask, val);
	};

	// *** control reg
	void write_ctrl(bool dllreset, bool neuronreset, bool pccont, uint del = 0)
	{
		write_cmd(
		    sp->hw_const->sc_cmd_ctrl() |
		        (((dllreset ? (1 << sp->hw_const->sc_sreg_dllres()) : 0) |
		          (neuronreset ? 0 : (1 << sp->hw_const->sc_sreg_nresetb())) // invert neuronreset
		          | (pccont ? (1 << sp->hw_const->sc_sreg_pccont()) : 0))
		         << sp->hw_const->sc_commandwidth()),
		    del);
	};

	void read_ctrl(uint del = 0) { read_cmd(sp->hw_const->sc_cmd_ctrl(), del); };

	// *** process correlation lookup table
	void write_plut(uint adr, uint val)
	{
		write_cmd(sp->hw_const->sc_cmd_plut() | ((adr | ((uint64_t)val << sp->hw_const->sc_aw()))
		                                         << sp->hw_const->sc_commandwidth()),
		          1);
	};

	void read_plut(uint adr)
	{
		read_cmd(sp->hw_const->sc_cmd_plut() | (adr << sp->hw_const->sc_commandwidth()), 1);
	};

	bool check_plut(uint v)
	{
		uint64_t data = 0, val = ((((uint64_t)v << sp->hw_const->sc_aw()))
		                          << sp->hw_const->sc_commandwidth()),
		         mask = (((mmw(sp->hw_const->sc_cprienc() * sp->hw_const->sc_plut_res())
		                   << sp->hw_const->sc_aw()))
		                 << sp->hw_const->sc_commandwidth());
		return check(data, mask, val);
	};

	void set_LUT(std::vector<int> _lut);
	uint gen_plut_data(float nval);
	bool fill_plut(int mode, float slope = 0, float off = 0);
};

} // end of namespace spikey2
