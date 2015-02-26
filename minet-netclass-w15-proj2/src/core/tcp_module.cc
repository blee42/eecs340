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

#define MAX_TRIES 3
#define MSS 536
#define TIMEOUT 10

// ======================================== //
//                  HELPERS                 //
// ======================================== //
Packet MakePacket(Buffer data, Connection conn, unsigned int seq_n, unsigned int ack_n, unsigned char flag)
{
  // Make Packet
  unsigned size = MIN_MACRO(IP_PACKET_MAX_LENGTH-TCP_HEADER_MAX_LENGTH, data.GetSize());
  Packet send_pack(data.ExtractFront(size));

  // Make and push IP header
  IPHeader send_iph;
  send_iph.SetProtocol(IP_PROTO_TCP);
  send_iph.SetSourceIP(conn.src);
  send_iph.SetDestIP(conn.dest);
  send_iph.SetTotalLength(size + TCP_HEADER_MAX_LENGTH + IP_HEADER_BASE_LENGTH);
  send_pack.PushFrontHeader(send_iph);

  // Make and push TCP header
  TCPHeader send_tcph;
  send_tcph.SetSourcePort(conn.srcport, send_pack);
  send_tcph.SetDestPort(conn.destport, send_pack);
  send_tcph.SetSeqNum(seq_n, send_pack);
  send_tcph.SetHeaderLen(TCP_HEADER_MAX_LENGTH, send_pack);
  if (IS_ACK(flag))
  {
    send_tcph.SetAckNum(ack_n, send_pack);
  }
  send_pack.PushBackHeader(send_tcph);

  return send_pack;
}


