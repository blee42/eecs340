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

#include <deque>

struct Entry {
	unsigned src_node;
	unsigned dest_node;
	double cost;
	ostream & Print(ostream &os) const;
	Entry(unsigned src, unsigned dest, double c);
};

struct DistanceEntry {
	double cost;
	unsigned predecessor;
	unsigned dest;
	ostream & Print(ostream &os) const;
	
	DistanceEntry(double c, unsigned p, unsigned d);
};

inline ostream & operator<<(ostream &os, const Entry &e) { return e.Print(os);}

class Table {
private:
	deque<Entry> contents;
public:
	deque<Entry> GetContents();
	void SetContents(deque<Entry> new_contents);
	Entry* GetEntry(unsigned src, unsigned dest);
	void EditEntry(unsigned src, unsigned dest, Entry new_entry);
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

inline ostream & operator<<(ostream &os, const Entry &e) { return e.Print(os);}

class Table {
private:
	deque<Entry> contents;
public:
	deque<Entry> GetEntrys();
	deque<Entry>::iterator GetDestinationEntry(unsigned);
	Entry* GetEntry(unsigned);
	void EditEntry(unsigned, Entry);
	ostream & Print(ostream &os) const;
};
#endif

inline ostream & operator << (ostream &os, const Table &t) { return t.Print(os);}

#endif
