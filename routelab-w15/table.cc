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

Entry::Entry(unsigned src, unsigned dest, double c) :
  src_node(src), dest_node(dest), cost(c)
{}

ostream & Entry::Print(ostream &os) const
{
  os << "Entry(src=" << src_node << ", dest=" << dest_node << ", cost=" << cost << ")";
  return os;
}

Entry* Table::GetEntry(unsigned src, unsigned dest)
{
  for(deque<Entry>::iterator entry = contents.begin(); entry != contents.end(); entry++)
  {
    if (entry->src_node == src && entry->dest_node == dest)
    {
      return new Entry(entry->src_node, entry->dest_node, entry->cost);
    }
  }
  return NULL;
}

void Table::EditEntry(unsigned src, unsigned dest, Entry new_entry)
{
  for(deque<Entry>::iterator entry = contents.begin(); entry != contents.end(); entry++)
  {
    if (entry->src_node == src && entry->dest_node == dest)
    {
      entry->src_node = new_entry.src_node;
      entry->dest_node = new_entry.dest_node;
      entry->cost = new_entry.cost;
      return;
    }
  }
  else
  {
    contents.push_back(new_entry);
  }
}

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

deque<Entry> Table::GetEntrys() {
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

Entry* Table::GetEntry(unsigned dest) {
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
      entry->dest_node = new_entry.dest_node;
      entry->next_node = new_entry.next_node;
      entry->cost = new_entry.cost;
    }
    else {
      contents.push_back(new_entry);
    }
}

ostream & Table::Print(ostream &os) const
{
  os << "Table(entries={";
  for (deque<Entry>::const_iterator entry = contents.begin(); entry != contents.end(); entry++) { 
    os << *entry << ", ";
  }
  os << "})";
  return os;
}

#endif
