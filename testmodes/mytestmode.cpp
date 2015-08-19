// example file for a custom testmode implementation
#include "common.h"
#include "testmode.h"

using namespace spikey2;

Lister<Testmode> tmlist;

// this line is deprecated (see singelton class Logger)
Debug dbg(15); // only fatal outputs (dbg(0))

class MyClient : public Testmode
{
public:
	MyClient() { dbg(15) << "MyClient was created." << endl; };
	bool test()
	{
		dbg(0) << "tested" << endl;
		return true;
	};
};

template <class T>
class MyTM_Listee : public Listee<T>
{
public:
	MyTM_Listee() : Listee<T>(tmlist, string("MyTestmode"), string("example testmode")){};
	T* create() { return new MyClient(); };
};
MyTM_Listee<Testmode> My_Listee;

main()
{
	cout << "hello" << endl;
	Testmode* t;

	t = tmlist.create(string("muell"));
	t = tmlist.create(string("MyTestmode"));

	t->test();
}
