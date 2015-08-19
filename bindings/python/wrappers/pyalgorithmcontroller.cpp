#define PY_ARRAY_UNIQUE_SYMBOL hal
#define NO_IMPORT_ARRAY

#include <iostream>
#include "pyalgorithmcontroller.h"
#include "GraphModell.h"
#include <string>

using namespace std;

PyAlgorithmController::PyAlgorithmController() : AlgorithmController("SpikeyModel")
{
	Init(false, false, false, false);
	printf("PyAlgorithmController: Creating Bio-Modell...\n");
	_BioModel->Init("Bio-System");
	_BioModel->TextOutFlag = false;
	_BioModel->MakeCompactModel = true;
	printf("PyAlgorithmController: Creating Bio-Modell done\n");

	printf("PyAlgorithmController: Creating HW-Modell for hardware type Stage1... ");
	basics::string<> hwinitfile = getenv("PYNN_HW_PATH");
	hwinitfile << "/MappingTool/stage1init.pl";
	_HWModel->PreInit(hwinitfile);
	_HWModel->Init("SpikeyModel");
	_HWModel->TextOutFlag = false;
	_HWModel->CreateModel();
	printf(" done\n");
}

long PyAlgorithmController::ReadData(std::string NetListFileName, std::string HWInitFileName,
                                     std::string HardwareModelName, std::string SerialFileName)
{
	return AlgorithmController::ReadData(
	    basics::string<>(NetListFileName.c_str()), basics::string<>(HWInitFileName.c_str()),
	    std::string(HardwareModelName.c_str()), basics::string<>(SerialFileName.c_str()));
}

long PyAlgorithmController::WriteData(std::string MappingFileName)
{
	return AlgorithmController::WriteData(basics::string<>(MappingFileName.c_str()));
}


GraphModel::GMNode& PyAlgorithmController::BioModelInsertNeuron(std::string name)
{
	GraphModel::GMNode* neuronNode = _BioModel->InsertNeuron(basics::string<>(name.c_str()));
	return *neuronNode;
}

void PyAlgorithmController::BioModelSetNeuronPosition(GraphModel::GMNode& neuron, float x, float y,
                                                      float z)
{
#ifdef _VISU
	neuron.pos[0] = x;
	neuron.pos[1] = y;
	neuron.pos[2] = z;
#endif
}

GraphModel::GMNode& PyAlgorithmController::BioModelInsertNeuronParameterSet(std::string name)
{
	GraphModel::GMNode* parameterSetNode =
	    _BioModel->InsertNeuronParameterSet(basics::string<>(name.c_str()));
	return *parameterSetNode;
}

void PyAlgorithmController::BioModelInsertParameter(GraphModel::GMNode& parameterSet,
                                                    std::string parameterName, std::string value)
{
	// check if parameter is already existing
	GraphModel::GMNode* parameterNameNode = parameterSet.GetChildList(parameterName.c_str())[0];
	if (parameterNameNode == GraphModel::GMNode::InvalidNode()) {
		// if not yet existing, create parameter (name plus value)
		_BioModel->InsertParameter(&parameterSet, basics::string<>(parameterName.c_str()),
		                           basics::string<>(value.c_str()));
		return;
	}
	GraphModel::GMNode* parameterValueNode =
	    parameterNameNode->GetOutConnectionList(EQUAL)[0]->Target;
	if (parameterValueNode == GraphModel::GMNode::InvalidNode()) {
		parameterValueNode =
		    _BioModel->CreateNode(*parameterNameNode, basics::string<>(value.c_str()));
		_BioModel->Connect(*parameterNameNode, *parameterValueNode, EQUAL);
		return;
	}
	delete parameterValueNode->Name;
	parameterValueNode->Name = new char[1 + value.length()];
	strcpy(parameterValueNode->Name, value.c_str());
	return;
}

void PyAlgorithmController::BioModelAssignElementToParameterSet(GraphModel::GMNode& element,
                                                                GraphModel::GMNode& parameterSet)
{
	_BioModel->AssignElementToParameterSet(&element, &parameterSet);
}


