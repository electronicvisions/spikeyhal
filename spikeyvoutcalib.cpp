#include "common.h"
#include <numeric>
#include <limits>
#include <iostream>
#include <algorithm>

// communication
#include "idata.h"
#include "sncomm.h"

// hardware constants
#include "sc_sctrl.h"
#include "sc_pbmem.h"
// chip
#include "spikenet.h"
#include "ctrlif.h"
#include "synapse_control.h" //synapse control class
#include "pram_control.h"    //parameter ram etc
#include "spikeyconfig.h"
#include "spikeyvoutcalib.h"
#include "spikey.h"
#include "spikeycalibratable.h"

// xml
#include <Qt/qdom.h>
#include <Qt/qfile.h>

// least squares fitting needs gnu scientific library
#include <gsl/gsl_fit.h>

static log4cxx::LoggerPtr logger = log4cxx::Logger::getLogger("HAL.VOC");

using namespace spikey2;

//*********SpikeyVoutCalib Constructor******

SpikeyVoutCalib::SpikeyVoutCalib(SpikeyCalibratable* myS, int nr, string calibfile)
{
	spikeyNr = nr;
	mySpikey = myS;
	mycfg.reset(new SpikeyConfig(myS->hw_const));
	corSlope.resize(SpikeyConfig::num_blocks * mySpikey->hw_const->ar_numvouts(), 0);
	corOff.resize(SpikeyConfig::num_blocks * mySpikey->hw_const->ar_numvouts(), 0);
	validMin.resize(SpikeyConfig::num_blocks * mySpikey->hw_const->ar_numvouts(), 0);
	validMax.resize(SpikeyConfig::num_blocks * mySpikey->hw_const->ar_numvouts(), 0);
	for (uint b = 0; b < SpikeyConfig::num_blocks; ++b) {
		for (uint n = 0; n < mySpikey->hw_const->ar_numvouts(); ++n) {
			// default values
			corSlope[b * mySpikey->hw_const->ar_numvouts() + n] = 1.0;
			corOff[b * mySpikey->hw_const->ar_numvouts() + n] = 0.0;
			if (mySpikey->hw_const->revision() < 4) {
				validMin[b * mySpikey->hw_const->ar_numvouts() + n] = 0.7;
				validMax[b * mySpikey->hw_const->ar_numvouts() + n] = 1.4;
			} else {
				validMin[b * mySpikey->hw_const->ar_numvouts() + n] = 0.1;
				validMax[b * mySpikey->hw_const->ar_numvouts() + n] = 1.6;
			}
		}
	}
	if (readCalib(calibfile)) {
		LOG4CXX_DEBUG(logger, "readVoutCalib with data from file " << calibfile << " successful");
	} else {
		LOG4CXX_WARN(logger, "error during readVoutCalib with data from file " << calibfile);
	}
}

//******** SpikeyVoutCalib methods *********


