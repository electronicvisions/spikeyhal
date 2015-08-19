#define PY_ARRAY_UNIQUE_SYMBOL hal
#define NO_IMPORT_ARRAY

#include "spikey2_lowlevel_includes.h"
#include "pysncomm.h"


using namespace spikey2;

SpikenetComm::Commstate PySpikenetComm::Send(Mode mode, IData data, uint del, uint chip,
                                             uint syncoffset)
{
	return this->get_override("Send")(mode, data, del, chip, syncoffset);
}

SpikenetComm::Commstate PySpikenetComm::Receive(Mode mode, IData& data, uint chip)
{
	return this->get_override("Receive")(mode, data, chip);
}
