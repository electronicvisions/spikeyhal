#include "common.h"
// library includes
ostream& binout(ostream& in, uint64_t u, uint max)
{
	for (int i = max - 1; i >= 0; --i)
		in << ((u & (1ULL << i)) ? "1" : "0");
	return in;
}