// search calibration values in XML data
bool SpikeyVoutCalib::calibSearchXML(QDomElement d, int spikeyNr)
{
	bool okvec;
	okvec = true;
	QDomNode n = d.firstChild();
	while (!n.isNull()) {
		QDomElement e = n.toElement(); // try to convert the node to an element.
		if (!e.isNull()) {             // the node really is an element.
			if (e.tagName() == "spikey") {
				QDomAttr a = e.attributeNode("nr"); // checking spikey nr
				QString str(a.value()); // all xml variables are strings
				if (str.toInt(&okvec, 10) == spikeyNr) { // typecast string to int
					if (!okvec) {
						LOG4CXX_ERROR(logger, "xml parsing error, wrong values in xml file ?");
						return false;
					}
					return calibSearchXML(e, spikeyNr); // recursive functioncall, with new
					                                    // startnode
				}
			}
			if ((e.tagName() == "slope")) {
				QString backstr(e.text()); // transform xml string to Qstring
				QString singlestr;
				for (uint b = 0; b < SpikeyConfig::num_blocks; ++b)
					for (uint n = 0; n < mySpikey->hw_const->ar_numvouts(); ++n) {
						singlestr = backstr.section(';', b * mySpikey->hw_const->ar_numvouts() + n,
						                            b * mySpikey->hw_const->ar_numvouts() +
						                                n); // returns sections of backstr,
						                                    // seperated by ';'
						corSlope[b * mySpikey->hw_const->ar_numvouts() + n] =
						    singlestr.toFloat(&okvec); // typecast string to float
						if (!okvec) {
							LOG4CXX_ERROR(logger, "xml parsing error, wrong values in xml file ?");
							return false;
						}
					}
			}
			if ((e.tagName() == "offset")) {
				QString backstr(e.text()); // transform xml string to Qstring
				QString singlestr;
				for (uint b = 0; b < SpikeyConfig::num_blocks; ++b)
					for (uint n = 0; n < mySpikey->hw_const->ar_numvouts(); ++n) {
						singlestr = backstr.section(';', b * mySpikey->hw_const->ar_numvouts() + n,
						                            b * mySpikey->hw_const->ar_numvouts() +
						                                n); // returns sections of backstr,
						                                    // seperated by ';'
						corOff[b * mySpikey->hw_const->ar_numvouts() + n] =
						    singlestr.toFloat(&okvec); // typecast string to float
						if (!okvec) {
							LOG4CXX_ERROR(logger, "xml parsing error, wrong values in xml file ?");
							return false;
						}
					}
			}
			if ((e.tagName() == "validMin")) {
				QString backstr(e.text()); // transform xml string to Qstring
				QString singlestr;
				for (uint b = 0; b < SpikeyConfig::num_blocks; ++b)
					for (uint n = 0; n < mySpikey->hw_const->ar_numvouts(); ++n) {
						singlestr = backstr.section(';', b * mySpikey->hw_const->ar_numvouts() + n,
						                            b * mySpikey->hw_const->ar_numvouts() +
						                                n); // returns sections of backstr,
						                                    // seperated by ';'
						validMin[b * mySpikey->hw_const->ar_numvouts() + n] =
						    singlestr.toFloat(&okvec); // typecast string to float
						if (!okvec) {
							LOG4CXX_ERROR(logger, "xml parsing error, wrong values in xml file ?");
							return false;
						}
					}
			}
			if ((e.tagName() == "validMax")) {
				QString backstr(e.text()); // transform xml string to Qstring
				QString singlestr;
				for (uint b = 0; b < SpikeyConfig::num_blocks; ++b)
					for (uint n = 0; n < mySpikey->hw_const->ar_numvouts(); ++n) {
						singlestr = backstr.section(';', b * mySpikey->hw_const->ar_numvouts() + n,
						                            b * mySpikey->hw_const->ar_numvouts() +
						                                n); // returns sections of backstr,
						                                    // seperated by ';'
						validMax[b * mySpikey->hw_const->ar_numvouts() + n] =
						    singlestr.toFloat(&okvec); // typecast string to float
						if (!okvec) {
							LOG4CXX_ERROR(logger, "xml parsing error, wrong values in xml file ?");
							return false;
						}
					}
				return true;
			}
		}
		n = n.nextSibling();
	}
	LOG4CXX_WARN(logger, "spikey not found in list, using standard values !");
	for (uint b = 0; b < SpikeyConfig::num_blocks; ++b)
		for (uint n = 0; n < mySpikey->hw_const->ar_numvouts(); ++n) {
			corSlope[b * mySpikey->hw_const->ar_numvouts() + n] = 1;
			corOff[b * mySpikey->hw_const->ar_numvouts() + n] = 0;
		}
	return false;
}

// opens XML data, searches and sets values for vout calibration
bool SpikeyVoutCalib::readCalib(string calibFile)
{
	QDomDocument doc("spikeyvoutcalib");
	QFile file;
	file.setFileName(calibFile.c_str());
	if (!file.open(QIODevice::ReadOnly)) { // trying to open file
		LOG4CXX_WARN(logger, "could not open file!");
		return 0;
	}
	if (!doc.setContent(&file)) { // trying to parse xml file
		file.close();
		LOG4CXX_WARN(logger, "could not set content!");
		return 0;
	}
	file.close();
	QDomElement docElem = doc.documentElement();
	return calibSearchXML(docElem, spikeyNr);
}