GraphModel::GMConnection&
PyAlgorithmController::BioModelInsertSynapse(GraphModel::GMNode& source, GraphModel::GMNode& target,
                                             GraphModel::GMNode& parameterSet)
{
	GraphModel::GMConnection* synapseConnection =
	    _BioModel->InsertSynapse(&source, &target, &parameterSet);
	return *synapseConnection;
}

GraphModel::GMNode& PyAlgorithmController::BioModelInsertSynapseParameterSet(std::string name)
{
	GraphModel::GMNode* parameterSetNode =
	    _BioModel->InsertSynapticParameterSet(basics::string<>(name.c_str()));
	return *parameterSetNode;
}


void PyAlgorithmController::BioModelPrintMapping(std::string filename)
{
	basics::CFile outputFile(filename.c_str(), "w");
	_BioModel->PrintMapping(*_BioModel->SystemNode, outputFile);
	outputFile.close();
}

GraphModel::GMNode& PyAlgorithmController::HWModelInsertGlobalParameterSet(std::string name)
{
	GraphModel::GMNode* parameterSetNode;
	parameterSetNode =
	    _HWModel->CreateNode(*_HWModel->globalParametersNode, basics::string<>(name.c_str()));
	return *parameterSetNode;
}

void PyAlgorithmController::HWModelInsertParameter(GraphModel::GMNode& parameterSet,
                                                   std::string name, std::string value)
{
	GraphModel::GMNode* ParameterName =
	    _HWModel->CreateNode(parameterSet, basics::string<>(name.c_str()));
	GraphModel::GMNode* ParameterValue =
	    _HWModel->CreateNode(*ParameterName, basics::string<>(value.c_str()));
	_HWModel->Connect(*ParameterName, *ParameterValue, EQUAL);
}

void PyAlgorithmController::HWModelPrintGlobalParameters(std::string parameterSetName)
{
	GraphModel::GMNode* GlobalParameters = _HWModel->globalParametersNode;

	basics::string<> path;
	path << "HERE/" << parameterSetName.c_str() << "/* \n";
	std::vector<GraphModel::GMNode*> paramListRef = GlobalParameters->GetNodesViaPath(path);
	std::vector<GraphModel::GMNode*>* paramList = &paramListRef;

	if (paramList == NULL) {
		printf("Requested parameter list is not existing!\n");
		return;
	}
	if (paramList->size() == 0) {
		printf("Requested parameter list is empty!\n");
		return;
	}
	for (uint i = 0; i < paramList->size(); ++i) {
		GraphModel::GMNode* nameNode = paramList->at(i);
		GraphModel::GMNode* valueNode = paramList->at(i)->GetOutConnectionList(EQUAL)[0]->Target;
		cout << nameNode->Name << ": " << valueNode->Name << endl;
	}
}

bool PyAlgorithmController::HWModelSetGlobalParameter(std::string parameterSetName,
                                                      std::string parameterName,
                                                      std::string newValue)
{
	GraphModel::GMNode* parameterSetNode =
	    _HWModel->SystemNode->GetChildList("GlobalParameters")[0]->GetChildList(
	        basics::string<>(parameterSetName.c_str()))[0];
	if (parameterSetNode == GraphModel::GMNode::InvalidNode())
		return false;

	GraphModel::GMNode* parameterNameNode =
	    parameterSetNode->GetChildList(parameterName.c_str())[0];
	if (parameterNameNode == GraphModel::GMNode::InvalidNode())
		return false;

	GraphModel::GMNode* parameterValueNode =
	    parameterNameNode->GetOutConnectionList(EQUAL)[0]->Target;
	if (parameterValueNode == GraphModel::GMNode::InvalidNode())
		return false;
	// else parameterValueNode->Name = basics::string<>(newValue.c_str());
	delete parameterValueNode->Name;
	parameterValueNode->Name = new char[1 + newValue.length()];
	strcpy(parameterValueNode->Name, newValue.c_str());
	return true;
}
