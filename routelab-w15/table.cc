#include "table.h"

#if defined(GENERIC)
ostream & Table::Print(ostream &os) const
{
  // WRITE THIS
  os << "Table()";
  return os;
}
#endif

#if defined(LINKSTATE)

#endif

#if defined(DISTANCEVECTOR)

RowLL::RowLL(unsigned dest, unsigned next, double c) :
	dest_node(dest), next_entry(next), cost(c)
{}

ostream & RowLL::Print(ostream &os) const
{
	os << "Row(dest=" << dest_node << ", next=" << next_entry << ", cost=" << cost << ")";
	return os;
}

#endif