bool SpikeyVoutCalib::writeCalib(string calibFile)
{
	bool okvec;
	// opening and reading file
	QDomDocument doc("spikeyvoutcalib");
	QFile file;
	file.setFileName(calibFile.c_str());
	if (!file.open(QIODevice::ReadOnly)) { // trying to open file
		LOG4CXX_WARN(logger, "could not open file!");
		return false;
	}
	if (!doc.setContent(&file)) { // trying to parse xml file
		file.close();
		LOG4CXX_WARN(logger, "could not set content!");
		return false;
	}
	QDomElement d = doc.documentElement();
	QDomNode n = d.firstChild();
	QDomElement newSpikey = doc.createElement("spikey"); // creating new spikey
	newSpikey.setAttribute("nr", spikeyNr);
	int found = 0;
	while (!n.isNull()) { // search for correct place to insert new spikey or change existing
		QDomElement e = n.toElement(); // try to convert the node to an element.
		if (!e.isNull()) {             // the node really is an element.
			if (e.tagName() == "spikey") {
				QDomAttr a = e.attributeNode("nr"); // checking spikey nr
				QString str(a.value()); // all xml variables are strings
				if (str.toInt(&okvec, 10) > spikeyNr) { // new spikey nonpresent, higher spikey
				                                        // found
					if (!okvec) {
						LOG4CXX_ERROR(logger, "xml parsing error, wrong values in xml file ?");
						return false;
					}
					d.insertBefore(newSpikey, n); // inserting in existing tree
					found = 1;
					break;
				} else if (str.toInt(&okvec, 10) == spikeyNr) { // typecast string to int
					if (!okvec) {
						LOG4CXX_ERROR(logger, "xml parsing error, wrong values in xml file ?");
						return false;
					}
					found = 1;
					d.replaceChild(newSpikey, n); // spikey already existent, replace
					break;
				}
			}
		}
		n = n.nextSibling();
	}
	if (found == 0) { // newspikey not in tree and highest number, appending
		d.appendChild(newSpikey);
	}
	// building new spikey tree
	QString corValue;
	QString app;

	// write calibration slope
	for (uint b = 0; b < SpikeyConfig::num_blocks; ++b)
		for (uint n = 0; n < mySpikey->hw_const->ar_numvouts(); ++n) {
			app = app.setNum(corSlope[b * mySpikey->hw_const->ar_numvouts() + n], 'g', 4);
			corValue += app;
			corValue += ';';
		}
	corValue.remove(corValue.length(), 1);

	QDomElement newSlopeNode = doc.createElement("slope");
	newSpikey.appendChild(newSlopeNode);
	QDomText newSlopeValue = doc.createTextNode(corValue);
	newSlopeNode.appendChild(newSlopeValue);

	// write calibration offset
	corValue.remove(0, corValue.length());
	for (uint b = 0; b < SpikeyConfig::num_blocks; ++b)
		for (uint n = 0; n < mySpikey->hw_const->ar_numvouts(); ++n) {
			app = app.setNum(corOff[b * mySpikey->hw_const->ar_numvouts() + n], 'g', 4);
			corValue += app;
			corValue += ';';
		}
	corValue.remove(corValue.length(), 1);

	QDomElement newOffNode = doc.createElement("offset");
	newSpikey.appendChild(newOffNode);
	QDomText newOffValue = doc.createTextNode(corValue);
	newOffNode.appendChild(newOffValue);

	// write valid min vout values
	corValue.remove(0, corValue.length());
	for (uint b = 0; b < SpikeyConfig::num_blocks; ++b)
		for (uint n = 0; n < mySpikey->hw_const->ar_numvouts(); ++n) {
			app = app.setNum(validMin[b * mySpikey->hw_const->ar_numvouts() + n], 'g', 4);
			corValue += app;
			corValue += ';';
		}
	corValue.remove(corValue.length(), 1);

	QDomElement newValidMinNode = doc.createElement("validMin");
	newSpikey.appendChild(newValidMinNode);
	QDomText newValidMinValue = doc.createTextNode(corValue);
	newValidMinNode.appendChild(newValidMinValue);

	// write valid max vout values
	corValue.remove(0, corValue.length());
	for (uint b = 0; b < SpikeyConfig::num_blocks; ++b)
		for (uint n = 0; n < mySpikey->hw_const->ar_numvouts(); ++n) {
			app = app.setNum(validMax[b * mySpikey->hw_const->ar_numvouts() + n], 'g', 4);
			corValue += app;
			corValue += ';';
		}
	corValue.remove(corValue.length(), 1);

	QDomElement newValidMaxNode = doc.createElement("validMax");
	newSpikey.appendChild(newValidMaxNode);
	QDomText newValidMaxValue = doc.createTextNode(corValue);
	newValidMaxNode.appendChild(newValidMaxValue);

	file.close();

	// re-open file for writing! before writing was to same file handler as reading,
	// this produced fatal fragments at end of file sometimes! (DB)
	QFile file2;
	file2.setFileName(calibFile.c_str());
	if (!file2.open(QIODevice::WriteOnly)) { // trying to open file
		LOG4CXX_WARN(logger, "could not open file!");
		return false;
	}

	// writing xml tree to file
	QString docString = doc.toString();
	file2.seek(0);
	file2.write(docString.toLatin1()); // docString.length());
	file2.close();
	return true;
}


