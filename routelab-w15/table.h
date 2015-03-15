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


// Each entry is the shortest path to the destination node that's
// not a direct neighbor.
struct Entry {
	unsigned dest_node; // end goal
	unsigned next_node; // node to immediately forward to 
	double cost;
	ostream & Print(ostream &os) const;

	Entry(unsigned dest, unsigned next, double c);
};

class Table {
private:
	deque<Entry> contents;
public:
	deque<Entry> GetEntrys();
	deque<Entry> GetDestinationEntry(unsigned dest);
	Entry* GetEntry(unsigned dest);
	void EditEntry(unsigned dest, Entry entry);
	ostream & Print(ostream &os) const;
};
#endif

inline ostream & operator<<(ostream &os, const Table &t) { return t.Print(os);}

#endif