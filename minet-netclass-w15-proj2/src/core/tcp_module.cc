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
#define GBN MSS*5
#define RTT 5

// ======================================== //
//                  HELPERS                 //
// ======================================== //
Packet MakePacket(Buffer data, Connection conn, unsigned int seq_n, unsigned int ack_n, unsigned char flag)
{
  // Make Packet
  unsigned size = MIN_MACRO(IP_PACKET_MAX_LENGTH-TCP_HEADER_MAX_LENGTH, data.GetSize());
  Packet send_pack(data.ExtractFront(size));

  // Make and push IP header
  IPHeader send_ip_h;
  send_ip_h.SetProtocol(IP_PROTO_TCP);
  send_ip_h.SetSourceIP(conn.src);
  send_ip_h.SetDestIP(conn.dest);
  send_ip_h.SetTotalLength(size + TCP_HEADER_BASE_LENGTH + IP_HEADER_BASE_LENGTH);
  send_pack.PushFrontHeader(send_ip_h);

  // Make and push TCP header
  TCPHeader send_tcp_h;
  send_tcp_h.SetSourcePort(conn.srcport, send_pack);
  send_tcp_h.SetDestPort(conn.destport, send_pack);
  send_tcp_h.SetHeaderLen(TCP_HEADER_BASE_LENGTH/4, send_pack);
  send_tcp_h.SetFlags(flag, send_pack);
  send_tcp_h.SetWinSize(1, send_pack); // to fix
  send_tcp_h.SetSeqNum(seq_n, send_pack);
  if (IS_ACK(flag))
  {
    send_tcp_h.SetAckNum(ack_n, send_pack);
  }
  send_tcp_h.RecomputeChecksum(send_pack);
  send_pack.PushBackHeader(send_tcp_h);

  cerr << "TCP Packet:\n IP Header is "<< send_ip_h <<"\n";
  cerr << "TCP Header is "<< send_tcp_h << "\n";
  cerr << "PACKET:\n" << send_pack << endl;

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
    // cerr << "\n === EVENT START === \n";
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

      unsigned char rec_flag;
      rec_tcp_h.GetFlags(rec_flag);

      // Check for open connection
      ConnectionList<TCPState>::iterator cs = clist.FindMatching(conn);
      // ConnectionList<TCPState>::iterator cs = clist.FindMatchingSource(conn);
      // cerr << "CONN: " << conn << endl;
      // cerr << "CLIST: " << clist << endl;

      if (cs != clist.end() && rec_tcp_h.IsCorrectChecksum(rec_pack))
      {   
        cerr << "Found matching connection\n";
        rec_tcp_h.GetHeaderLen((unsigned char&)tcphlen);
        tcphlen -= TCP_HEADER_MAX_LENGTH;
        Buffer data = rec_pack.GetPayload().ExtractFront(tcphlen);
        cerr << "this is the data: " << data << "\n";

        unsigned char send_flag = 0;
        SockRequestResponse res;
        Packet send_pack;

        switch(cs->state.GetState())
        {
          case CLOSED:
          {
            cerr << "CLOSEDMUX:  STATE\n";
          }
          break;
          case LISTEN:
          {
            cerr << "\n=== MUX: LISTEN STATE ===\n";
            // coming from ACCEPT in socket layer
            if (IS_SYN(rec_flag))
            {
              send_seq_n = rand();

              cs->state.SetState(SYN_RCVD);
              // cs->state.SetLastAcked(rec_ack_n);
              cs->state.SetLastRecvd(rec_seq_n);
              cs->state.SetLastSent(send_seq_n); // generate random SEQ # to send out

              // timer?
              cs->bTmrActive = true;
              cs->timeout = Time()+RTT;

              SET_SYN(send_flag);
              SET_ACK(send_flag);
              send_pack = MakePacket(Buffer(NULL, 0), conn, send_seq_n, send_ack_n, send_flag); // ack
              MinetSend(mux, send_pack);
            }
          }
          break;
          case SYN_RCVD:
          {
            cerr << "\n=== MUX: SYN_RCVD STATE ===\n";
            cerr << "rec_flag: " << rec_flag << endl;
            cerr << "rec_ack_n: " << rec_ack_n << endl;
            cerr << "get last sent: " << cs->state.GetLastSent() << endl;
            if (IS_ACK(rec_flag) && cs->state.GetLastSent() == rec_ack_n - 1)
            {
              cs->state.SetState(ESTABLISHED);
              // cs->state.SetLastAcked(rec_ack_n); // -1?
              cs->state.SetLastRecvd(rec_seq_n); // okay think about all of this
              cs->state.SetLastSent(send_seq_n);

              // timer


              // create res to send to sock
              res.type = WRITE;
              res.connection = conn;
              res.bytes = 0;
              res.error = EOK;
              MinetSend(sock, res);
            }
          }
          break;
          case SYN_SENT:
          {
            cerr << "\n=== MUX: SYN_SENT STATE ===\n";
            if (IS_SYN(rec_flag) && IS_ACK(rec_flag))
            {
              send_seq_n = cs->state.GetLastSent() + 1;

              cs->state.SetState(ESTABLISHED);
              // cs->state.SetLastAcked(rec_ack_n);
              cs->state.SetLastRecvd(rec_seq_n);
              cs->state.SetLastSent(send_seq_n);


              SET_ACK(send_flag);
              send_pack = MakePacket(Buffer(NULL, 0), conn, rec_ack_n, send_ack_n, send_flag);
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
            cerr << "\n=== MUX: SYN_SENT1 STATE ===\n";
            // may not need this
          }
          break;
          case ESTABLISHED:
          {
            cerr << "\n=== MUX: ESTABLISHED STATE ===\n";
            // if the otherside is ready to close
            if (IS_FIN(rec_flag))
            {
              send_seq_n = cs->state.GetLastSent() + 1;

              cs->state.SetState(CLOSE_WAIT);
              cs->state.SetLastSent(send_seq_n);
              cs->state.SetLastRecvd(rec_seq_n);
              // cs->state.SetLastAcked(rec_ack_n);

              SET_ACK(send_flag);
              send_pack = MakePacket(Buffer(NULL, 0), conn, send_seq_n, send_ack_n, send_flag);
              MinetSend(mux, send_pack);
            }
            // else otherside continues to send data
            else
            {
              if (IS_ACK(rec_flag))
              {
                // clears the buffer - maybe -1?
                cs->state.SendBuffer.Erase(0, rec_ack_n - cs->state.GetLastAcked());

                cs->state.SetLastAcked(rec_ack_n);
                cs->state.SetLastRecvd(rec_seq_n);
  
                // if there is data
                if (IS_PSH(rec_flag))
                {
                  // if there is overflow of the recieved data
                  if (cs->state.RecvBuffer.GetSize() < data.GetSize()) 
                  {
                    cs->state.RecvBuffer.AddBack(data.ExtractFront(cs->state.RecvBuffer.GetSize()));
                    cs->state.SetLastRecvd(rec_seq_n + cs->state.RecvBuffer.GetSize()); // maybe -1
                  }
                  // else there is no overflow
                  else
                  {
                    cs->state.RecvBuffer.AddBack(data);
                    cs->state.SetLastRecvd(rec_seq_n + data.GetSize()); // maybe -1
                  }

                  SET_ACK(send_flag);
                  send_pack = MakePacket(Buffer(NULL, 0), conn, rec_ack_n, send_ack_n, send_flag);
                  MinetSend(mux, send_pack);

                  // set window stuff

                  // create res to send to sock
                  res.type = WRITE;
                  res.connection = conn;
                  res.data = cs->state.RecvBuffer;
                  res.bytes = cs->state.RecvBuffer.GetSize();
                  res.error = EOK;
                  // send a WRITE in socket layer 
                  MinetSend(sock, res);
                  cs->state.SetLastSent(cs->state.GetLastSent() + 1);
                }
              }
            }
          }
          break;
          case SEND_DATA:
          {
            cerr << "\n=== MUX: SEND_DATA STATE ===\n";
          }
          break;
          case CLOSE_WAIT:
          {
            cerr << "\n=== MUX: CLOSE_WAIT STATE ===\n";
            if (IS_FIN(rec_flag))
            {
              // send a fin ack back
              cs->state.SetState(LAST_ACK);
            }
          }
          break;
          case FIN_WAIT1:
          {
            cerr << "\n=== MUX: FIN_WAIT1 STATE ===\n";
          }
          break;
          case CLOSING:
          {
            cerr << "\n=== MUX: CLOSING STATE ===\n";
          }
          break;
          case LAST_ACK:
          {
            cerr << "\n=== MUX: LAST_ACK STATE ===\n";
            if (IS_ACK(rec_flag))
            {
              cs->state.SetState(CLOSED);
            }
          }
          break;
          case FIN_WAIT2:
          {
            cerr << "\n=== MUX: FIN_WAIT2 STATE ===\n";
            // 
          }
          break;
          case TIME_WAIT:
          {
            cerr << "\n=== MUX: TIME_WAIT STATE ===\n";
          }
          break;
          default:
          {
            cerr << "\n=== MUX: DEFAULTED STATE ===\n";
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
      Packet send_pack;
      unsigned char send_flag;
      cerr << "Received Socket Request:" << req << endl;

      switch(req.type)
      {
        case CONNECT:
        {
          cerr << "\n=== SOCK: CONNECT ===\n";

          TCPState connect_conn(rand(), LISTEN, MAX_TRIES);
          connect_conn.N = 0;
          // may need to change timeout time
          ConnectionToStateMapping<TCPState> new_conn(req.connection, Time(), connect_conn, false);
          clist.push_front(new_conn);
         
          res.type = STATUS;
          res.connection = req.connection;
          res.bytes = 0;
          res.error = EOK;
          MinetSend(sock, res);

          SET_SYN(send_flag);
          Packet send_pack = MakePacket(Buffer(NULL, 0), new_conn.connection, rand(), 0, send_flag); // not sure what the seq_n should be
          MinetSend(mux, send_pack);

          cerr << "\n=== SOCK: END CONNECT ===\n";
        }
        break;
        case ACCEPT:
        {
          // passive open
          cerr << "\n=== SOCK: ACCEPT ===\n";

          // unsigned int init_seq_n = rand();
          TCPState accept_conn(rand(), LISTEN, MAX_TRIES);
          accept_conn.N = 0; // set window size to something
          // may need to change timeout time
          ConnectionToStateMapping<TCPState> new_conn(req.connection, Time(), accept_conn, false);
          clist.push_front(new_conn);
         
          res.type = STATUS;
          res.connection = req.connection;
          res.bytes = 0;
          res.error = EOK;
          MinetSend(sock, res);

          cerr << "\n=== SOCK: END ACCEPT ===\n";
        }
        break;
        case WRITE:
        {
          cerr << "\n=== SOCK: WRITE ===\n";

          ConnectionList<TCPState>::iterator cs = clist.FindMatching(req.connection);
          if (cs != clist.end() && cs->state.GetState() == ESTABLISHED)
          {
            cerr << "\n=== SOCK: WRITE: CONNECTION FOUND ===\n";
            // put data in buffer
            size_t send_buffer_size = cs->state.TCP_BUFFER_SIZE - cs->state.SendBuffer.GetSize();
            // if there is more data than buffer space
            if (send_buffer_size < req.bytes)
            {
              cs->state.SendBuffer.AddBack(req.data.ExtractFront(send_buffer_size));

              res.bytes = send_buffer_size;
              res.error = EBUF_SPACE;
            }
            // else there is no overflow
            else
            {
              cs->state.SendBuffer.AddBack(req.data);

              res.bytes = req.bytes;
              res.error = EOK;
            }
            
            res.type = STATUS;
            res.connection = req.connection;
            MinetSend(sock, res);

            // send data from buffer using "Go Back N"
            unsigned int win_size = cs->state.GetN(); // window size
            unsigned int rwnd = cs->state.GetRwnd(); // receiver congestion window
            size_t cwnd = cs->state.SendBuffer.GetSize(); // sender congestion window

            // iterate through all the packets
            while(win_size < GBN)
            {
              Buffer data;
              rwnd = rwnd - win_size;
              cwnd = cwnd - win_size;

              // if MSS < rwnd < cwnd or MSS < cwnd < rwnd
              // MSS is the smallest
              // not sure why arithmetic shift?
              if(((MSS < rwnd && rwnd << cwnd) || (MSS < cwnd && cwnd < rwnd)) && (win_size + MSS < GBN))
              {
                data = cs->state.SendBuffer.Extract(win_size, MSS);
                // set new seq_n
                cs->state.SetLastSent(cs->state.GetLastSent() + MSS);
                // move on to the next set of packets
                win_size = win_size + MSS;
                SET_ACK(send_flag);
                send_pack = MakePacket(data, cs->connection, cs->state.GetLastSent(), cs->state.GetLastRecvd() + 1, send_flag);

              }

              // else if cwnd < MSS < rwnd or cwnd < rwnd < MSS
              // cwnd is the smallest
              else if (((cwnd < MSS && MSS << rwnd) || (cwnd < rwnd && rwnd < MSS)) && (win_size + cwnd < GBN))
              {
                data = cs->state.SendBuffer.Extract(win_size, cwnd);
                // set new seq_n
                cs->state.SetLastSent(cs->state.GetLastSent() + cwnd);
                // move on to the next set of packets
                win_size = win_size + cwnd;
                SET_ACK(send_flag);
                send_pack = MakePacket(data, cs->connection, cs->state.GetLastSent(), cs->state.GetLastRecvd() + 1, send_flag);
              }

              // else if rwnd < MSS < cwnd or rwnd < cwnd < MSS
              // rwnd is hte smallest
              else if (win_size + rwnd < GBN)
              {
                data = cs->state.SendBuffer.Extract(win_size, rwnd);
                // set new seq_n
                cs->state.SetLastSent(cs->state.GetLastSent() + rwnd);
                win_size = win_size + cwnd;
                SET_ACK(send_flag);
                send_pack = MakePacket(data, cs->connection, cs->state.GetLastSent(), cs->state.GetLastRecvd() + 1, send_flag);
              }

              MinetSend(mux, send_pack);
              // set timeout
            }

            cs->state.N = win_size;

            // TODO: need to loop because write may need more than one packet
            // TODO: save seq and ack number with the state
            // TODO: state.setlastsent - need to set this as getlastsent + mss
          }
          else
          {
            cerr << "\n=== SOCK: WRITE: NO CONNECTION FOUND ===\n";
            res.connection = req.connection;
            res.type = STATUS;
            res.bytes = req.bytes;
            res.error = ENOMATCH;
          }
          
          cerr << "\n=== SOCK: END WRITE ===\n";
        }
        break;
        case FORWARD:
        {
          cerr << "\n=== SOCK: FORWARD ===\n";
          // TODO: find connection of request
          // TODO: request response to that connection?
          cerr << "\n=== SOCK: END FORWARD ===\n";
        }
        break;
        case CLOSE:
        {
          cerr << "\n=== SOCK: CLOSE ===\n";
          cerr << "\n=== SOCK: END CLOSE ===\n";
        }
          // TODO: find connection of request
          // TODO: create and send request
        break; 
        case STATUS:
        {
          cerr << "\n=== SOCK: STATUS ===\n";
          
          cerr << "\n=== SOCK: END STATUS ===\n";
        }
        break;
        default:
        {
          cerr << "\n=== SOCK: DEFAULT ===\n";
          cerr << "\n=== SOCK: END DEFAULT ===\n";
        } 
          // TODO: responsd to request with
        break;

      }
    }
  }
  return 0;
}