//! calibration function for programmable vout voltages
bool SpikeyVoutCalib::autoCalibRobust(string calibFile, string filenamePlot,
                                      boost::shared_ptr<SpikeyConfig> someSpikeyConfig)
{
	// resetting vout calibration values
	std::cout << "resetting vout calibration values" << std::endl;
	assert(corSlope.size() == SpikeyConfig::num_blocks * mySpikey->hw_const->ar_numvouts());
	for (uint b = 0; b < SpikeyConfig::num_blocks; ++b)
		for (uint n = 0; n < mySpikey->hw_const->ar_numvouts(); ++n) {
			corSlope[b * mySpikey->hw_const->ar_numvouts() + n] = 1;
			corOff[b * mySpikey->hw_const->ar_numvouts() + n] = 0;
			validMin[b * mySpikey->hw_const->ar_numvouts() + n] = 0.0;
			validMax[b * mySpikey->hw_const->ar_numvouts() + n] = 2.5;
		}

	assert(someSpikeyConfig != NULL);
	mySpikeyConfig = someSpikeyConfig;

	// initialize some variables (like irefdac, etc.)
	mySpikey->config(mySpikeyConfig);

	// for every vout
	uint measurePoints = 33; // 2**n + 1 (last point)

	// workaround for spikey4: skip left block completely!
	uint firstBlock = 0;
	if (mySpikey->hw_const->revision() == 4)
		firstBlock = 1;


	// write measurement points to file for plotting
	ofstream filePlot;
	if (filenamePlot != "") {
		filePlot.open(filenamePlot);
		if (!filePlot.is_open()) {
			LOG4CXX_ERROR(logger, "Could not write to vout calib plot file: " << filenamePlot);
		}
	}

	for (uint b = firstBlock; b < SpikeyConfig::num_blocks; ++b) {
		for (uint n = 0; n < PramControl::num_vout_adj; ++n) {
			// reset playback memory to avoid overflow
			boost::shared_ptr<SC_Mem> membus = boost::dynamic_pointer_cast<SC_Mem>(mySpikey->bus);
			membus->intClear();

			// b = 0; n = 9;
			uint vout = b * mySpikey->hw_const->ar_numvouts() + n;

			// libgsl needs old c-style, sorry :(
			double* xvals = new double[measurePoints];
			double* yvals = new double[measurePoints];
			double* yweights = new double[measurePoints];
			double oldVoutVal = mySpikeyConfig->vout[vout];

			// measure #resolution values between lowest and highest possible value
			for (uint i = 0; i < measurePoints; ++i) {
				// dac resolution 10 bits
				uint dac = 1024.0 / (measurePoints - 1) * i;
				if (i == (measurePoints - 1))
					dac = 1023;
				if (i == 0)
					dac = 8; // do not measure 0 :)
				xvals[i] = mySpikey->convDacVolt(dac);
				yvals[i] = 0.0;
				yweights[i] = 0.0;
			}


			// !!! HACK: (Statistical) outliers persist: let's measure multiple curves sequentially.
			uint avgRuns = 20;
			double* all_yvals = new double[avgRuns * measurePoints];
			double* my_yvals = new double[measurePoints];
			double* my_yweights = new double[measurePoints];

			for (uint i = 0; i < avgRuns; ++i) {
				// the measurement
				readVoutValues(b, n, /* avgRuns */ 10, xvals, measurePoints, my_yvals, my_yweights,
				               /* verbose */ true);
				for (uint ii = 0; ii < measurePoints; ++ii) {
					assert(i * measurePoints + ii < avgRuns * measurePoints);
					all_yvals[i * measurePoints + ii] = my_yvals[ii];
					yvals[ii] += my_yvals[ii];
					std::cout << "." << std::flush;
				}
			}
			std::cout << "\n";

			std::cout << std::endl
			          << std::endl
			          << "vout   dac    volt = mean +/- sigma  idx" << std::endl;
			for (uint ii = 0; ii < measurePoints; ++ii) {
				yvals[ii] /= avgRuns;
				for (uint i = 0; i < avgRuns; ++i) {
					assert(i * measurePoints + ii < avgRuns * measurePoints);
					double v = (all_yvals[i * measurePoints + ii] - yvals[ii]);
					yweights[ii] += v * v;
				}
				if (!yweights[ii] > 0.0)
					yweights[ii] = sqrt(yweights[ii] / avgRuns);

				std::cout << setprecision(4) << fixed << setw(4)
				          << b * mySpikey->hw_const->ar_numvouts() + n << "  " << setw(4)
				          << mySpikey->convVoltDac(xvals[ii],
				                                   b * mySpikey->hw_const->ar_numvouts() + n)
				          << "  " << setw(4) << xvals[ii] << "  " << yvals[ii] << setprecision(5)
				          << "  " << setw(7) << yweights[ii] << "  " << setw(3) << ii << std::endl;

				if (filenamePlot != "" and filePlot.is_open()) {
					filePlot << setprecision(4) << fixed << setw(4)
					         << b * mySpikey->hw_const->ar_numvouts() + n << "  " << setw(4)
					         << mySpikey->convVoltDac(
					                xvals[ii], b * mySpikey->hw_const->ar_numvouts() + n) << "  "
					         << setw(4) << xvals[ii] << "  " << yvals[ii] << setprecision(5) << "  "
					         << setw(7) << yweights[ii] << "  " << setw(3) << ii << std::endl;
				}
			}
			// !!! END OF HACK


			// invert stddev (=> weighted points)
			for (uint i = 0; i < measurePoints; ++i)
				yweights[i] = 1.0 / (1.0 + yweights[i]);

			mySpikeyConfig->vout[vout] = oldVoutVal; // avoid warnings :)

			// find left (lower) and right (upper) plateau
			linearFunction lower, upper;
			getPlateau(/* left */ true, measurePoints, xvals, yvals, yweights, &lower);
			std::cout << "f(x) = " << lower.offset << " + " << lower.slope << "*x" << std::endl;
			getPlateau(/* left */ false, measurePoints, xvals, yvals, yweights, &upper);
			std::cout << "g(x) = " << upper.offset << " + " << upper.slope << "*x" << std::endl;

			// find fit boundaries
			double lowerMedian, upperMedian, dynamicRange, tolerance;
			lowerMedian = getMedian(yvals + lower.startIdx, lower.stopIdx - lower.startIdx);
			upperMedian = getMedian(yvals + upper.startIdx, upper.stopIdx - upper.startIdx);
			dynamicRange = upperMedian - lowerMedian;
			tolerance = 0.1 * dynamicRange;

			unsigned int start, stop;
			getFitRange(lowerMedian, upperMedian, tolerance, xvals, yvals, yweights, measurePoints,
			            &start, &stop);

			// calibration fit
			linearFunction calibFit;
			linearFit(start, stop, measurePoints, xvals, yvals, yweights, &calibFit);
			std::cout << "Calibration Fit (" << calibFit.start << "..." << calibFit.stop << ", "
			          << calibFit.startIdx << "..." << calibFit.stopIdx << "):" << std::endl;
			std::cout << "h(x) = " << calibFit.offset << " + " << calibFit.slope << "*x"
			          << std::endl;

			// some calibration checks based on emperical data
			bool die = false;
			double maxUpper, minUpper, maxLower, minLower, maxSlope, minSlope, maxOffset, minOffset,
			    maxPlateauSlope;
			if (mySpikey->hw_const->revision() < 4) {
				maxUpper = 1.80, minUpper = 1.30;    // upper plateau
				maxLower = 1.00, minLower = 0.35;    // lower plateau
				maxSlope = 1.25, minSlope = 0.75;    // calibration fit slope
				maxOffset = 0.50, minOffset = -0.10; // calibration fit offset
				maxPlateauSlope = 0.08;              // plateau needs to be flat
			} else {
				maxUpper = 2.00, minUpper = 1.60;    // upper plateau
				maxLower = 0.10, minLower = 0.00;    // lower plateau
				maxSlope = 1.25, minSlope = 0.75;    // calibration fit slope
				maxOffset = 0.10, minOffset = -0.10; // calibration fit offset
				maxPlateauSlope = 0.08;              // plateau needs to be flat
			}

			if (mySpikey->hw_const->revision() < 4) {
				if (!(abs(lower.slope) <= maxPlateauSlope)) {
					std::cout << "lower plateau isn't flat enough: slope = " << lower.slope
					          << std::endl;
					die = true;
				}
				if (!(abs(upper.slope) <= maxPlateauSlope)) {
					std::cout << "upper plateau isn't flat enough: slope = " << upper.slope
					          << std::endl;
					die = true;
				}
			}
			if (!(minUpper <= upper.offset && upper.offset <= maxUpper)) {
				std::cout << "ERROR: upper plateau is not within (" << minUpper << "..." << maxUpper
				          << "V): " << upper.offset << std::endl;
				die = true;
			}
			if (!(minLower <= lower.offset && lower.offset <= maxLower)) {
				std::cout << "ERROR: lower plateau is not within (" << minLower << "..." << maxLower
				          << "V): " << lower.offset << std::endl;
				die = true;
			}
			if (!(minSlope <= calibFit.slope && maxSlope >= calibFit.slope)) {
				std::cout << "ERROR: slope is not in range (" << minSlope << "..." << maxSlope
				          << "): " << calibFit.slope << std::endl;
				die = true;
			}
			if (!(minOffset <= calibFit.offset && maxOffset >= calibFit.offset)) {
				std::cout << "ERROR: offset is not in range (" << minOffset << "..." << maxOffset
				          << "): " << calibFit.offset << std::endl;
				die = true;
			}

			if (die)
				assert(false);

			// ok, save calibration to vector (later to file ;))
			assert(corSlope.size() == SpikeyConfig::num_blocks * mySpikey->hw_const->ar_numvouts());
			corOff[vout] = calibFit.offset;
			corSlope[vout] = calibFit.slope;
			validMin[vout] = lower.offset + 0.05 * dynamicRange;
			validMax[vout] = upper.offset - 0.05 * dynamicRange;
		}
	}
	if (filenamePlot != "" and filePlot.is_open()) {
		filePlot.close();
	}

	// now, let's check the calibration fits
	checkVoutCalib();

	return SpikeyVoutCalib::writeCalib(calibFile);
}

