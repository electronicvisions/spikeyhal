// forward declarations required outside namespaces!
class FacetsHWS1V2Access;
class PySC_Mem;
class TMEventPbci;
class TMEvBugRnd;
class TMEvBugReg;
class TMSpikeyClass;


namespace spikey2
{

// ***** busmode playback memory *****


class SC_Mem : public SpikenetComm
{
	// for fpga debugging
	friend class ::TMEventPbci;
	friend class ::TMEvBugRnd;
	friend class ::TMEvBugReg;
	friend class ::TMSpikeyClass;
	friend class ::FacetsHWS1V2Access;
	friend class ::PySC_Mem;

private:
	// internal SC_SlowCtrl class to perfom real hardware access
	boost::shared_ptr<SC_SlowCtrl> sc;

	// playback mem
	static const uint wsize = (1 << 25); // 256MB write buffer size in words (64bit!)
	uint wstart,                         // write start address in FlySpi memory (64bit!)
	    wadr;                            // current write address

	// storage variables
	uint laststartadr, lastnumread;

	// receive mem
	//! ADC read buffer size: 128MB in words (64bit aligned)
	static const uint adcsize = (1 << 24);
	//! Record buffer size: 128MB in words (64bit aligned)
	static const uint rsize = (1 << 24);
	//! start address of record memory (64bit aligned), second memory module starts with address for
	//512MB
	static const uint roffs = (1 << 26);

	uint rstart, // read start address in FlySpi memory (64bit!)
	    rmvadr,  // read max valid adr, rstart to rmvadr-1 is valid in the SDRAM
	    radr,    // actual read pointer, rstart to radr-1 has been already received
	    recupto; // last block read up to this address

	vector<uint64_t> readbuf; //!< vector to store data received from playback memory

	queue<uint> skip;       // holds positions in read data to be skipped
	queue<uint> saved_skip; // holds positions in read data to be skipped of last flushed experiment
	                        // (for recall stuff)
	uint sendidx, recidx;   // track numer of read commands issued during send/receive

	bool inrec, insend, flushed;
	vector<IData> pbdat; // data for playback memory

	vector<IData> rcvev[maxid]; // sort received events in this buffer

	static const uint max_poll_time = 10000; //!< max number of milli seconds while waiting for
	                                         //playback memory to become idle

public:
	vector<IData>* rcvd(uint c) { return &(rcvev[c]); }; // received events
	vector<IData>* eev(uint c) { return sc->eev(c); };   // dropped/modified events

	boost::shared_ptr<SC_SlowCtrl> getSCTRL() { return sc; };

	// look like sc_sctrl to the outside world
	SC_Mem(uint time = 0, std::string workstation = "");
	virtual ~SC_Mem(){};

	virtual void resetFlushed() { flushed = false; }

	virtual Commstate Send(Mode mode, IData data = emptydata, uint del = 0, uint chip = 0,
	                       uint syncoffset = 0);
	virtual Commstate Receive(Mode mode, IData& data, uint chip = 0);
	virtual Commstate Flush(uint chip);
	virtual Commstate Run();
	virtual Commstate Clear() { return intClear(); };


	// sideband data is directly transferred to the sc class -> therefore firein pulses are NOT
	// synchronized to playback data
	// only useful for DAC settings
	virtual Commstate SendSBD(Mode mode, const SBData& data = emptysbdata, uint del = 0,
	                          uint chip = 0)
	{
		return sc->SendSBD(mode, data, del, chip);
	};
	virtual Commstate RecSBD(Mode mode, SBData& data, uint chip = 0)
	{
		return sc->RecSBD(mode, data, chip);
	};

private:
	// hack!
public:
	// new private methods
	Commstate intClear(); // non-virtual clear to be called from the constructor
	Commstate initChip(); // initialize Spikey after power up
	void pbTrans(); // copy pbdat to pbmem according to inci state

	// hack!
public:
	// save current playback memory state
	// -> actually represents a MemObj and is sort of a quick hack to recall an already sent
	// SpikeTrain.
	// -> DO NOT execute intClear when/before using this function as memory content could be
	// overwritten subsequently!!!
	void savePbContent();
	void recallPbContent(); // recalls results from savePbContent()
	void reFlush();         // re-executes a recalled playback memory program

	// wait until playback memory is idle and update write pointer
	void waitIdle();

	uint getWadr() { return wadr; };
	std::string getWorkStationName() { return sc->getWorkStationName(); }
};

} // end of namespace spikey2
