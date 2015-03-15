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


/*
	Each row is for getting TO a destination. Each column is
	the via path.

	For some node A:
	deque		via B			via C 			via D
	  |		RowLL (to B) -> RowLL (to B) -> RowLL (to B)
	  |		RowLL (to C) -> RowLL (to C) -> RowLL (to C)
	  v     RowLL (to D) -> RowLL (to D) -> RowLL (to D)
*/
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
	deque<RowLL> GetRows();
	deque<RowLL> GetDestinationRow(unsigned dest);
	void AddRowEntry(unsigned dest, RowLL entry);
	ostream & Print(ostream &os) const;
};
#endif

inline ostream & operator<<(ostream &os, const Table &t) { return t.Print(os);}

#endif