// helpers for autoCalibRobust
int SpikeyVoutCalib::getPlateau(bool left, uint n, double* xvals, double* yvals, double* yweights,
                                linearFunction* myFit)
{
	linearFunction temp;
	int error = 0;
	temp.slope = std::numeric_limits<double>::infinity();

	assert(n >= 9);
	uint fitTrials = n / 2 - 2;
	if (left) { // left plateau
		std::cout << "Left plateau: " << std::endl;
		for (uint i = 3; i < fitTrials; ++i) {
			// discard first (0) value in array
			int errorTemp = linearFit(1, i, n, xvals, yvals, yweights, myFit);
			if (abs(myFit->slope) < abs(temp.slope)) {
				// std::cout << "2DEL: smaller " << myFit->offset << "  " << myFit->slope <<
				// std::endl;
				temp = (*myFit); // save new value
				error = errorTemp;
			}
		}
	} else { // right plateau
		std::cout << "Right plateau: " << std::endl;
		for (uint i = n - 1 - 2; i > n - 1 - 2 - fitTrials; --i) {
			int errorTemp = linearFit(i, n - 1, n, xvals, yvals, yweights, myFit);
			if (abs(myFit->slope) < abs(temp.slope)) {
				// std::cout << "2DEL: smaller " << myFit->offset << "  " << myFit->slope <<
				// std::endl;
				temp = (*myFit); // save new value
				error = errorTemp;
			}
		}
	}

	*myFit = temp; // smallest slope
	// std::cout << "smallest slope: " << myFit->slope << std::endl;
	return error;
}

