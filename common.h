#ifndef SPIKEY_COMMON_H
#define SPIKEY_COMMON_H

// standard library includes
#include <string>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <vector>
#include <map>
#include <list>
#include <queue>
#include <valarray>
#include <bitset>

#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <numeric>

#include <boost/shared_ptr.hpp>
#include <boost/pointer_cast.hpp>

#ifdef CONFIG_H_AVAILABLE
#include "config.h"
#endif

extern "C" {
#include "getch.h"
}
using namespace std;

// my favorite type abbreviations
typedef unsigned char ubyte;
typedef unsigned short uint16;
typedef unsigned short uint16_t;
typedef unsigned int uint;
typedef unsigned int uint32_t; // not valid in an ILP64 programming model, but LP64 is better anyway
                               // and used by Linux
#if __WORDSIZE == 64
typedef unsigned long int uint64_t;
typedef long int int64_t;
#else
typedef unsigned long long int uint64_t;
typedef long long int int64_t;
#endif


extern uint randomseed;


// helper function that outputs binary numbers
ostream& binout(ostream& in, uint64_t u, uint max);

// helper function that generates a binary mask from an msb bit definition
inline uint64_t makemask(uint msb)
{
	return (1ULL << (msb + 1)) - 1;
}
inline uint64_t mmm(uint msb)
{
	return makemask(msb);
}
inline uint64_t mmw(uint width)
{
	return makemask(width - 1);
}

// simple statistics: standard deviation and mean of vector T
template <class T>
T sdev(vector<T>& v)
{
	T mean = accumulate(v.begin(), v.end(), 0.0) / v.size();
	T sdev = 0;
	for (uint i = 0; i < v.size(); ++i)
		sdev += pow(v[i] - mean, 2);
	sdev *= 1.0 / (v.size() - 1.0);
	sdev = sqrt(sdev);
	return sdev;
}

template <class T>
T mean(vector<T>& v)
{
	return accumulate(v.begin(), v.end(), 0.0) / v.size();
}

// fir filter, res must have the same size than data and be initialized with zero
template <class T>
void fir(vector<T>& res, const vector<T>& data, const vector<T>& coeff)
{
	T norm = accumulate(coeff.begin(), coeff.end(), 0.0) / coeff.size();
	for (uint i = 0; i < data.size() - coeff.size() + 1; ++i) {
		T val = 0;
		for (uint j = 0; j < coeff.size(); ++j)
			val += data[i + j] * coeff[j];
		res[i + coeff.size() / 2 + 1] = val / norm;
	}
}

//----------------------convert T to string----------------------------------
template <typename T>
std::string to_str(T rv)
{
	std::stringstream strm;
	strm << rv;
	return strm.str();
}
//---------------------------------------------------------------------------
template <typename T>
T str_to(const std::string& str)
{
	std::stringstream strm(str);
	T rv;
	strm >> rv;
	return rv;
}
//---------------------------------------------------------------------------

#endif /* SPIKEY_COMMON_H */
