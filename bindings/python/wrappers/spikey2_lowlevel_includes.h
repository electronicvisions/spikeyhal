// This is the class to be included if you want to use the hardware abstracting layers interfacing
// the Spikey neuromorphic system

#define XQUOTE(x) QUOTE(x)
#define QUOTE(x) #x


#ifndef SPIKEY2_LOWLEVEL_INCLUDES_H
#define SPIKEY2_LOWLEVEL_INCLUDES_H

#include XQUOTE(SPIKEYHALPATH/common.h) // library include
#include XQUOTE(SPIKEYHALPATH/hardwareConstants.h)
#include XQUOTE(SPIKEYHALPATH/hardwareConstantsRev4USB.h)
#include XQUOTE(SPIKEYHALPATH/hardwareConstantsRev5USB.h)
#include XQUOTE(SPIKEYHALPATH/idata.h)
#include XQUOTE(SPIKEYHALPATH/sncomm.h)
#include XQUOTE(SPIKEYHALPATH/spikenet.h)
#include XQUOTE(SPIKEYHALPATH/sc_sctrl.h)
#include XQUOTE(SPIKEYHALPATH/sc_pbmem.h)
#include XQUOTE(SPIKEYHALPATH/ctrlif.h)
#include XQUOTE(SPIKEYHALPATH/synapse_control.h) // synapse control class
#include XQUOTE(SPIKEYHALPATH/pram_control.h)    // parameter ram etc
#include XQUOTE(SPIKEYHALPATH/spikeyconfig.h)
#include XQUOTE(SPIKEYHALPATH/spikeyvoutcalib.h)
#include XQUOTE(SPIKEYHALPATH/spikey.h)
#include XQUOTE(SPIKEYHALPATH/spikeycalibratable.h)


#endif // SPIKEY2_LOWLEVEL_INCLUDES_H