int SpikeyVoutCalib::linearFit(uint fitStart, uint fitStop, uint n, double* xvals, double* yvals,
                               double* yweights, linearFunction* myFit)
{
	static_cast<void>(n);
	// std::cout << "fitStart " << fitStart << "  "
	//          << "fitStop  " << fitStop
	//          << std::endl;
	double c0, c1, cov00, cov01, cov11, chisq;
	int error = gsl_fit_wlinear(xvals + fitStart, 1, yweights + fitStart, 1, yvals + fitStart, 1,
	                            fitStop - fitStart + 1, &c0, &c1, &cov00, &cov01, &cov11, &chisq);
	assert(error == 0); // some error checking?
	myFit->offset = c0;
	myFit->slope = c1;
	myFit->cov00 = cov00;
	myFit->cov01 = cov01;
	myFit->cov11 = cov11;
	myFit->chisq = chisq;
	myFit->start = xvals[fitStart];
	myFit->stop = xvals[fitStop];
	myFit->startIdx = fitStart;
	myFit->stopIdx = fitStop;
	// std::cout << (*myFit);
	// std::cout << "linearFit: " << myFit->offset << "  " << myFit->slope << std::endl;

	return error; // this should be zero
}

int SpikeyVoutCalib::linearFitOnAxis(bool xAxis, double fitStart, double fitStop, uint n,
                                     double* xvals, double* yvals, double* yweights,
                                     linearFunction* myFit)
{
	// std::cout << "fitStart " << fitStart << "  "
	//          << "fitStop  " << fitStop << "  "
	//          << "n  " << n
	//          << std::endl;

	// fitRange on x-axis (or y-axis)?
	double* fitRangeAxis;
	if (xAxis)
		fitRangeAxis = xvals;
	else
		fitRangeAxis = yvals;

	// find fit range
	int leftIdx = -1, rightIdx = -1;
	bool left = true;
	for (uint i = 0; i < n; ++i) {
		if (left)                              // left boundary search
			if (fitRangeAxis[i] >= fitStart) { // left boundary
				leftIdx = i;
				// std::cout << "leftIdx: " << leftIdx
				//          << " fitRangeAxis[" << i-1 << "] = " << fitRangeAxis[i-1]
				//          << " fitRangeAxis[" << i << "] = " << fitRangeAxis[i]
				//          << " fitStart = " << fitStart
				//          << std::endl;
				left = false;
			}
		if ((fitRangeAxis[i] >= fitStop) || ((i + 1) == n)) {
			rightIdx = i;
			// std::cout << "rightIdx: " << rightIdx
			//	      << " fitRangeAxis[" << i-1 << "] = " << fitRangeAxis[i-1]
			//        << " fitRangeAxis[" << i << "] = " << fitRangeAxis[i]
			//	      << " fitStop = " << fitStop
			//	      << " i = " << i
			//	      << " n = " << n
			//        << std::endl;
			break;
		}
	}

	if (leftIdx == -1 || rightIdx == -1) {
		std::cout << "Didn't find borders. This is a BUG!"
		          << "leftIdx " << leftIdx << "  "
		          << "rightIdx " << rightIdx << "  "
		          << "fitRange was " << fitStart << "..." << fitStop << std::endl;
		assert(false);
	}

	int error = linearFit(leftIdx, rightIdx, n, xvals, yvals, yweights, myFit);
	// std::cout << "linearFit: " << myFit->offset << "  " << myFit->slope << std::endl;

	return error;
}

