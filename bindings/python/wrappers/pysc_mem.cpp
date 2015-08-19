#define PY_ARRAY_UNIQUE_SYMBOL hal
#define NO_IMPORT_ARRAY

#include "spikey2_lowlevel_includes.h"
#include "pysc_mem.h"

// shortcut namespace
using namespace spikey2;


PySC_Mem::PySC_Mem(uint starttime, std::string workstation) : SC_Mem(starttime, workstation)
{
}


void PySC_Mem::intClear()
{
	SC_Mem::intClear();
}


int PySC_Mem::minTimebin()
{
	boost::shared_ptr<SC_SlowCtrl> sc = this->getSCTRL();
	return ((sc->getSyncMax() + sc->getEvtLatency()) << 5);
} // 400MHz and timebins


int PySC_Mem::stime()
{
	return stime();
}

std::string PySC_Mem::getWorkStationName()
{
	return SC_Mem::getWorkStationName();
}
