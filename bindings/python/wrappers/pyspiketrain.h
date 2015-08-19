#define PY_ARRAY_UNIQUE_SYMBOL hal
#include <boost/python.hpp>


using namespace spikey2;


//! Python wrapper class for the C++ class SpikeTrain. Container for event input/output data to be
//sent to/received from the Spikey neuromorphic system.
class PySpikeTrain : public SpikeTrain
{
public:
	//! constructor
	PySpikeTrain();
	//! return data as array
	boost::python::object get(void);
	//! fill data with 2-d array
	void set(std::vector<std::vector<int>>& arr);

	bool writeToFile(string filename);
	bool readFromFile(string filename);

private:
	vector<vector<int>> _trainForPython;
};
