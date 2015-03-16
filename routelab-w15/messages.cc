#include "messages.h"


#if defined(GENERIC)
ostream &RoutingMessage::Print(ostream &os) const
{
  os << "RoutingMessage()";
  return os;
}
#endif


#if defined(LINKSTATE)

ostream &RoutingMessage::Print(ostream &os) const
{
  // os << "RoutingMessage(src=" << src << ", dest=" << dest << ", cost=" << cost << ")";
  return os;
}

RoutingMessage::RoutingMessage()
{}


RoutingMessage::RoutingMessage(const RoutingMessage &rhs) :
  seq_num(rhs.seq_num), table(rhs.table)
{}

RoutingMessage::RoutingMessage(unsigned seq_n, Table t) :
  seq_num(seq_n), table(t)
{}

#endif


#if defined(DISTANCEVECTOR)

ostream &RoutingMessage::Print(ostream &os) const
{
  os << "RoutingMessage(src=" << src << ", dest=" << dest << ", cost=" << cost << ")";
  return os;
}

RoutingMessage::RoutingMessage()
{}


RoutingMessage::RoutingMessage(const RoutingMessage &rhs) :
  src(rhs.src), dest(rhs.dest), cost(rhs.cost)
{}

RoutingMessage::RoutingMessage(Node s, Node d, double c) :
  src(s), dest(d), cost(c)
{}

#endif

