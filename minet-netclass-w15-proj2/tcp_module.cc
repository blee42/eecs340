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
#include <algorithm>

#include "Minet.h"
#include "tcpstate.h"


using std::cout;
using std::endl;
using std::cerr;
using std::string;
using std::cin;
using std::min;
using std::max;

// ======================================== //
//              CONST & MACROS              //
// ======================================== //

#define MAX_TRIES 5
#define MSS 536
#define TIMEOUT 10
#define GBN MSS*16
#define RTT 5

#define RECV_BUF_SIZE(state) (state.TCP_BUFFER_SIZE - state.RecvBuffer.GetSize())
#define SEND_BUF_SIZE(state) (state.TCP_BUFFER_SIZE - state.SendBuffer.GetSize())

// ======================================== //
//                  HELPERS                 //
// ======================================== //
Packet MakePacket(Buffer data, Connection conn, unsigned int seq_n, unsigned int ack_n, size_t win_size, unsigned char flag)
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
  send_tcp_h.SetWinSize(win_size, send_pack); // to fix
  send_tcp_h.SetSeqNum(seq_n, send_pack);
  if (IS_ACK(flag))
  {
    send_tcp_h.SetAckNum(ack_n, send_pack);
  }
  send_tcp_h.RecomputeChecksum(send_pack);
  send_pack.PushBackHeader(send_tcp_h);

  cerr << "== MAKING PACKET ==" << endl;
  cerr << send_ip_h <<"\n";
  cerr << send_tcp_h << "\n";
  cerr << send_pack << endl;

  return send_pack;
}


