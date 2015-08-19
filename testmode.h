#include <algorithm>
#include <stdexcept>
#include "logger.h"

using namespace spikey2;

// common testmode interface
// class Spikenet;
// class SpikenetComm;

class Testmode
{
protected:
	// No logger here, because this logger would
	// be initialized before the one in createtb.cpp's main
	// But we want to initialize it there with a certain loglevel
	// Logger& dbg;
public:
	// one element for one spikey in a daisy-chain at one nathan
	vector<boost::shared_ptr<Spikenet>> chip;
	boost::shared_ptr<SpikenetComm> bus;

	// each element holds the communication interface for one nathan
	vector<boost::shared_ptr<SpikenetComm>> busv;

	// one element for one spikey on one nathan
	vector<boost::shared_ptr<Spikenet>> chipv;

	// hold nathan numbers for busv and chipv indices
	vector<unsigned> nathanNums;

	std::size_t nathanIndex(unsigned nathan_num) const
	{
		vector<unsigned>::const_iterator i;
		i = find(nathanNums.begin(), nathanNums.end(), nathan_num);
		if (i == nathanNums.end())
			throw std::runtime_error("Could not find index for given nathan number");

		return i - nathanNums.begin();
	}

	Testmode(){}; // : dbg(Logger::instance()) {};
	virtual ~Testmode(){};
	virtual bool test() = 0; // performs test, return true if passed
};

/* Listee is a registration concept to avoid recompilation after changing or adding new testmodes
each testmode is derived from the class testmode and must implement at least the
bool test() function that does the testing
the system initializes the vector<Spikenet *> and SpikenetComm* elements to give the testmode access
to all data
it also has access to the global debug output dbg and to the static Listee functions

to connect the testmode with the main programm, the Testmoder Listee registers itself in a linked
list of Listees
each Listee contains the name and the description of the testmode together with a create function
that
calls the constructor of its assigned Testmode. Therefore it acts as a factory for its accompanying
Testmode class.
This allows runtime-creation of a testmode by specifying its name.
*/

// new Listee lists itself

template <class T>
class Listee
{ // general Listee type
	string myname; // name by which class ist listed
	string mycomment; // mandatory explanation of Listee's purpose
	Listee<T>* next;

public:
	static Listee<T>* first;
	Listee(string nname, string ncomment)
	{
		myname = nname;
		mycomment = ncomment;
		Listee<T>* p = first;
		if (p == NULL) {
			first = this;
		} else {
			while (p->next != NULL)
				p = p->next;
			p->next = this;
		}
		next = NULL;
	};
	virtual ~Listee(){};
	const string& name(void) { return myname; };
	const string& comment(void) { return mycomment; };
	virtual T* create() = 0; // calls constructor of Listee user
	Listee<T>* getNext() { return next; };
	static Listee<T>* getListee(string name)
	{
		Listee<T>* p = first;
		while (p != NULL) {
			if (p->name() == name)
				return p;
			p = p->next;
		}
		return NULL;
	};

	static T* createNew(string name)
	{
		Listee<T>* l;
		if ((l = getListee(name)))
			return l->create();
		else
			return NULL;
	};
};

/*example
class MyListee:Listee<MyTypeBase>{
    MyListee():Listee(string("name"),string("comment")){}:
    MyTypeBase * create(){return new MyTypeDerived();};
} mylistee;
*/
//}  // end of namespace spikey2
