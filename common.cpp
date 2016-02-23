#include "common.h"

ostream& binout(ostream& in, uint64_t u, uint max)
{
	for (int i = max - 1; i >= 0; --i)
		in << ((u & (1ULL << i)) ? "1" : "0");
	return in;
}

void bin_bits(uint value, std::vector<uint>& binned_bits)
{
	if(value >= (1 << (binned_bits.size() + 1))) {
		  throw std::runtime_error("value too large for given number of bits");
	}
	for(uint i = 0; i < binned_bits.size(); i++) {
		binned_bits[i] += bool((1 << i) & value);
	}
}

int number_set_bits(uint value)
{
	uint counter = 0;
	while(value) {
		counter++;
		value = (value - 1) & value;
	}
	return counter;
}
