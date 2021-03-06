#ifndef _messages
#define _messages

#include <iostream>

#include "node.h"
#include "link.h"

#if defined(GENERIC)
struct RoutingMessage {
 public:
  ostream & Print(ostream &os) const;
};
#endif

#if defined(LINKSTATE)
struct RoutingMessage {
  unsigned seq_num;
  Table table;

  RoutingMessage();
  RoutingMessage(const RoutingMessage &rhs);
  RoutingMessage &operator=(const RoutingMessage &rhs);
  RoutingMessage(unsigned seq_n, Table t);

  ostream & Print(ostream &os) const;
};
#endif

#if defined(DISTANCEVECTOR)
struct RoutingMessage {
  Node src;
  Node dest;
  double cost;

  RoutingMessage();
  RoutingMessage(const RoutingMessage &rhs);
  RoutingMessage &operator=(const RoutingMessage &rhs);
  RoutingMessage(Node s, Node d, double c);

  ostream & Print(ostream &os) const;
};
#endif

inline ostream & operator<<(ostream &os, const RoutingMessage &m) { return m.Print(os);}

#endif
