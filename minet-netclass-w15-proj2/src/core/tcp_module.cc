#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>


#include <iostream>

#include "Minet.h"
#include "tcpstate.h"


using std::cout;
using std::endl;
using std::cerr;
using std::string;
using std::cin;

// ======================================== //
//              CONST & MACROS              //
// ======================================== //
// max number of tries 
#define MAX_TRIES = 3;
// max segment size
#define MSS = 536;

// ======================================== //
//                  HELPERS                 //
// ======================================== //
Packet MakePacket(Buffer data, Connection conn, unsigned int seq_n, unsigned int ack_n, unsigned char flag)
{
  // make Packet
  unsigned size = MIN_MACRO(IP_PACKET_MAX_LENGTH-TCP_HEADER_MAX_LENGTH, data.GetSize());
  Packet send_pack(data.ExtractFront(size));
  // make IP header
  IPHeader send_iph;
  send_iph.SetProtocol(IP_PROTO_TCP);
  send_iph.SetSourceIP(conn.src);
  send_iph.SetDestIP(conn.dest);
  send_iph.SetTotalLength(size + TCP_HEADER_MAX_LENGTH + IP_HEADER_BASE_LENGTH);
  // push ip header onto packet
  send_pack.PushFrontHeader(send_iph);
  // make the TCP header
  TCPHeader send_tcph;
  send_tcph.SetSourcePort(conn.srcport, send_pack);
  send_tcph.SetDestPort(conn.destport, send_pack);
  send_tcph.SetSeqNum(seq_n, send_pack);

  // check if we need to include an ack
  if (IS_ACK(flag))
  {
    send_tcph.SetAckNum(ack_n, send_pack);
  }

  send_tcph.SetHeaderLen(TCP_HEADER_MAX_LENGTH, send_pack);
  // push tcp header onto packet
  send_pack.PushBackHeader(send_tcph);

  return send_pack;
}


