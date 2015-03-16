#include "node.h"
#include "context.h"
#include "error.h"

#if defined(LINKSTATE)
Node::Node(const unsigned n, SimulationContext *c, double b, double l) : 
    number(n), context(c), bw(b), lat(l), seq_num(0) 
{}

Node::Node(const Node &rhs) : 
  number(rhs.number), context(rhs.context), bw(rhs.bw), lat(rhs.lat, seq_num(rhs.seq_num)) {}
#endif

#if defined(GENERIC)
Node::Node(const unsigned n, SimulationContext *c, double b, double l) : 
    number(n), context(c), bw(b), lat(l)
{}

Node::Node(const Node &rhs) : 
  number(rhs.number), context(rhs.context), bw(rhs.bw), lat(rhs.lat) {}
#endif

#if defined(DISTANCEVECTOR)
Node::Node(const unsigned n, SimulationContext *c, double b, double l) : 
    number(n), context(c), bw(b), lat(l)
{}

Node::Node(const Node &rhs) : 
  number(rhs.number), context(rhs.context), bw(rhs.bw), lat(rhs.lat) {}
#endif

Node::Node() 
{ throw GeneralException(); }

Node & Node::operator=(const Node &rhs) 
{
  return *(new(this)Node(rhs));
}

void Node::SetNumber(const unsigned n) 
{ number = n;}

unsigned Node::GetNumber() const 
{ return number;}

void Node::SetLatency(const double l)
{ lat = l;}

double Node::GetLatency() const 
{ return lat;}

void Node::SetBW(const double b)
{ bw = b;}

double Node::GetBW() const 
{ return bw;}

Node::~Node()
{}

// Implement these functions  to post an event to the event queue in the event simulator
// so that the corresponding node can recieve the ROUTING_MESSAGE_ARRIVAL event at the proper time
void Node::SendToNeighbors(const RoutingMessage *message)
{
  context->SendToNeighbors(this, message);
}

void Node::SendToNeighbor(const Node *dest, const RoutingMessage *message)
{
  context->SendToNeighbor(this, dest, message);
}

deque<Node*> *Node::GetNeighbors()
{
  return context->GetNeighbors(this);
}

void Node::SetTimeOut(const double timefromnow)
{
  context->TimeOut(this,timefromnow);
}


bool Node::Matches(const Node &rhs) const
{
  return number==rhs.number;
}


#if defined(GENERIC)
void Node::LinkHasBeenUpdated(const Link *l)
{
  cerr << *this << " got a link update: "<<*l<<endl;
  //Do Something generic:
  SendToNeighbors(new RoutingMessage);
}


void Node::ProcessIncomingRoutingMessage(const RoutingMessage *m)
{
  cerr << *this << " got a routing messagee: "<<*m<<" Ignored "<<endl;
}


void Node::TimeOut()
{
  cerr << *this << " got a timeout: ignored"<<endl;
}

Node *Node::GetNextHop(const Node *destination) const
{
  return 0;
}

Table *Node::GetRoutingTable() const
{
  return new Table;
}


ostream & Node::Print(ostream &os) const
{
  os << "Node(number="<<number<<", lat="<<lat<<", bw="<<bw<<")";
  return os;
}

#endif

#if defined(LINKSTATE)


void Node::LinkHasBeenUpdated(const Link *link)
{
  cerr << *this<<": Link Update: "<<*link<<endl;

  unsigned src = GetNumber();
  unsigned dest = link->GetDest();
  double new_cost = link->GetLatency();
  Entry* neighbor = table.GetEntry(src, dest);

  Entry* new_entry = Entry(src, dest, new_cost);
  table.EditEntry(src, dest, new_entry);

  SendToNeighbors(new RoutingMessage(seq_num+1, table));
}