// ======================================== //
//                   MAIN                   //
// ======================================== //
int main(int argc, char *argv[])
{
  MinetHandle mux, sock;

  ConnectionList<TCPState> clist;

  MinetInit(MINET_TCP_MODULE);

  mux = MinetIsModuleInConfig(MINET_IP_MUX) ? MinetConnect(MINET_IP_MUX) : MINET_NOHANDLE;
  sock = MinetIsModuleInConfig(MINET_SOCK_MODULE) ? MinetAccept(MINET_SOCK_MODULE) : MINET_NOHANDLE;

  if (MinetIsModuleInConfig(MINET_IP_MUX) && mux == MINET_NOHANDLE) 
  {
    MinetSendToMonitor(MinetMonitoringEvent("Can't connect to mux"));
    return -1;
  }

  if (MinetIsModuleInConfig(MINET_SOCK_MODULE) && sock == MINET_NOHANDLE) 
  {
    MinetSendToMonitor(MinetMonitoringEvent("Can't accept from sock module"));
    return -1;
  }

  cerr << "\nBEGINNING TO HANDLE TCP.......\n";

  MinetSendToMonitor(MinetMonitoringEvent("tcp_module handling TCP traffic"));

  MinetEvent event;

  while (MinetGetNextEvent(event, TIMEOUT) == 0) 
  {
    // Timeout
    if (event.eventtype == MinetEvent::Timeout)
    {

    }
    // Unexpected event type
    else if (event.eventtype != MinetEvent::Dataflow || event.direction != MinetEvent::IN)
    {
      MinetSendToMonitor(MinetMonitoringEvent("Unknown event ignored."));
    } 
    //  Data from the IP layer below  //
    else if (event.handle == mux) 
    {
      cerr << "\nHANDLING DATA FROM IP LAYER BELOW\n";

      Packet rec_pack;
      MinetReceive(mux, rec_pack);
      
      unsigned tcphlen = TCPHeader::EstimateTCPHeaderLength(rec_pack);
      rec_pack.ExtractHeaderFromPayload<TCPHeader>(tcphlen);
      cerr << "estimated header len=" << tcphlen << "\n";

      IPHeader rec_ip_h = rec_pack.FindHeader(Headers::IPHeader);
      TCPHeader rec_tcp_h = rec_pack.FindHeader(Headers::TCPHeader);

      cerr << "TCP Packet:\n IP Header is "<< rec_ip_h <<"\n";
      cerr << "TCP Header is "<< rec_tcp_h << "\n";
      cerr << "Checksum is " << (rec_tcp_h.IsCorrectChecksum(rec_pack) ? "VALID\n\n" : "INVALID\n\n");
      cerr << "PACKET CONTENTS: " << rec_pack << "\n";

      // REMOVED HARDCODED PACKET AND NO LONGER WORKS

      // Unpack useful data
      Connection conn;
      rec_ip_h.GetDestIP(conn.src);
      rec_ip_h.GetSourceIP(conn.dest);
      rec_ip_h.GetProtocol(conn.protocol);
      rec_tcp_h.GetDestPort(conn.srcport);
      rec_tcp_h.GetSourcePort(conn.destport);

      // Get window size
      unsigned short rwnd;
      rec_tcp_h.GetWinSize(rwnd);

      // do we need to switch these as we send packets back and forth
      unsigned int rec_seq_n;
      unsigned int rec_ack_n;
      unsigned int send_ack_n;
      unsigned int send_seq_n;
      rec_tcp_h.GetSeqNum(rec_seq_n);
      rec_tcp_h.GetAckNum(rec_ack_n);
      send_ack_n = rec_seq_n + 1;

      unsigned char flag;
      rec_tcp_h.GetFlags(flag);
      cerr << "FLAG: " << flag << endl;

      // Check for open connection
      ConnectionList<TCPState>::iterator cs = clist.FindMatching(conn);
      cerr << "CONN: " << conn << endl;
      cerr << "CLIST: " << clist << endl;

      if (cs != clist.end() && rec_tcp_h.IsCorrectChecksum(rec_pack))
      {   
        cerr << "Found matching connection\n";
        rec_tcp_h.GetHeaderLen((unsigned char&)tcphlen);
        tcphlen -= TCP_HEADER_MAX_LENGTH;
        Buffer &data = rec_pack.GetPayload().ExtractFront(tcphlen);
        cerr << "this is the data: " << data << "\n";

        unsigned char send_flag;
        SockRequestResponse res;

        switch(cs->state.GetState())
        {
          case CLOSED:
          {
            cerr << "CLOSED STATE\n";
          }
          break;
          case LISTEN:
          {
            cerr << "LISTEN STATE\n";
            // coming from ACCEPT in socket layer
            if (IS_SYN(flag))
            {
              send_seq_n = rand();

              cs->state.SetState(SYN_RCVD);
              cs->state.SetLastAcked(rec_ack_n);
              cs->state.SetLastRecvd(rec_seq_n);
              cs->state.SetLastSent(send_seq_n); // generate random SEQ # to send out

              // timer?

              SET_SYN(send_flag);
              SET_ACK(send_flag);
              Packet send_pack = MakePacket(Buffer(NULL, 0), conn, send_seq_n, send_ack_n, send_flag);
              MinetSend(mux, send_pack);

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
              send_seq_n = cs->state.GetLastSent() + 1;

              cs->state.SetState(ESTABLISHED);
              cs->state.SetLastAcked(rec_ack_n);
              cs->state.SetLastRecvd(rec_seq_n);
              cs->state.SetLastSent(send_seq_n);


              SET_ACK(send_flag);
              Packet send_pack = MakePacket(Buffer(NULL, 0), conn, rec_ack_n, send_ack_n, send_flag);
              MinetSend(mux, send_pack);

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
          {
            cerr << "DEFAULTED STATE\n";
          }
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
    else if (event.handle == sock) 
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

          TCPState connect_conn(rand(), LISTEN, MAX_TRIES);
          connect_conn.N = 0;
          // may need to change timeout time
          ConnectionToStateMapping<TCPState> new_conn(req.connection, Time(3), connect_conn, false);
          clist.push_front(new_conn);

          TCPState connect_conn(rand(), LISTEN, MAX_TRIES);
          connect_conn.N = 0;
          // may need to change timeout time
          ConnectionToStateMapping<TCPState> new_conn(req.connection, Time(3), connect_conn, false);
          clist.push_front(new_conn);
         
          res.type = STATUS;
          res.connection = req.connection;
          res.bytes = 0;
          res.error = EOK;
          MinetSend(sock, res);

          unsigned char send_flag;
          SET_SYN(send_flag);
          Packet send_pack = MakePacket(Buffer(NULL, 0), new_conn.connection, rand(), 0, send_flag); // not sure what the seq_n should be
          MinetSend(mux, send_pack);

          cerr << "\n===END CONNECT===\n";
        }
        break;
        case ACCEPT:
        {
          // passive open
          cerr << "\n===ACCEPT===\n";

          // unsigned int init_seq_n = rand();
          TCPState accept_conn(rand(), LISTEN, MAX_TRIES);
          accept_conn.N = 0; // set window size to something
          // may need to change timeout time
          ConnectionToStateMapping<TCPState> new_conn(req.connection, Time(3), accept_conn, false);
          clist.push_front(new_conn);
         
          res.type = STATUS;
          res.connection = req.connection;
          res.bytes = 0;
          res.error = EOK;
          MinetSend(sock, res);

          cerr << "\n===END ACCEPT===\n";
        }
        break;
        case WRITE:
        {
          cerr << "\n===WRITE===\n";

          ConnectionList<TCPState>::iterator cs = clist.FindMatching(req.connection);
          if (cs != clist.end() && cs->state.GetState() == ESTABLISHED)
          {
            cerr << "\n===WRITE: CONNECTION FOUND===\n";
            // any chance of too much data?
            // put data into a buffer to send to sock layer
            res.bytes = req.bytes;
            res.error = EOK;
            res.type = STATUS;
            res.connection = req.connection;
            MinetSend(sock, res);

            unsigned char send_flag;
            SET_ACK(send_flag);
            // TODO: need to loop because write may need more than one packet
            // TODO: save seq and ack number with the state
            // TODO: state.setlastsent - need to set this as getlastsent + mss
            // Packet send_pack = MakePacket(req.data, req.connection, , , send_flag);
            // MinetSend(mux, send_pack);
          }
          else
          {
            cerr << "\n===WRITE: NO CONNECTION FOUND===\n";
            res.connection = req.connection;
            res.type = STATUS;
            res.bytes = req.bytes;
            res.error = ENOMATCH;
          }
          
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
  return 0;
}