// ======================================== //
//                   MAIN                   //
// ======================================== //
int main(int argc, char *argv[])
{
  MinetHandle mux, sock;
  srand(time(NULL)); // generate seeds
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

  MinetSendToMonitor(MinetMonitoringEvent("tcp_module handling TCP traffic"));

  MinetEvent event;

  while (MinetGetNextEvent(event, TIMEOUT) == 0) 
  {

    if (event.eventtype == MinetEvent::Timeout)
    {
      // check all connections in connection list
      for (ConnectionList<TCPState>::iterator cs = clist.begin(); cs != clist.end(); cs++)
      {
        // check for closed connections
        if (cs->state.GetState() == CLOSED)
        {
          clist.erase(cs);
        }

        // check for active timers
        Time curr_time = Time();
        if (cs->bTmrActive == true && cs->timeout < curr_time)
        {
          // if maxed out number of tries
          if (cs->state.ExpireTimerTries())
          {
            SockRequestResponse res;
            res.type = CLOSE;
            res.connection = cs->connection;
            res.error = EOK;
            MinetSend(sock, res);
          }
          // else handle each case of timeout
          else
          {
            Packet send_pack;
            unsigned char send_flag;
            switch(cs->state.GetState())
            {
              case SYN_RCVD:
              {
                cerr << "TIMEOUT: SYN_RCVD - SEND SYN ACK" << endl;
                SET_SYN(send_flag);
                SET_ACK(send_flag);
                MakePacket(Buffer(NULL, 0), cs->connection, cs->state.GetLastSent(), cs->state.GetLastRecvd(), RECV_BUF_SIZE(cs->state), send_flag);
              }
              break;
              case SYN_SENT:
              {
                cerr << "TIMEOUT: SYN_SENT - SEND SYN" << endl;
                SET_SYN(send_flag);
                MakePacket(Buffer(NULL, 0), cs->connection, cs->state.GetLastSent(), cs->state.GetLastRecvd(), SEND_BUF_SIZE(cs->state), send_flag); 
              } 
              break;
              case ESTABLISHED:
              {
                cerr << "TIMEOUT: ESTABLISHED - SEND DATA" << endl;
                // use GBN to resend data
                if (cs->state.N > 0)
                {
                  cerr << "TIMEOUT: ESTABLISHED - SEND DATA - GBN" << endl;

                  unsigned int inflight_n = cs->state.GetN();
                  unsigned int rwnd = cs->state.GetRwnd(); // receiver congestion window
                  size_t cwnd = cs->state.SendBuffer.GetSize(); // sender congestion window

                  Buffer data;
                  while(inflight_n < GBN && cwnd != 0 && rwnd != 0)
                  {
                    cerr << "\n inflight_n: " << inflight_n << endl;
                    cerr << "\n rwnd: " << rwnd << endl;
                    cerr << "\n cwnd: " << cwnd << endl;
                    send_flag = 0;

                    // if MSS < rwnd and MSS < cwnd
                    // space in rwnd and cwnd
                    if(MSS < rwnd && MSS < cwnd)
                    {
                      cerr << "space in rwnd and cwnd" << endl;
                      data = cs->state.SendBuffer.Extract(inflight_n, MSS);

                      inflight_n = inflight_n + MSS;
                      CLR_SYN(send_flag);
                      SET_ACK(send_flag);
                      SET_PSH(send_flag);
                      send_pack = MakePacket(data, cs->connection, cs->state.GetLastSent(), cs->state.GetLastRecvd() + 1, SEND_BUF_SIZE(cs->state), send_flag);

                      cerr << "SET1: " << cs->state.GetLastSent() << endl;
                      cs->state.SetLastSent(cs->state.GetLastSent() + MSS);
                      cerr << "SET2: " << cs->state.GetLastSent() << endl;
                    }

                    // else space in cwnd or rwnd
                    else
                    {
                      cerr << "space in either or" << endl;
                      data = cs->state.SendBuffer.Extract(inflight_n, min((int)rwnd, (int)cwnd));

                      inflight_n = inflight_n + min((int)rwnd, (int)cwnd);
                      CLR_SYN(send_flag);
                      SET_ACK(send_flag);
                      SET_PSH(send_flag);
                      send_pack = MakePacket(data, cs->connection, cs->state.GetLastSent(), cs->state.GetLastRecvd() + 1, SEND_BUF_SIZE(cs->state), send_flag);
                      cs->state.SetLastSent(cs->state.GetLastSent() + min((int)rwnd, (int)cwnd));
                    }

                    MinetSend(mux, send_pack);

                    if ((rwnd < rwnd - inflight_n) && (cwnd < cwnd - inflight_n)) 
                    {
                      break;
                    }
                    else
                    {
                      rwnd = rwnd - inflight_n;
                      cwnd = cwnd - inflight_n;                
                    }

                    cerr << "\n inflight_n: " << inflight_n << endl;
                    cerr << "rwnd: " << rwnd << endl;
                    cerr << "cwnd: " << cwnd << endl;

                    // set timeout
                    cs->bTmrActive = true;
                    cs->timeout = Time() + RTT;
                  }


                  cs->state.N = inflight_n;

                }
                // otherwise just need to resend ACK
                else
                {
                  cerr << "TIMEOUT: ESTABLISHED - SEND DATA - SEND ACK" << endl;
                  SET_ACK(send_flag);
                  MakePacket(Buffer(NULL, 0), cs->connection, cs->state.GetLastSent(), cs->state.GetLastRecvd() + 1, RECV_BUF_SIZE(cs->state), send_flag);
                }
              }
              break;
              case CLOSE_WAIT:
              {
                cerr << "TIMEOUT: CLOSE_WAIT - SEND ACK" << endl;
                SET_ACK(send_flag);
                MakePacket(Buffer(NULL, 0), cs->connection, cs->state.GetLastSent(), cs->state.GetLastRecvd(), RECV_BUF_SIZE(cs->state), send_flag);
              }
              break;
              case FIN_WAIT1:
              {
                cerr << "TIMEOUT: FIN_WAIT1 - SEND FIN" << endl;
                SET_FIN(send_flag);
                MakePacket(Buffer(NULL, 0), cs->connection, cs->state.GetLastSent(), cs->state.GetLastRecvd(), SEND_BUF_SIZE(cs->state), send_flag);
              }
              break;
              case CLOSING:
              {
                cerr << "TIMEOUT: CLOSING - SEND ACK" << endl;
                SET_ACK(send_flag);
                MakePacket(Buffer(NULL, 0), cs->connection, cs->state.GetLastSent(), cs->state.GetLastRecvd(), RECV_BUF_SIZE(cs->state), send_flag);
              }
              break;
              case LAST_ACK:
              {
                cerr << "TIMEOUT: LAST_ACK - SEND FIN" << endl;
                SET_FIN(send_flag);
                MakePacket(Buffer(NULL, 0), cs->connection, cs->state.GetLastSent(), cs->state.GetLastRecvd(), SEND_BUF_SIZE(cs->state), send_flag);
              }
              break;
              case TIME_WAIT:
              {
                cerr << "TIMEOUT: TIME_WAIT - SEND ACK" << endl;
                SET_ACK(send_flag);
                MakePacket(Buffer(NULL, 0), cs->connection, cs->state.GetLastSent(), cs->state.GetLastRecvd(), RECV_BUF_SIZE(cs->state), send_flag);
              }
            }
            MinetSend(mux, send_pack);
            cs->timeout = Time() + RTT;
          }
        }
      }
    }
    // Unexpected event type, signore
    else if (event.eventtype != MinetEvent::Dataflow || event.direction != MinetEvent::IN)
    {
      MinetSendToMonitor(MinetMonitoringEvent("Unknown event ignored."));
    } 
    //  Data from the IP layer below 
    else if (event.handle == mux) 
    {
      cerr << "\n === IP LAYER START === \n";

      cerr << "  == GOT A PACKET ==\n";

      Packet rec_pack;
      MinetReceive(mux, rec_pack);
      
      unsigned tcphlen = TCPHeader::EstimateTCPHeaderLength(rec_pack);
      rec_pack.ExtractHeaderFromPayload<TCPHeader>(tcphlen);

      IPHeader rec_ip_h = rec_pack.FindHeader(Headers::IPHeader);
      TCPHeader rec_tcp_h = rec_pack.FindHeader(Headers::TCPHeader);

      cerr << rec_ip_h <<"\n";
      cerr << rec_tcp_h << "\n";
      cerr << "Checksum is " << (rec_tcp_h.IsCorrectChecksum(rec_pack) ? "VALID\n\n" : "INVALID\n\n");
      cerr << rec_pack << "\n";

      // Unpack useful data
      Connection conn;
      rec_ip_h.GetDestIP(conn.src);
      rec_ip_h.GetSourceIP(conn.dest);
      rec_ip_h.GetProtocol(conn.protocol);
      rec_tcp_h.GetDestPort(conn.srcport);
      rec_tcp_h.GetSourcePort(conn.destport);

      unsigned short rwnd;
      rec_tcp_h.GetWinSize(rwnd);

      unsigned int rec_seq_n;
      unsigned int rec_ack_n;
      unsigned int send_ack_n;
      unsigned int send_seq_n;
      rec_tcp_h.GetSeqNum(rec_seq_n);
      rec_tcp_h.GetAckNum(rec_ack_n);
      send_ack_n = rec_seq_n + 1;

      unsigned char rec_flag;
      rec_tcp_h.GetFlags(rec_flag);

      ConnectionList<TCPState>::iterator cs = clist.FindMatching(conn);
      if (cs != clist.end() && rec_tcp_h.IsCorrectChecksum(rec_pack))
      {   
        cerr << "Found matching connection\n";
        cerr << "Last Acked: " << cs->state.GetLastAcked() << endl;
        cerr << "Last Sent: " << cs->state.GetLastSent() << endl;
        cerr << "Last Recv: " << cs->state.GetLastRecvd() << endl << endl;;
        rec_tcp_h.GetHeaderLen((unsigned char&)tcphlen);
        tcphlen -= TCP_HEADER_MAX_LENGTH;
        Buffer data = rec_pack.GetPayload().ExtractFront(tcphlen);
        data.Print(cerr);
        cerr << endl;

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
              cerr << "generated seq: " << send_seq_n << endl;

              cs->state.SetState(SYN_RCVD);
              cs->state.SetLastRecvd(rec_seq_n);
              cerr << "SET1: " << cs->state.GetLastSent() << endl;
              cs->state.SetLastSent(send_seq_n); // generate random SEQ # to send out
              cerr << "SET2: " << cs->state.GetLastSent() << endl;

              cs->bTmrActive = true;
              cs->timeout = Time() + RTT;

              SET_SYN(send_flag);
              SET_ACK(send_flag);
              send_pack = MakePacket(Buffer(NULL, 0), conn, send_seq_n, send_ack_n, RECV_BUF_SIZE(cs->state), send_flag); // ack
              MinetSend(mux, send_pack);
            }
          }
          break;
          case SYN_RCVD:
          {
            cerr << "\n=== MUX: SYN_RCVD STATE ===\n";
            if (IS_ACK(rec_flag) && cs->state.GetLastSent() == rec_ack_n - 1)
            {
              cs->state.SetState(ESTABLISHED);
              cs->state.SetLastRecvd(rec_seq_n - 1); // next will have same seq num

              // timer
              cs->bTmrActive = false;
              cs->state.SetTimerTries(MAX_TRIES);

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
              cerr << "Last Acked: " << cs->state.GetLastAcked() << endl;
              cerr << "Last Sent: " << cs->state.GetLastSent() << endl;
              cerr << "Last Recv: " << cs->state.GetLastRecvd() << endl;
              send_seq_n = cs->state.GetLastSent() + data.GetSize() + 1;
              cerr << "increment" << data.GetSize() + 1;

              cs->state.SetState(ESTABLISHED);
              cs->state.SetLastAcked(rec_ack_n - 1);
              cs->state.SetLastRecvd(rec_seq_n); // first data will be the same as this


              SET_ACK(send_flag);
              send_pack = MakePacket(Buffer(NULL, 0), conn, send_seq_n, send_ack_n, SEND_BUF_SIZE(cs->state), send_flag);
              MinetSend(mux, send_pack);

              // NOTE: we do this for compatibility with our tcp_server testing, which seems to receives size 6 ACKs
              // from us even though the size is shown as zero above.
              cs->state.SetLastSent(max((int) cs->state.GetLastSent() + 7, (int) send_seq_n));

              res.type = WRITE;
              res.connection = conn;
              res.bytes = 0;
              res.error = EOK;
              MinetSend(sock, res);
            }
          }
          break;
          case SYN_SENT1:
          {
            cerr << "\n=== MUX: SYN_SENT1 STATE ===\n";
            // NO IMPLEMENTATION NEEDED
          }
          break;
          case ESTABLISHED:
          {
            cerr << "\n=== MUX: ESTABLISHED STATE ===\n";

            if (IS_FIN(rec_flag))
            {
              cerr << "FIN flagged.\n";
              send_seq_n = cs->state.GetLastSent() + data.GetSize() + 1;

              cs->state.SetState(CLOSE_WAIT);
              cerr << "SET1: " << cs->state.GetLastSent() << endl;
              cs->state.SetLastSent(send_seq_n);
              cerr << "SET2: " << cs->state.GetLastSent() << endl;
              cs->state.SetLastRecvd(rec_seq_n);

              SET_ACK(send_flag);
              send_pack = MakePacket(Buffer(NULL, 0), conn, send_seq_n, send_ack_n, RECV_BUF_SIZE(cs->state), send_flag);
              MinetSend(mux, send_pack);
            }
            // Dataflow
            else
            {
              if (IS_ACK(rec_flag) && cs->state.GetLastRecvd() < rec_seq_n)
              {
                cerr << "ACK flagged.\n";

                // if (IS_PSH(rec_flag)) // not right?
                if (data.GetSize() > 0)
                {
                  cerr << "Has data..\n";
                  cerr << "Recv: " << cs->state.GetLastRecvd() << endl;
                  // if there is overflow of the recieved data
                  size_t recv_buf_size = RECV_BUF_SIZE(cs->state);
                  if (recv_buf_size < data.GetSize()) 
                  {
                    cs->state.RecvBuffer.AddBack(data.ExtractFront(recv_buf_size));
                    send_ack_n = rec_seq_n + recv_buf_size - 1;
                    cs->state.SetLastRecvd(send_ack_n);
                  }
                  else
                  {
                    cs->state.RecvBuffer.AddBack(data);
                    send_ack_n = rec_seq_n + data.GetSize() - 1;
                    cs->state.SetLastRecvd(send_ack_n);
                  }

                  send_seq_n = cs->state.GetLastSent() + min(recv_buf_size, data.GetSize());
                  cs->state.SetLastSent(send_seq_n);

                  // send ACK flag packet to mux
                  SET_ACK(send_flag);
                  send_pack = MakePacket(Buffer(NULL, 0), conn, send_seq_n, send_ack_n + 1, RECV_BUF_SIZE(cs->state), send_flag);
                  MinetSend(mux, send_pack);

                  res.type = WRITE;
                  res.connection = conn;
                  res.data = cs->state.RecvBuffer;
                  res.bytes = cs->state.RecvBuffer.GetSize();
                  res.error = EOK;
                  MinetSend(sock, res);
                }
                else
                {
                  cerr << "Not PSH flagged.\n";
                  cs->state.SendBuffer.Erase(0, rec_ack_n - cs->state.GetLastAcked() - 1);
                  cs->state.N = cs->state.N - (rec_ack_n - cs->state.GetLastAcked() - 1);

                  cs->state.SetLastAcked(rec_ack_n);
                  cs->state.SetLastRecvd(rec_seq_n);
                  cs->state.last_acked = rec_ack_n;


                  cerr << "\nSend Buffer: ";
                  cs->state.SendBuffer.Print(cerr);
                  cerr << endl;

                  cs->state.N = cs->state.N - (rec_ack_n - cs->state.GetLastAcked() - 1);
  
                  // send some of the infromation in the buffer
                  // if there is overflow in the send buffer
                  if (cs->state.SendBuffer.GetSize() - cs->state.GetN() > 0)
                  {
                    unsigned int inflight_n = cs->state.GetN();
                    unsigned int rwnd = cs->state.GetRwnd(); // receiver congestion window
                    size_t cwnd = cs->state.SendBuffer.GetSize(); // sender congestion window

                    while(inflight_n < GBN && cwnd != 0 && rwnd != 0)
                    {
                      cerr << "\n inflight_n: " << inflight_n << endl;
                      cerr << "\n rwnd: " << rwnd << endl;
                      cerr << "\n cwnd: " << cwnd << endl;
                      send_flag = 0;

                      // if MSS < rwnd and MSS < cwnd
                      // space in rwnd and cwnd
                      if(MSS < rwnd && MSS < cwnd)
                      {
                        cerr << "space in rwnd and cwnd" << endl;
                        data = cs->state.SendBuffer.Extract(inflight_n, MSS);
                        // set new seq_n
                        // move on to the next set of packets
                        inflight_n = inflight_n + MSS;
                        CLR_SYN(send_flag);
                        SET_ACK(send_flag);
                        SET_PSH(send_flag);
                        send_pack = MakePacket(data, cs->connection, cs->state.GetLastSent(), cs->state.GetLastRecvd() + 1, SEND_BUF_SIZE(cs->state), send_flag);

                        cerr << "SET1: " << cs->state.GetLastSent() << endl;
                        cs->state.SetLastSent(cs->state.GetLastSent() + MSS);
                        cerr << "SET2: " << cs->state.GetLastSent() << endl;
                      }

                      // else space in cwnd or rwnd
                      else
                      {
                        cerr << "space in either or" << endl;
                        data = cs->state.SendBuffer.Extract(inflight_n, min((int)rwnd, (int)cwnd));
                        // set new seq_n
                        // move on to the next set of packets
                        inflight_n = inflight_n + min((int)rwnd, (int)cwnd);
                        CLR_SYN(send_flag);
                        SET_ACK(send_flag);
                        SET_PSH(send_flag);
                        send_pack = MakePacket(data, cs->connection, cs->state.GetLastSent(), cs->state.GetLastRecvd() + 1, SEND_BUF_SIZE(cs->state), send_flag);
                        cs->state.SetLastSent(cs->state.GetLastSent() + min((int)rwnd, (int)cwnd));
                      }

                      MinetSend(mux, send_pack);

                      if ((rwnd < rwnd - inflight_n) && (cwnd < cwnd - inflight_n)) 
                      {
                        break;
                      }
                      else
                      {
                        rwnd = rwnd - inflight_n;
                        cwnd = cwnd - inflight_n;                
                      }

                      cerr << "\n inflight_n: " << inflight_n << endl;
                      cerr << "rwnd: " << rwnd << endl;
                      cerr << "cwnd: " << cwnd << endl;
                      // set timeout
                      cs->bTmrActive = true;
                      cs->timeout = Time() + RTT;
                    }

                    cs->state.N = inflight_n;
                  }
                  
                }
              }
            }
          }
          break;
          case SEND_DATA:
          {
            cerr << "\n=== MUX: SEND_DATA STATE ===\n";
            // NO IMPLEMENTATION NEEDED
          }
          break;
          case CLOSE_WAIT:
          {
            cerr << "\n=== MUX: CLOSE_WAIT STATE ===\n";
            if (IS_FIN(rec_flag))
            {
              // send a fin ack back
              send_seq_n = cs->state.GetLastSent() + data.GetSize() + 1;

              cs->state.SetState(LAST_ACK);
              cs->state.SetLastRecvd(rec_seq_n);
              cerr << "SET1: " << cs->state.GetLastSent() << endl;
              cs->state.SetLastSent(send_seq_n);
              cerr << "SET2: " << cs->state.GetLastSent() << endl;

              // timeout stuff
              cs->bTmrActive = true;
              cs->timeout = Time() + RTT;
              cs->state.SetTimerTries(MAX_TRIES);

              SET_FIN(send_flag);
              send_pack = MakePacket(Buffer(NULL, 0), conn, send_seq_n, send_ack_n, RECV_BUF_SIZE(cs->state), send_flag);
              MinetSend(mux, send_pack);
            }
          }
          break;
          case FIN_WAIT1:
          {
            cerr << "\n=== MUX: FIN_WAIT1 STATE ===\n";
            if (IS_FIN(rec_flag))
            {
              send_seq_n = cs->state.GetLastSent() + data.GetSize() + 1;

              cs->state.SetState(CLOSING);
              cs->state.SetLastRecvd(rec_seq_n);
              cerr << "SET1: " << cs->state.GetLastSent() << endl;
              cs->state.SetLastSent(send_seq_n);
              cerr << "SET2: " << cs->state.GetLastSent() << endl;

              // set timeout
              cs->bTmrActive = true;
              cs->timeout = Time() + RTT;
              cs->state.SetTimerTries(MAX_TRIES);

              SET_FIN(send_flag);
              SET_ACK(send_flag);
              send_pack = MakePacket(Buffer(NULL, 0), conn, send_seq_n, send_ack_n, SEND_BUF_SIZE(cs->state), send_flag);
              MinetSend(mux, send_pack);
            }
            else if (IS_ACK(rec_flag))
            {

              cs->state.SetState(FIN_WAIT2);
              cs->state.SetLastSent(send_seq_n);
              cs->state.SetLastAcked(rec_ack_n - 1);
            }
          }
          break;
          case CLOSING:
          {
            cerr << "\n=== MUX: CLOSING STATE ===\n";
            if (IS_ACK(rec_flag))
            {
              // done, not sending any other packets
              // move to time-wait
              cs->state.SetState(TIME_WAIT);
              cs->state.SetLastAcked(rec_ack_n - 1);
              cs->state.SetLastRecvd(rec_seq_n);
            }
          }
          break;
          case LAST_ACK:
          {
            cerr << "\n=== MUX: LAST_ACK STATE ===\n";
            if (IS_ACK(rec_flag))
            {
              cs->state.SetState(CLOSED);
              cs->state.SetLastAcked(rec_ack_n - 1);
              cs->state.SetLastRecvd(rec_seq_n);
            }
          }
          break;
          case FIN_WAIT2:
          {
            cerr << "\n=== MUX: FIN_WAIT2 STATE ===\n";
            if (IS_FIN(rec_flag))
            {
              send_seq_n = cs->state.GetLastSent() + data.GetSize() + 1;

              cs->state.SetState(TIME_WAIT);
              cs->state.SetLastRecvd(rec_seq_n);
              cs->state.SetLastSent(send_seq_n);
              cs->state.SetLastAcked(rec_ack_n - 1);
 
              // set timeout
              cs->bTmrActive = true;
              cs->timeout = Time() + RTT;
              cs->state.SetTimerTries(MAX_TRIES);

              SET_ACK(send_flag);
              send_pack = MakePacket(Buffer(NULL, 0), conn, send_seq_n, send_ack_n, SEND_BUF_SIZE(cs->state), send_flag);
              MinetSend(mux, send_pack);
            }
          }
          break;
          case TIME_WAIT:
          {
            cerr << "\n=== MUX: TIME_WAIT STATE ===\n";
            cs->timeout = Time() + 30;
            cs->state.SetState(CLOSED);
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
      cerr << "\n === IP LAYER DONE === \n";
    }

    //  Data from the Sockets layer above  //
    else if (event.handle == sock) 
    {
      cerr << "\n === SOCK LAYER START === \n";
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

          unsigned int init_seq = rand();
          TCPState connect_conn(init_seq, SYN_SENT, MAX_TRIES);
          connect_conn.N = 0;
          ConnectionToStateMapping<TCPState> new_conn(req.connection, Time(), connect_conn, true);
          clist.push_front(new_conn);
         
          res.type = STATUS;
          res.error = EOK;
          MinetSend(sock, res);

          SET_SYN(send_flag);
          Packet send_pack = MakePacket(Buffer(NULL, 0), new_conn.connection, init_seq, 0, SEND_BUF_SIZE(new_conn.state), send_flag);
          MinetSend(mux, send_pack);
          sleep(1);

          MinetSend(mux, send_pack);

          cerr << "\n=== SOCK: END CONNECT ===\n";
        }
        break;
        case ACCEPT:
        {
          // passive open
          cerr << "\n=== SOCK: ACCEPT ===\n";

          TCPState accept_conn(rand(), LISTEN, MAX_TRIES);
          accept_conn.N = 0;
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

            size_t send_buffer_size = SEND_BUF_SIZE(cs->state);
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
            
            cs->state.SendBuffer.Print(cerr);
            cerr << endl;

            res.type = STATUS;
            res.connection = req.connection;
            MinetSend(sock, res);

            // send data from buffer using "Go Back N"
            unsigned int inflight_n = 0; // packets in flight
            unsigned int rwnd = cs->state.GetRwnd(); // receiver congestion window
            size_t cwnd = cs->state.SendBuffer.GetSize(); // sender congestion window

            cerr << "\n outside of gbn loop\n";
            cerr << "inflight_n: " << inflight_n << endl;
            cerr << "rwnd: " << rwnd << endl;
            cerr << "cwnd: " << cwnd << endl;
            cerr << "last SENT: " << cs->state.GetLastSent() << endl;
            cerr << "last ACKED: " << cs->state.GetLastAcked() << endl;
            

            // iterate through all the packets
            Buffer data;
            while(inflight_n < GBN && cwnd != 0 && rwnd != 0)
            {
              cerr << "\n=== SOCK: WRITE: GBN LOOP ===\n";
              // if MSS < rwnd and MSS < cwnd
              // space in rwnd and cwnd
              if(MSS < rwnd && MSS < cwnd)
              {
                cerr << "space in rwnd and cwnd" << endl;
                data = cs->state.SendBuffer.Extract(inflight_n, MSS);
                // set new seq_n
                // move on to the next set of packets
                inflight_n = inflight_n + MSS;
                CLR_SYN(send_flag);
                SET_ACK(send_flag);
                SET_PSH(send_flag);
                send_pack = MakePacket(data, cs->connection, cs->state.GetLastSent(), cs->state.GetLastRecvd() + 1, SEND_BUF_SIZE(cs->state), send_flag);

                cerr << "SET1: " << cs->state.GetLastSent() << endl;
                cs->state.SetLastSent(cs->state.GetLastSent() + MSS);
                cerr << "SET2: " << cs->state.GetLastSent() << endl;
              }

              // else space in cwnd or rwnd
              else
              {
                cerr << "space in either or" << endl;
                data = cs->state.SendBuffer.Extract(inflight_n, min((int)rwnd, (int)cwnd));
                // set new seq_n
                // move on to the next set of packets
                inflight_n = inflight_n + min((int)rwnd, (int)cwnd);
                CLR_SYN(send_flag);
                SET_ACK(send_flag);
                SET_PSH(send_flag);
                send_pack = MakePacket(data, cs->connection, cs->state.GetLastSent(), cs->state.GetLastRecvd() + 1, SEND_BUF_SIZE(cs->state), send_flag);
                cs->state.SetLastSent(cs->state.GetLastSent() + min((int)rwnd, (int)cwnd));
              }

              MinetSend(mux, send_pack);

              if ((rwnd < rwnd - inflight_n) && (cwnd < cwnd - inflight_n)) 
              {
                break;
              }
              else
              {
                rwnd = rwnd - inflight_n;
                cwnd = cwnd - inflight_n;                
              }

              cerr << "\n inflight_n: " << inflight_n << endl;
              cerr << "rwnd: " << rwnd << endl;
              cerr << "cwnd: " << cwnd << endl;
              // set timeout
            }

            cs->state.N = inflight_n;
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
          // NO IMPELMTNATION NEEDED
          cerr << "\n=== SOCK: END FORWARD ===\n";
        }
        break;
        case CLOSE:
        {
          cerr << "\n=== SOCK: CLOSE ===\n";
          ConnectionList<TCPState>::iterator cs = clist.FindMatching(req.connection);
          if (cs->state.GetState() == ESTABLISHED)
          {
            unsigned char send_flag;
            Packet send_pack;
            cs->state.SetState(FIN_WAIT1);
            SET_FIN(send_flag);
            send_pack = MakePacket(Buffer(NULL, 0), cs->connection, cs->state.GetLastSent(), cs->state.GetLastRecvd() + 1, RECV_BUF_SIZE(cs->state), send_flag);
            MinetSend(mux, send_pack);
          }
          cerr << "\n=== SOCK: END CLOSE ===\n";
        }
        break; 
        case STATUS:
        {
          cerr << "\n=== SOCK: STATUS ===\n";
          ConnectionList<TCPState>::iterator cs = clist.FindMatching(req.connection);
          if (cs->state.GetState() == ESTABLISHED)
          {
            cerr << req.bytes << " out of " << cs->state.RecvBuffer.GetSize() << " read" << endl;
            // if all data read
            if (req.bytes == cs->state.RecvBuffer.GetSize())
            {
              cs->state.RecvBuffer.Clear();
            }
            // if some data still in buffer
            else
            {
              cs->state.RecvBuffer.Erase(0, req.bytes);

              res.type = WRITE;
              res.connection = req.connection;
              res.data = cs->state.RecvBuffer;
              res.bytes = cs->state.RecvBuffer.GetSize();
              res.error = EOK;

              MinetSend(sock, res);
            }
          }
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

      cerr << "\n === SOCK LAYER DONE === \n";

    }
  }
  return 0;
}
