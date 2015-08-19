#define PY_ARRAY_UNIQUE_SYMBOL hal
#define NO_IMPORT_ARRAY

#include "common.h"
#include "idata.h"
#include "spikeyconfig.h"

#include "pyspiketrain.h"

#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include <numpy/arrayobject.h>

// shortcut namespace
using namespace spikey2;
using namespace std;

PySpikeTrain::PySpikeTrain()
{
	// the python representation of a spiketrain is a 2-dimensional vector:
	// first row contains neuron numbers
	// second row contains spike times
	_trainForPython.resize(2);
}

boost::python::object PySpikeTrain::get()
{
	using namespace boost::python;

	const size_t size = this->d.size();
	const int num_dims = 1;
	npy_intp dims[] = {static_cast<npy_intp>(size)};

	object np_ids(handle<>(PyArray_SimpleNew(num_dims, dims, NPY_INT)));
	object np_spikes(handle<>(PyArray_SimpleNew(num_dims, dims, NPY_FLOAT)));

	int* ids = static_cast<int*>(PyArray_DATA((PyArrayObject*)np_ids.ptr()));
	float* spikes = static_cast<float*>(PyArray_DATA((PyArrayObject*)np_spikes.ptr()));

	for (size_t ii = 0; ii < size; ++ii) {
		if (this->d[ii].isEvent()) {
			*ids = this->d[ii].neuronAdr();
			*spikes = this->d[ii].time();
		} else {
			PyErr_SetString(PyExc_TypeError, "There is a non-event object within a SpikeTrain!");
			boost::python::throw_error_already_set();
		}
		++ids;
		++spikes;
	}

	return boost::python::make_tuple(np_spikes, np_ids);
}


void PySpikeTrain::set(vector<vector<int>>& vec)
{

	// if reducedResolution == true:
	// workaround for Spikey problem (last time bin sometimes lost, if spikey clk too high):
	// use only time bins 0, 4, 8, 12 !!!
	bool reducedResolution = true;

	if (vec[0].size() != vec[1].size()) {
		PyErr_SetString(PyExc_TypeError, "Inconsistent spike train was passed!");
		boost::python::throw_error_already_set();
	}

	// fill local data
	size_t mask = (~0);
	if (reducedResolution)
		mask = mask << 2;
	// cout << "mask: " << hex << mask << dec << endl;

	this->d.resize(vec[0].size());
	for (uint i = 0; i < vec[0].size(); ++i)
		this->d[i].setEvent(vec[1][i], (vec[0][i] & mask));
	// this->d[i] = (IData::Event(vec[1][i],(vec[0][i])&mask));

	return;
}


bool PySpikeTrain::writeToFile(string filename)
{
	return SpikeTrain::writeToFile(filename);
}


bool PySpikeTrain::readFromFile(string filename)
{
	return SpikeTrain::readFromFile(filename);
}