// ======================================== //
//                   MAIN                   //
// ======================================== //
int main(int argc, char *argv[])
{
  MinetHandle mux, sock;

  // each element in ConnectionList is a ConnectiontoStateMappeing struct
  ConnectionList<TCPState> clist;

  MinetInit(MINET_TCP_MODULE);

  mux=MinetIsModuleInConfig(MINET_IP_MUX) ? MinetConnect(MINET_IP_MUX) : MINET_NOHANDLE;
  sock=MinetIsModuleInConfig(MINET_SOCK_MODULE) ? MinetAccept(MINET_SOCK_MODULE) : MINET_NOHANDLE;

  if (MinetIsModuleInConfig(MINET_IP_MUX) && mux==MINET_NOHANDLE) 
  {
    MinetSendToMonitor(MinetMonitoringEvent("Can't connect to mux"));
    return -1;
  }

  if (MinetIsModuleInConfig(MINET_SOCK_MODULE) && sock==MINET_NOHANDLE) 
  {
    MinetSendToMonitor(MinetMonitoringEvent("Can't accept from sock module"));
    return -1;
  }

  cerr << "\nBEGINNING TO HANDLE TCP.......\n";

  MinetSendToMonitor(MinetMonitoringEvent("tcp_module handling TCP traffic"));

  MinetEvent event;

  while (MinetGetNextEvent(event)==0) 
  {
    // if we received an unexpected type of event, print error
    if (event.eventtype!=MinetEvent::Dataflow || event.direction!=MinetEvent::IN) 
    {
      MinetSendToMonitor(MinetMonitoringEvent("Unknown event ignored."));
    } 

    // else if we received a valid event from Minet, do processing
    else 
    {
      //  Data from the IP layer below  //
      if (event.handle==mux) 
      {
        cerr << "\nHANDLING DATA FROM IP LAYER BELOW\n";
        Packet rec_pack;
        MinetReceive(mux,rec_pack);
        unsigned tcphlen=TCPHeader::EstimateTCPHeaderLength(rec_pack);
        cerr << "estimated header len="<<tcphlen<<"\n";
        rec_pack.ExtractHeaderFromPayload<TCPHeader>(tcphlen);
        IPHeader rec_ip_h=rec_pack.FindHeader(Headers::IPHeader);
        TCPHeader rec_tcp_h=rec_pack.FindHeader(Headers::TCPHeader);

        cerr << "TCP Packet:\n IP Header is "<<rec_ip_h<<"\n";
        cerr << "TCP Header is "<<rec_tcp_h << "\n";
        cerr << "Checksum is " << (rec_tcp_h.IsCorrectChecksum(rec_pack) ? "VALID\n\n" : "INVALID\n\n");

        cerr << "PACKET CONTENTS: " << rec_pack << "\n";

        // Unpacking useful data
        Connection conn;
        rec_ip_h.GetDestIP(conn.src);
        rec_ip_h.GetSourceIP(conn.dest);
        rec_ip_h.GetProtocol(conn.protocol);
        rec_tcp_h.GetDestPort(conn.srcport);
        rec_tcp_h.GetSourcePort(conn.destport);

        // do we need to switch these as we send packets back and forth
        unsigned int rec_seq_n;
        unsigned int rec_ack_n;
        unsigned int send_ack_n;
        rec_tcp_h.GetSeqNum(rec_seq_n);
        rec_tcp_h.GetAckNum(rec_ack_n);
        send_ack_n = rec_seq_n + 1;

        unsigned char flag;
        rec_tcp_h.GetFlags(flag);

        // testing code
        TCPState hardCodedState(1000, LISTEN, 2);
        ConnectionToStateMapping<TCPState> hardCodedConnection(conn, Time(3), hardCodedState, true);
        clist.push_back(hardCodedConnection);
 
        // check if there is already a connection
        ConnectionList<TCPState>::iterator cs = clist.FindMatching(conn);
        // if there is an open connection
        if (cs != clist.end())
        {   
          cerr << "Found matching connection\n";
          rec_tcp_h.GetHeaderLen((unsigned char&)tcphlen);
          tcphlen -= TCP_HEADER_MAX_LENGTH;
          Buffer &data = rec_pack.GetPayload().ExtractFront(tcphlen);
          cerr << "this is the data: " << data << "\n";
          SockRequestResponse write(WRITE, (*cs).connection, data, tcphlen, EOK);

          MinetSend(sock, write);

          // TODO: check for correct checksum
          // TODO: find the info to send responses to (header info, sourceIP, etc.)
          // TODO: build packet
          // TODO: send response packet
          unsigned char send_flag;
          SockRequestResponse res;
          switch(cs->state.GetState())
          {
            case LISTEN:
            {
              cerr << "LISTEN STATE\n";
              // coming from ACCEPT in socket layer
              if (IS_SYN(flag))
              {
                SET_SYN(send_flag);
                SET_ACK(send_flag);
                Packet send_pack = MakePacket(Buffer(NULL, 0), conn, rec_ack_n, send_ack_n, send_flag);
                MinetSend(mux, send_pack);
                cs->state.SetState(SYN_RCVD);
              }
            }
            break;
            case SYN_RCVD:
            {
              cerr << "SYN_RCVD STATE\n";
              if (IS_ACK(flag))
              {
                cs->state.SetState(ESTABLISHED);
                // create res to send to sock
                res.type = WRITE;
                res.connection = conn;
                // TODO fix this
                res.bytes = 0;
                res.error = EOK;
                MinetSend(sock, res);
              }
            }
            break;
            case SYN_SENT:
            {
              cerr << "SYN_SENT STATE\n";
              if (IS_SYN(flag) && IS_ACK(flag))
              {
                SET_ACK(send_flag);
                Packet send_pack = MakePacket(Buffer(NULL, 0), conn, rec_ack_n, send_ack_n, send_flag);
                MinetSend(mux, send_pack);
                cs->state.SetState(ESTABLISHED);

                // create res to send to sock
                res.type = WRITE;
                res.connection = conn;
                // TODO fix this
                res.bytes = 0;
                res.error = EOK;
                MinetSend(sock, res);
              }
            }
            break;
            case SYN_SENT1:
            {
              cerr << "SYN_SENT1 STATE\n";
              // may not need this
            }
            break;
            case ESTABLISHED:
            {
              cerr << "ESTABLISHED STATE\n";
              // if the otherside is ready to close
              if (IS_FIN(flag))
              {
                // send back ACK for the FIN
                cs->state.SetState(CLOSE_WAIT);
              }
              // else otherside continues to send data
              else
              {
                if (IS_ACK(flag))
                {
                  // set the states
                  // if there is data
                  if (IS_PSH(flag))
                  {
                    SET_ACK(send_flag);
                    Packet send_pack = MakePacket(Buffer(NULL, 0), conn, rec_ack_n, send_ack_n, send_flag);
                    MinetSend(mux, send_pack);

                    // set window stuff

                    // create res to send to sock
                    res.type = WRITE;
                    res.connection = conn;
                    
                    // maybe get packet payload?
                    // TODO fix this
                    // res.data = connection.data;
                    // res.bytes = data.length;
                    
                    res.error = EOK;
                    // send a WRITE in socket layer 
                    MinetSend(sock, res);
                  }
                }
              }
            }
            break;
            case SEND_DATA:
            {
              cerr << "SEND_DATA STATE\n";
            }
            break;
            case CLOSE_WAIT:
            {
              cerr << "CLOSE_WAIT STATE\n";
              if (IS_FIN(flag))
              {
                // send a fin ack back
                cs->state.SetState(LAST_ACK);
              }
            }
            break;
            case FIN_WAIT1:
            {
              cerr << "FIN_WAIT1 STATE\n";
            }
            break;
            case CLOSING:
            {
              cerr << "CLOSING STATE\n";
            }
            break;
            case LAST_ACK:
            {
              cerr << "LAST_ACK STATE\n";
              if (IS_ACK(flag))
              {
                cs->state.SetState(CLOSED);
              }
            }
            break;
            case FIN_WAIT2:
            {
              cerr << "FIN_WAIT2 STATE\n";
              // 
            }
            break;
            case TIME_WAIT:
            {
              cerr << "TIME_WAIT STATE\n";
            }
            break;
            default:
              break;
          }
        }
        // else there is no open connection
        else
        {
          cerr << "Could not find matching connection\n";
        }

      }

      //  Data from the Sockets layer above  //
      if (event.handle==sock) 
      {
        cerr << "\nHANDLING DATA FROM SOCKETS LAYER ABOVE\n";
        SockRequestResponse req;
        SockRequestResponse res;
        MinetReceive(sock, req);
        cerr << "Received Socket Request:" << req << endl;

        switch(req.type)
        {
          case CONNECT:
          {
            cerr << "\n===CONNECT===\n";
            cerr << "\n===END CONNECT===\n";
          }
          case ACCEPT:
          {
            // passive open
            cerr << "\n===ACCEPT===\n";

            // unsigned int init_seq_n = rand();
            TCPState accept_conn(rand(), LISTEN, MAX_TRIES);
            // add window size - "N" value
            // may need to change timeout time
            ConnectionToStateMapping<TCPState> new_conn(req.connection, Time(), accept_conn, false);
            clist.push_back(new_conn);

           
            res.type = STATUS;
            res.connection = req.connection;
            res.bytes = 0;
            res.error = EOK;
            MinetSend(sock, res);
            cerr << "\n=== END ACCEPT===\n";
          }
          break;
          case WRITE:
          {
            // HAVE NOT TESTED
            cerr << "\n===WRITE===\n";
            unsigned size = MIN_MACRO(IP_PACKET_MAX_LENGTH-TCP_HEADER_MAX_LENGTH, s.data.GetSize());
            // create the payload of the packet
            Packet send_pack(s.data.ExtractFront(size));
            // make IP header because we need to do tcp checksum
            IPHeader send_ip_h;
            send_ip_h.SetProtocol(IP_PROTO_TCP);
            send_ip_h.SetSourceIP(s.connection.src);
            send_ip_h.SetDestIP(s.connection.dest);
            send_ip_h.SetTotalLength(size + TCP_HEADER_MAX_LENGTH + IP_HEADER_BASE_LENGTH);
            // push ip header onto packet
            send_pack.PushFrontHeader(send_ip_h);
            // make the TCP header.GetSeqNum
            TCPHeader send_tcp_h;
            send_tcp_h.SetSourcePort(s.connection.srcport, send_pack);
            send_tcp_h.SetDestPort(s.connection.destport, send_pack);
            send_tcp_h.SetHeaderLen(TCP_HEADER_MAX_LENGTH, send_pack);
            // push the TCP header behind the IP header
            send_pack.PushBackHeader(send_tcp_h);
            MinetSend(mux, send_pack);
            cerr << "\n===END WRITE===\n";
          }
          break;
          case FORWARD:
          {
            cerr << "\n===FORWARD===\n";
            // TODO: find connection of request
            // TODO: request response to that connection?
            cerr << "\n===END FORWARD===\n";
          }
          break;
          case CLOSE:
          {
            cerr << "\n===CLOSE===\n";
            cerr << "\n===END CLOSE===\n";
          }
            // TODO: find connection of request
            // TODO: create and send request
          break; 
          case STATUS:
          {
            cerr << "\n===STATUS===\n";
            // no response needed
            cerr << "\n===END STATUS===\n";
          }
          break;
          default:
          {
            cerr << "\n===DEFAULT===\n";
            cerr << "\n===END DEFAULT===\n";
          } 
            // TODO: responsd to request with
          break;

        }
      }
    }
  }
  return 0;
}
