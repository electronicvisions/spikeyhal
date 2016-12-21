// This file is the main interface between the c++-based low level hardware
// access and the Python-based high level software framework called PyHAL.
// It includes the c++ code, the wrapper classes and contains the new Python
// class definitions.


// this define has to be included in every c++ source code file for the python interface
#define PY_ARRAY_UNIQUE_SYMBOL hal

// allows easy wrapping of stl vectors for access from python
#include <boost/python/suite/indexing/vector_indexing_suite.hpp>
#include <boost/python/return_internal_reference.hpp>
#include <boost/python/enum.hpp>
#include <boost/python/def.hpp>

// the symap2ic logging mechanism
#include "logger.h"

#ifndef WITHOUT_HARDWARE
// include all low level hardware access header files
#include "spikey2_lowlevel_includes.h"
// include c++-python wrapper files
#include "pyspiketrain.h"
#include "pyspikeyconfig.h"
#include "pysncomm.h"
#include "pysc_mem.h"
#include "pyspikey.h"
#include "pyhwtest.h"
#define HARDWARE_AVAILABLE true
using namespace spikey2;
uint randomseed = 42;
#else
#define HARDWARE_AVAILABLE false
#endif // WITHOUT_HARDWARE

#ifdef WITH_GRAPHMODEL
// include c++-python wrapper files
#include "pyalgorithmcontroller.h"
#endif // WITH_GRAPHMODEL

#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include <numpy/arrayobject.h>


//! gives back hardware availability status by checking pre-processor define
bool hardwareAvailable()
{
	return HARDWARE_AVAILABLE;
}

//! writes log messages to files or to stdout
void toLog(int level, std::string message)
{
	//! acquire (and - if createLogger was not called before - create) the one and only logger
	//instance
	Logger& logger =
	    Logger::instance("spikeyhal.python.toLog_deprecated", Logger::WARNING, "logfile.txt", true);
	logger(level) << "PyNN: " << message;
}

//! If no logger was created yet, it will be done. Otherwise, this function HAS NO EFFECT.
void createLogger(int level, std::string filename)
{
	//! create the one and only logger instance
	Logger::instance("spikeyhal.python.toLog_deprecated", level, filename, true);
}

// default argument handling
BOOST_PYTHON_MEMBER_FUNCTION_OVERLOADS(pyspikey_overloads_autocalib, autocalib, 0, 2)

