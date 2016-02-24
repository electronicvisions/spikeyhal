#include <QtXml/qdom.h>
#include <iostream>
#include <algorithm>
#include "logger.h"

namespace spikey2
{

using namespace std;

class SpikeyCalibratable;
class SpikeyConfig;

// ****** calibrates the Vouts of a spikey chip ******
class SpikeyVoutCalib
{
	friend class Spikey;

public:
	// defines a linear function (f(x) = a + b * x)
	struct linearFunction {
		double offset, slope, cov00, cov01, cov11, chisq;
		double start, stop;     // fit range on x axis
		uint startIdx, stopIdx; // fit range on array
	};

	// ******* Constructor *****
	SpikeyVoutCalib(SpikeyCalibratable* myS, int nr, string calibfile = "spikeycalib.xml");
	// ******* Destructor *****
	virtual ~SpikeyVoutCalib(){};

	// ******* variables *******
	vector<float> corSlope;
	vector<float> corOff;
	vector<float> validMin;
	vector<float> validMax;
	int spikeyNr;

	// ******** methods ********
	// opens XML data, searches and sets values for vout calibration
	virtual bool readCalib(string calibFile = "spikeycalib.xml");
	// automaticalliy searches Calibration Values of Spikey[spikeyNr]
	virtual bool
	autoCalibRobust(string calibFile = "spikeycalib.xml", string filenamePlot = "",
	                boost::shared_ptr<SpikeyConfig> mycfg = boost::shared_ptr<SpikeyConfig>());
	// read adc for a set of target values
	void readVoutValues(uint block, uint voutnr, uint avgRuns, double* targetV, uint n,
	                    double* mean, double* std, bool msgs = true);
	void loadConfig(boost::shared_ptr<SpikeyConfig> mycfg){mySpikeyConfig = mycfg;};

private:
	SpikeyCalibratable* mySpikey;
	boost::shared_ptr<SpikeyConfig> mySpikeyConfig;
	boost::shared_ptr<SpikeyConfig> mycfg;
	// writes Calibration Values of Spikey[spikeyNr] to XML-file (calibFile)
	virtual bool writeCalib(string calibFile = "spikeycalib.xml");
	// search calibration values in XML data
	bool calibSearchXML(QDomElement, int spikeyNr);

	// check calibration and calculate chisq of fitted
	void checkVoutCalib();

	// ******* Fitting & helper stuff *******
	// find first value breaking a threshold value right (reverse) of mid in array values of length
	// len
	unsigned int findFirst(bool reverse, double threshold, double* values, uint len, uint mid)
	{
		if (!reverse)
			for (uint i = mid; i < len; ++i)
				if (values[i] >= threshold)
					return i;
		if (reverse)
			for (int i = mid; i >= 0; --i)
				if (values[i] <= threshold)
					return static_cast<uint>(i);
		assert(false);
		return 0;
	}
	// search for fit range starting from the middle of all y-values
	void getFitRange(double lo, double hi, double t, double* x, double* y, double* w, uint n,
	                 uint* start, uint* stop);
	// get median of a value set
	double getMedian(double* values, uint n);
	// find plateau using linear fits
	int getPlateau(bool left, uint n, double* xvals, double* yvals, double* yweights,
	               linearFunction* myFit);
	// fit from xvals[fitStart] to xvals[fitStop] using yvals yweights
	int linearFit(uint fitStart, uint fitStop, uint n, double* xvals, double* yvals,
	              double* yweights, linearFunction* myFit);
	// find fit range and fit
	int linearFitOnAxis(bool xAxis, double FitStart, double FitStop, uint n, double* xvals,
	                    double* yvals, double* yweights, linearFunction* myFit);
};
// ****** end of SpikeyVoutCalib class declaration ********

/*ostream& operator<< (ostream & os, const SpikeyVoutCalib::linearFunction& s) {
    return os << std::endl
              << "c0        c1        cov00      cov01     cov11     chisq"
              << std::endl
              << setw(8) << fixed << setprecision(6)
              << s.offset << "  " << s.slope << "  "
              << s.cov00 << "  " << s.cov01 << "  " << s.cov11 << "  " << s.chisq
              << s.start << "  " << s.stop
              << std::endl;
}*/


} // end of namespace spikey2