void SpikeyVoutCalib::getFitRange(double low, double high, double t, double* x, double* y,
                                  double* w, uint n, uint* start, uint* stop)
{
	static_cast<void>(x);
	static_cast<void>(w);
	assert(n > 2);

	double middle = (high + low) / 2;
	double upperBound = high - t;
	double lowerBound = low + t;
	// std::cout << " lower: " << low
	//		  << " upper: " << high
	//		  << " middle: " << middle
	//		  << " tol:   " << t
	//		  << " n:   " << n
	//		  << " uB:   " << upperBound
	//		  << " lB:   " << lowerBound
	//		  << std::endl;
	double distance = std::numeric_limits<double>::infinity();
	uint middleIdx = 0;
	for (uint i = 0; i < n; ++i) {
		if (abs(y[i] - middle) < distance) {
			distance = abs(y[i] - middle);
			middleIdx = i;
		}
	}
	// std::cout << "middle idx is " << middleIdx
	//		  << " = " << y[middleIdx] << std::endl;

	// search fitrange beginning with middleIdx...
	*start = findFirst(/* reverse */ true, lowerBound, y, n, middleIdx); // left  "half"
	*stop = findFirst(/* reverse */ false, upperBound, y, n, middleIdx); // right "half"
	if ((*stop - *start) < 2) {
		std::cout << "ERROR: found fitRange " << *start << "..." << *stop << std::endl;
		assert(false);
	}

	// more robust fit
	if ((*stop - *start) > 4) {
		*start += 1;
		*stop -= 1;
	}

	// std::cout << "start: " << *start << " stop: " << *stop << std::endl;
}

double SpikeyVoutCalib::getMedian(double* values, uint n)
{
	assert(n >= 2);
	double* temp = new double[n];
	std::copy(values, values + n, temp);
	std::sort(temp, temp + n); // ascending sorting

	return temp[n / 2];
}

