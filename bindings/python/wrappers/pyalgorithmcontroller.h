#define PY_ARRAY_UNIQUE_SYMBOL hal

#include <boost/python.hpp>

#include "AlgorithmController.h"

// forward declarations
class GraphModel;
class GMNode;
class GMConnection;

//! Python wrapper class for the C++ class AlgorithmController.
class PyAlgorithmController : public AlgorithmController
{
public:
	PyAlgorithmController();
	long ReadData(std::string NetListFileName, std::string HWInitFileName,
	              std::string HardwareModelName, std::string SerialFileName);
	long WriteData(std::string MappingFileName);
	GraphModel::GMNode& BioModelInsertNeuron(std::string name);
	void BioModelSetNeuronPosition(GraphModel::GMNode& neuron, float x, float y, float z);
	GraphModel::GMNode& BioModelInsertNeuronParameterSet(std::string name);
	void BioModelInsertParameter(GraphModel::GMNode& parameterSet, std::string name,
	                             std::string value);
	void BioModelAssignElementToParameterSet(GraphModel::GMNode& node,
	                                         GraphModel::GMNode& parameterSet);
	GraphModel::GMConnection& BioModelInsertSynapse(GraphModel::GMNode& source,
	                                                GraphModel::GMNode& target,
	                                                GraphModel::GMNode& parameterSet);
	GraphModel::GMNode& BioModelInsertSynapseParameterSet(std::string name);
	void BioModelPrintMapping(std::string filename);
	GraphModel::GMNode& HWModelInsertGlobalParameterSet(std::string name);
	void HWModelInsertParameter(GraphModel::GMNode& parameterSet, std::string name,
	                            std::string value);
	void HWModelPrintGlobalParameters(std::string parameterSetName);
	bool HWModelSetGlobalParameter(std::string parameterSetName, std::string parameterName,
	                               std::string newValue);
};
