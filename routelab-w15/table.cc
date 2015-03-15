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

Entry::Entry(unsigned dest, unsigned next, double c) :
	dest_node(dest), next_node(next), cost(c)
{}

ostream & Entry::Print(ostream &os) const
{
	os << "Entry(dest=" << dest_node << ", next=" << next_node << ", cost=" << cost << ")";
	return os;
}

deque<Entry> Table::GetRows() {
    return contents;
}

deque<Entry>::iterator Table::GetDestinationEntry(unsigned dest) {
    for(deque<Entry>::iterator entry = contents.begin(); entry != contents.end(); entry++){
      if(entry->dest_node == dest){
        return entry;
      } 
    }
    return contents.end();
}

Entry Table::GetEntry(unsigned dest) {
  deque<Entry>::iterator entry = GetDestinationEntry(dest);
  if (entry != contents.end()) {
    return new Entry(entry->dest_node, entry->next_node, entry->cost);
  } 
  else {
    return NULL;
  }
}

void Table::EditEntry(unsigned dest, Entry new_entry) {
    deque<Entry>::iterator entry = GetDestinationEntry(dest);
    if (entry != contents.end()) {
      entry->dest_node = new_entry->dest_node;
      entry->next_node = new_entry->next_node;
      entry->cost = new_entry->cost;
    }
    else {
      contents.push_back(new_entry);
    }
}

#endif