void SpikeyVoutCalib::readVoutValues(uint block, uint voutnr, uint avgRuns, double* targetV,
                                     uint len, double* mean, double* std, bool msgs)
{
	uint vout = block * mySpikey->hw_const->ar_numvouts() + voutnr;

	if (msgs) {
		// two newlines for gnuplot's "index"
		std::cout << std::endl
		          << std::endl
		          << "vout   dac    volt = mean +/- sigma  idx" << std::endl;
	}

	// walk through all x-values
	for (uint i = 0; i < len; ++i) {
		// prepare hardware for analog read-out
		boost::shared_ptr<AnalogReadout> analog_readout =
		    mySpikey->getAR(); // get pointer to analog readout control class
		if (!block) {
			analog_readout->clear(analog_readout->voutr, 200);
			analog_readout->set(analog_readout->voutl, voutnr, true, 200);
		} else {
			analog_readout->clear(analog_readout->voutl, 200);
			analog_readout->set(analog_readout->voutr, voutnr, true, 200);
		}

		// permute vouts & update config
		// std::random_shuffle(mySpikeyConfig->vout.begin(), mySpikeyConfig->vout.end());
		mySpikeyConfig->vout[vout] = targetV[i];
		mySpikey->config(mySpikeyConfig);

		usleep(200000); // wait for parameter update TODO: minimize!!!

		// measure vout
		std::vector<float> values;
		mean[i] = 0.0;
		// average over #avgRuns runs
		for (uint j = 0; j < avgRuns; ++j) {
			usleep(2000); // wait for uncorrelated measurements
			values.push_back(mySpikey->readAdc(SBData::ibtest));
			mean[i] += values[j];
		}
		mean[i] /= avgRuns;

		std[i] = 0.0;
		for (uint j = 0; j < avgRuns; ++j) {
			std[i] += (values[j] - mean[i]) * (values[j] - mean[i]);
		}
		std[i] /= avgRuns;
		std[i] = sqrt(std[i]);

		// TODO: Why are the first x-values wrongly converted? CHECK!
		if (msgs) {
			std::cout << setprecision(4) << fixed << setw(4)
			          << block * mySpikey->hw_const->ar_numvouts() + voutnr << "  " << setw(4)
			          << mySpikey->convVoltDac(targetV[i], vout) << "  " << setw(4) << targetV[i]
			          << "  " << mean[i] << setprecision(5) << "  " << setw(7) << std[i] << "  "
			          << setw(3) << i << std::endl;
		}
	}
}

void SpikeyVoutCalib::checkVoutCalib()
{
	std::cout << "Checking Vout calibration:" << std::endl;

	std::vector<double> chisqs;
	double chisq_mean = 0.0;

	// workaround for spikey4: skip left block completely!
	uint firstBlock = 0;
	if (mySpikey->hw_const->revision() == 4)
		firstBlock = 1;

	uint measurePoints = 33;
	for (uint block = firstBlock; block < SpikeyConfig::num_blocks; ++block) {
		for (uint voutnr = 0; voutnr < PramControl::num_vout_adj; ++voutnr) {
			uint vout = block * mySpikey->hw_const->ar_numvouts() + voutnr;

			double* xvals = new double[measurePoints];
			double* yvals = new double[measurePoints];
			double* ystds = new double[measurePoints];
			double* yweights = new double[measurePoints];

			// measure #resolution values between lowest and highest valid values
			double dynamicRange = validMax[vout] - validMin[vout];
			for (uint i = 0; i < measurePoints; ++i) {
				xvals[i] = validMin[vout] + dynamicRange / measurePoints * i;
			}

			uint avgRuns = 3;
			double oldVoutVal = mySpikeyConfig->vout[vout]; // save context
			readVoutValues(block, voutnr, avgRuns, xvals, measurePoints, yvals, ystds,
			               /* msgs */ false);
			mySpikeyConfig->vout[vout] = oldVoutVal; // restore context...

			for (uint i = 0; i < measurePoints; ++i)
				yweights[i] = 1.0 / (1.0 + ystds[i]);

			linearFunction myFit;
			linearFit(0, measurePoints, measurePoints, xvals, yvals, yweights, &myFit);

			chisqs.push_back(myFit.chisq);
			chisq_mean += myFit.chisq;
			LOG4CXX_INFO(logger, "Vout: " << std::setw(2) << vout << "  "
			                              << "Slope: " << std::setw(8) << std::setprecision(5)
			                              << myFit.slope << "  "
			                              << "Offset: " << std::setw(8) << std::setprecision(5)
			                              << myFit.offset << "  "
			                              << "Chi^2/dof = " << std::setw(8) << std::setprecision(5)
			                              << myFit.chisq / (measurePoints - 2)
			                              << ((abs(myFit.slope - 1.0) > 0.02) ? "  SLOPE!" : "")
			                              << ((abs(myFit.offset) > 0.08) ? "  OFFSET!" : ""));

			// reset playback memory to avoid overflow
			boost::shared_ptr<SC_Mem> membus(boost::dynamic_pointer_cast<SC_Mem>(mySpikey->bus));
			membus->intClear();
		}
	}
	chisq_mean /= chisqs.size();
	LOG4CXX_INFO(logger, "Chi^2 mean/d.o.f = " << chisq_mean / (measurePoints - 2));
	// TP: TODO: add assert for too large chi
}