// python-module supported by BOOST library
BOOST_PYTHON_MODULE(pyhal_c_interface_s1v2)
{
	using namespace boost::python;

	// For numpy C API
	import_array();

	//! returns hardware availability status
	def("hardwareAvailable", &hardwareAvailable);

	//! a logging function, which takes the criticality level and a message string
	def("createLogger", &createLogger);

	//! a logging function, which takes the criticality level and a message string
	def("toLog", &toLog);

	//! the criticality levels of log messages
	enum_<Logger::levels>("level")
	    .value("ERROR", Logger::ERROR)
	    .value("WARNING", Logger::WARNING)
	    .value("INFO", Logger::INFO)
	    .value("DEBUG0", Logger::DEBUG0)
	    .value("DEBUG1", Logger::DEBUG1)
	    .value("DEBUG2", Logger::DEBUG2)
	    .value("DEBUG3", Logger::DEBUG3)
	    .export_values();

	//! python access to stl "unsigned byte" vectors
	class_<std::vector<ubyte>>("vectorUbyte").def(vector_indexing_suite<std::vector<ubyte>>());

	//! python access to stl "unsigned short" vectors
	class_<std::vector<uint16_t>>("vectorUshort")
	    .def(vector_indexing_suite<std::vector<uint16_t>>());

	//! python access to stl integer vectors
	class_<std::vector<int>>("vectorInt").def(vector_indexing_suite<std::vector<int>>());

	//! python access to stl float vectors
	class_<std::vector<float>>("vectorFloat").def(vector_indexing_suite<std::vector<float>>());

	//! python access to stl double vectors
	class_<std::vector<double>>("vectorDouble").def(vector_indexing_suite<std::vector<double>>());

	//! python access to stl vectors of int vectors
	class_<std::vector<std::vector<int>>>("vectorVectorInt")
	    .def(vector_indexing_suite<std::vector<std::vector<int>>>());

	//! python access to stl vectors of double vectors
	class_<std::vector<std::vector<double>>>("vectorVectorDouble")
	    .def(vector_indexing_suite<std::vector<std::vector<double>>>());

#ifndef WITHOUT_HARDWARE
	//! python access to spike trains
	class_<PySpikeTrain>("SpikeTrain")
	    .add_property("data", &PySpikeTrain::get, &PySpikeTrain::set)
	    .def("writeToFile", &PySpikeTrain::writeToFile)
	    .def("readFromFile", &PySpikeTrain::readFromFile);

	//! python access to spikey's configuration class
	class_<PySpikeyConfig>("SpikeyConfig")
	    .def("initialize", &PySpikeyConfig::initialize)
	    .def("clearConfig", &PySpikeyConfig::clearConfig)
	    .def("clearRowConfig", &PySpikeyConfig::clearRowConfig)
	    .def("clearColConfig", &PySpikeyConfig::clearColConfig)
	    .def("clearParams", &PySpikeyConfig::clearParams)
	    .def("clearWeights", &PySpikeyConfig::clearWeights)
	    .def("readConfigFile", &PySpikeyConfig::readConfigFile)
	    .def("enableNeuron", &PySpikeyConfig::enableNeuron)
	    .def("enableMembraneMonitor", &PySpikeyConfig::enableMembraneMonitor)
	    .def("membraneMonitorEnabled", &PySpikeyConfig::membraneMonitorEnabled)
	    .def("writeConfigFile", &PySpikeyConfig::writeConfigFile)
	    .def("setSynapseDriver", &PySpikeyConfig::setSynapseDriver)
	    .def("setWeights", &PySpikeyConfig::setWeights)
	    .def("setVoltages", &PySpikeyConfig::setVoltages)
	    .def("setVoltageBiases", &PySpikeyConfig::setVoltageBiases)
	    .def("setBiasbs", &PySpikeyConfig::setBiasbs)
	    .def("setILeak", &PySpikeyConfig::setILeak)
	    .def("setIcb", &PySpikeyConfig::setIcb)
	    .def("getILeak", &PySpikeyConfig::getILeak)
	    .def("getIcb", &PySpikeyConfig::getIcb)
	    .def("setSTDPParams", &PySpikeyConfig::setSTDPParams)
	    .def_readwrite("biasb", &PySpikeyConfig::biasb)
	    .def_readwrite("vout", &PySpikeyConfig::vout)
	    .def_readwrite("voutbias", &PySpikeyConfig::voutbias)
	    .def_readwrite("vm", &PySpikeyConfig::vm)
	    .def_readonly("weight", &PySpikeyConfig::weight_wrapper);

	//! python access to spikenet communication class
	class_<PySpikenetComm, boost::noncopyable>("SpikenetComm", init<std::string>())
	    .def("Send", pure_virtual(&SpikenetComm::Send))
	    .def("Receive", pure_virtual(&SpikenetComm::Receive));
	enum_<PySpikenetComm::Commstate>("Commstate")
	    .value("ok", PySpikenetComm::ok)
	    .value("openfailed", PySpikenetComm::openfailed)
	    .value("eof", PySpikenetComm::eof)
	    .value("readfailed", PySpikenetComm::readfailed)
	    .value("writefailed", PySpikenetComm::writefailed)
	    .value("parametererror", PySpikenetComm::parametererror)
	    .value("readtimeout", PySpikenetComm::readtimeout)
	    .value("writetimeout", PySpikenetComm::writetimeout)
	    .value("pbreadempty", PySpikenetComm::pbreadempty)
	    .value("pbwriteinh", PySpikenetComm::pbwriteinh)
	    .value("notimplemented", PySpikenetComm::notimplemented)
	    .export_values();

	//! python access to playback memory management class
	class_<PySC_Mem, boost::shared_ptr<PySC_Mem>>("SC_Mem", init<uint, std::string>())
	    .def("intClear", &PySC_Mem::intClear)
	    .def("minTimebin", &PySC_Mem::minTimebin)
	    .def("stime", &PySC_Mem::stime)
	    .def("getWorkStationName", &PySC_Mem::getWorkStationName);

	//! python access to spikey class
	class_<PySpikey>("Spikey", init<boost::shared_ptr<PySC_Mem>, float, uint, uint, std::string>())
	    .def("config", &PySpikey::config)
	    .def("interruptActivity", &PySpikey::interruptActivity)
	    .def("getTemp", &PySpikey::getTemp)
	    .def("autocalib", &PySpikey::autocalib,
	         pyspikey_overloads_autocalib(args("cfg"), "vout autocalib"))
	    .def("assignMembranePin2TestPin", &PySpikey::assignMembranePin2TestPin)
	    .def("assignVoltage2TestPin", &PySpikey::assignVoltage2TestPin)
	    .def("assignMultipleVoltages2IBTest", &PySpikey::assignMultipleVoltages2IBTest)
	    .def("calibParam", &PySpikey::calibParam)
	    .def("clearPlaybackMem", &PySpikey::clearPlaybackMem)
	    .def("sendSpikeTrain", &PySpikey::sendSpikeTrain)
	    .def("Run", &PySpikey::Run)
	    .def("waitPbFinished", &PySpikey::waitPbFinished)
	    .def("resendSpikeTrain", &PySpikey::resendSpikeTrain)
	    .def("recSpikeTrain", &PySpikey::recSpikeTrain)
	    .def("checkSynRam", &PySpikey::checkSynRam)
	    .def("setLUT", &PySpikey::setLUT)
	    .def("getLUT", &PySpikey::getLUT)
	    .def("setSTDPParamsCont", &PySpikey::setSTDPParamsCont)
	    .def("sendSpikeTrainWithSTDP", &PySpikey::sendSpikeTrainWithSTDP)
	    .def("getCorrFlag", &PySpikey::getCorrFlag)
	    .def("getCorrFlags", &PySpikey::getCorrFlags)
	    .def("getSynapseWeight", &PySpikey::getSynapseWeight)
	    .def("getSynapseWeights", &PySpikey::getSynapseWeights)
	    .def("correlationInformation", &PySpikey::correlationInformation)
	    .def("flushPlaybackMemory", &PySpikey::flushPlaybackMemory)
	    .def("getVoltages", &PySpikey::getVoltages)
	    .def("getIbCurr", &PySpikey::getIbCurr)
	    .def("getVout", &PySpikey::getVout)
	    .def("write_lut", &PySpikey::write_lut) // FIXME: PramControl class should be wrapped
	                                            // instead?
	    .def("version", &PySpikey::revision)
	    .def("setGlobalTrigger", &PySpikey::setGlobalTrigger)
	    .def("setupFastAdc", &PySpikey::setupFastAdc)
	    .def("readFastAdc", &PySpikey::readFastAdc);

	//! python access to spikey test mode class
	class_<PySpikeyTM>("SpikeyTM", init<uint, uint>())
	    .def("getError", &PySpikeyTM::getError)
	    .def("test", &PySpikeyTM::test)
	    .def("link", &PySpikeyTM::link)
	    .def("parameterRAM", &PySpikeyTM::parameterRAM)
	    .def("eventLoopback", &PySpikeyTM::eventLoopback);
#endif

#ifdef WITH_GRAPHMODEL

	//! python access to bio model class
	class_<BioModel>("BioModel");

	//! python access to hardware model class
	class_<HWModelStage1>("HWModel");

	//! python access to graph model node class
	class_<GraphModel::GMNode>("GMNode")
#ifdef _VISU
	    .add_property("posX", &GraphModel::GMNode::getpositionX, &GraphModel::GMNode::setpositionX)
	    .add_property("posY", &GraphModel::GMNode::getpositionY, &GraphModel::GMNode::setpositionY)
	    .add_property("posZ", &GraphModel::GMNode::getpositionZ, &GraphModel::GMNode::setpositionZ)
#endif
	    ;

	//! python access to graph model connection class
	class_<GraphModel::GMConnection>("GMConnection");

	//! python access to algorithm controller class
	class_<PyAlgorithmController>("AlgorithmController")
	    .def("ReadData", &PyAlgorithmController::ReadData)
	    .def("WriteData", &PyAlgorithmController::WriteData)
	    .def("Start", &PyAlgorithmController::Start)
	    .def("EraseModels", &PyAlgorithmController::EraseModels)
	    .def("BioModelInsertNeuron", &PyAlgorithmController::BioModelInsertNeuron,
	         return_internal_reference<>())
	    .def("BioModelSetNeuronPosition", &PyAlgorithmController::BioModelSetNeuronPosition)
	    .def("BioModelInsertNeuronParameterSet",
	         &PyAlgorithmController::BioModelInsertNeuronParameterSet,
	         return_internal_reference<>())
	    .def("BioModelInsertParameter", &PyAlgorithmController::BioModelInsertParameter)
	    .def("BioModelAssignElementToParameterSet",
	         &PyAlgorithmController::BioModelAssignElementToParameterSet)
	    .def("BioModelInsertSynapse", &PyAlgorithmController::BioModelInsertSynapse,
	         return_internal_reference<>())
	    .def("BioModelInsertSynapseParameterSet",
	         &PyAlgorithmController::BioModelInsertSynapseParameterSet,
	         return_internal_reference<>())
	    .def("BioModelPrintMapping", &PyAlgorithmController::BioModelPrintMapping)
	    .def("HWModelInsertParameter", &PyAlgorithmController::HWModelInsertParameter,
	         return_internal_reference<>())
	    .def("HWModelInsertGlobalParameterSet",
	         &PyAlgorithmController::HWModelInsertGlobalParameterSet, return_internal_reference<>())
	    .def("HWModelPrintGlobalParameters", &PyAlgorithmController::HWModelPrintGlobalParameters)
	    .def("HWModelSetGlobalParameter", &PyAlgorithmController::HWModelSetGlobalParameter)
#ifdef _VISU
	    .def("StartVisualization", &PyAlgorithmController::StartVisualization)
	    .def("StopVisualization", &PyAlgorithmController::StopVisualization)
#endif
	    ;

#endif // WITH_GRAPHMODEL
}
