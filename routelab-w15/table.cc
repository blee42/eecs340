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

deque<RowLL> Table::GetRows() {
    return contents;
}

deque<RowLL>::iterator Table::GetDestinationRow(unsigned dest) {
    for(deque<RowLL>::iterator entry = contents.begin(); entry != contents.end(); entry++){
      if(entry->dest_node == dest){
        return entry;
      } 
    }
    return contents.end();
}

void Table::AddRowEntry(unsigned dest, RowLL entry) {
    deque<RowLL>::iterator dest_row = GetDestinationRow(dest);
}

#endif
