#include "common.h"

#include "idata.h"
#include "sncomm.h"
#include "sc_sctrl.h"
#include "sc_pbmem.h"

#include "ctrlif.h"
#include "spikenet.h"

#include "pram_control.h"
#include "synapse_control.h"
#include "spikeyconfig.h"
#include "spikey.h"

namespace spikey2
{

class decorrNetwork
{
public:
	uint run();
};
}
