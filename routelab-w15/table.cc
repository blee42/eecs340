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

deque<Entry>* Table::GetContents()
{
  return contents;
}

void SetContents(deque<Entry> new_contents)
{
  contents = new_contents;
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
    // cerr << "GET DESTINATION, CALLED WITH: " << dest << endl;
    for(deque<Entry>::iterator entry = contents.begin(); entry != contents.end(); entry++){
      if(entry->dest_node == dest){
        // cerr << "GET DESTINATION, RETURNED WITH: " << dest << endl;
        return entry;
      } 
    }
    // cerr << "GET DESTINATION, RETURNED WITH: " << dest << endl;
    return contents.end();
}

Entry* Table::GetEntry(unsigned dest) {
  // cerr << "GET ENTRY, CALLED WITH: " << dest << endl;
  deque<Entry>::iterator entry = GetDestinationEntry(dest);
  if (entry != contents.end()) 
  {
    Entry* res = new Entry(entry->dest_node, entry->next_node, entry->cost);
    // cerr << "GET ENTRY, RETRUNED WITH: " << dest << endl;
    return res;
  } 
  else 
  {
    // cerr << "GET ENTRY, RETRUNED WITH: " << dest << endl;
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