void Node::ProcessIncomingRoutingMessage(const RoutingMessage *m
{
  cerr << *this << " Routing Message: "<<*m;

  unsigned message_seq_n = m->seq_num;
  Table message_table = m->table;

  if (message_seq_n > seq_num)
  {
    // recalculate table
    table.SetContents(message_table.GetContents());
    SendToNeighbors(m);
  }
}

void Node::TimeOut()
{
  cerr << *this << " got a timeout: ignored"<<endl;
}

Node *Node::GetNextHop(const Node *destination) const
{
  // WRITE
  return 0;
}

Table *Node::GetRoutingTable() const
{
  // WRITE
  return 0;
}


ostream & Node::Print(ostream &os) const
{
  os << "Node(number=" << number << ", lat=" << lat << ", bw=" << bw << ")";
  return os;
}
#endif


#if defined(DISTANCEVECTOR)

void Node::UpdatesFromNeighbors();

void Node::LinkHasBeenUpdated(const Link *link)
{
  cerr << *this << ": Link Update: " << *link << endl;

  unsigned dest = link->GetDest();
  double new_cost = link->GetLatency();
  Entry* neighbor = table.GetEntry(dest);

  /*
    Cases:
      1. No entry currently in the table -> Make one.
      2. Cost directly to neighbor is lower than cost current in table -> Update.
      3. Cost in table is for direct path (e.g. information outdated) -> Update.
  */
  if (neighbor == NULL|| 
    neighbor->cost > new_cost ||
    neighbor->dest_node == neighbor->next_node)
  {
    // cerr << "Found neighbor: " << neighbor << endl;
    table.EditEntry(dest, Entry(dest, dest, new_cost));
    // cerr << *this << ": Current Table: " << table << endl;

    // Making a new Node a good idea? Look for GetNode by dest or something...
    // ignoring bw and l because they're unimportant
    SendToNeighbors(new RoutingMessage(*this, Node(dest, context, 0, 0), new_cost));
    UpdatesFromNeighbors();
  }
}


void Node::ProcessIncomingRoutingMessage(const RoutingMessage *message)
{
  // message.Print(cerr);
  cerr << *this << ": Received Message: " << *message << endl;

  // unpack data
  Node src = message->src;
  unsigned src_num = src.GetNumber();
  Node dest = message->dest;
  unsigned dest_num = dest.GetNumber();
  double sd_cost = message->cost;

  // unnecessary routing message received
  if (dest_num == GetNumber()) {
    return;
  }

  // check this node's distance to the destination in message
  Entry* src_entry = table.GetEntry(src_num);
  Entry* dest_entry = table.GetEntry(dest_num);

  // compare that with this node's distance to the source in the message + cost in the message

  /* Cases:
      1. src_entry nonexistent -> no updates possible.
      2. dest_entry nonexistent -> definitely update.
      3. cost in table > new cost -> update.
  */
  if (src_entry == NULL) {
    return;
  }
  else if (dest_entry == NULL || 
    dest_entry->cost > src_entry->cost + sd_cost) 
  {
    double new_cost = src_entry->cost + sd_cost;
    table.EditEntry(dest_num, Entry(dest_num, src_num, new_cost));

    SendToNeighbors(new RoutingMessage(*this, Node(dest_num, context, 0, 0), new_cost));
    UpdatesFromNeighbors();
  }
}

void Node::TimeOut()
{
  cerr << *this << " got a timeout: ignored" << endl;
}


Node *Node::GetNextHop(const Node *destination) const
{
  cerr << "Next Hop, Table: " << table << endl;
  unsigned dest_num = destination->GetNumber();
  Entry* dest_entry = GetRoutingTable()->GetEntry(dest_num);

  return new Node(dest_entry->next_node, NULL, 0, 0);
}

Table *Node::GetRoutingTable() const
{
  return new Table(table);
}

void Node::UpdatesFromNeighbors() 
{
  deque<Entry> entries = table.GetEntrys();
  deque<Node*> neighbors = GetNeighbors();

  for(deque<Entry>::iterator entry = entries.begin(); entry != entries.end(); entry++)
  {
    double lowest_cost_so_far = entry->cost;
    unsigned next_so_far = entry->next_node;

    for(deque<Node*>::iterator neighbor = neighbors.begin(); neighbor != neighbors.end(); neighbor++)
    {
      // should be no way this is null...
      double neighbor_cost = table.GetEntry(neighbor->GetNumber())->cost;
      Entry* neighbor_to_dest = neighbor->GetRoutingTable()->GetEntry(entry->dest_node);
      if (neighbor_to_dest != NULL)
      {
        double this_cost = neighbor_cost + neighbor_to_dest->cost;
        if (this_cost < lowest_cost_so_far)
        {
          lowest_cost_so_far = this_cost;
          next_so_far = neighbor->GetNumber();
        }
      }
    }
    // cost was lowered. change and flood
    if (lowest_cost_so_far != entry->cost)
    {
      table.EditEntry(entry->dest_node, Entry(entry->dest_node, next_so_far, lowest_cost_so_far));
      SendToNeighbors(new RoutingMessage(*this, Node(entry->dest_node, context, 0, 0), lowest_cost_so_far));
    }
  }

}


ostream & Node::Print(ostream &os) const
{
  os << "Node(number=" << number << ", lat=" << lat << ", bw=" << bw;
  return os;
}
#endif
