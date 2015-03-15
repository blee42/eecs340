#ifndef _table
#define _table


#include <iostream>

using namespace std;

#if defined(GENERIC)
class Table {
	// Students should write this class

public:
	ostream & Print(ostream &os) const;
};
#endif


#if defined(LINKSTATE)
class Table {
	// Students should write this class
public:
	ostream & Print(ostream &os) const;
};
#endif

#if defined(DISTANCEVECTOR)

#include <deque>

struct RowLL {
private:
	unsigned dest_node;
	unsigned next_entry;
	double cost;
	ostream & Print(ostream &os) const;

	RowLL(unsigned dest, unsigned next, double c);
};

class Table {
private:
	deque<RowLL> contents;
public:
	deque<RowLL> 
	ostream & Print(ostream &os) const;
};
#endif

inline ostream & operator<<(ostream &os, const Table &t) { return t.Print(os);}

#endif
