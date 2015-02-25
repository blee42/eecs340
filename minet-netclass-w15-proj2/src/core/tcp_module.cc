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

int main(int argc, char *argv[])
{
    MinetHandle mux, sock;

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
                Packet p;
                MinetReceive(mux,p);
                unsigned tcphlen=TCPHeader::EstimateTCPHeaderLength(p);
                cerr << "estimated header len="<<tcphlen<<"\n";
                p.ExtractHeaderFromPayload<TCPHeader>(tcphlen);
                IPHeader ipl=p.FindHeader(Headers::IPHeader);
                TCPHeader tcph=p.FindHeader(Headers::TCPHeader);

                cerr << "TCP Packet:\n IP Header is "<<ipl<<"\n";
                cerr << "TCP Header is "<<tcph << "\n";
                cerr << "Checksum is " << (tcph.IsCorrectChecksum(p) ? "VALID\n\n" : "INVALID\n\n");

                cerr << "PACKET CONTENTS: " << p << "\n";

                Connection c;
                ipl.GetDestIP(c.src);
                ipl.GetSourceIP(c.dest);
                ipl.GetProtocol(c.protocol);
                tcph.GetDestPort(c.srcport);
                tcph.GetSourcePort(c.destport);

                // testing code
                TCPState hardCodedState(1000, LISTEN, 2);
                ConnectionToStateMapping<TCPState> hardCodedConnection(c, Time(3), hardCodedState, true);
                clist.push_back(hardCodedConnection);

                // check if there is already a connection
                ConnectionList<TCPState>::iterator cs = clist.FindMatching(c);
                // if there is an open connection
                if (cs != clist.end())
                {   
                    cerr << "Found matching connection\n";
                    tcph.GetHeaderLen((unsigned char&)tcphlen);
                    tcphlen -= TCP_HEADER_MAX_LENGTH;
                    Buffer &data = p.GetPayload().ExtractFront(tcphlen);
                    cerr << "this is the data: " << data << "\n";
                    cin >> data;
                    SockRequestResponse write(WRITE, (*cs).connection, data, tcphlen, EOK);

                    MinetSend(sock, write);
                }
                // else there is no open connection
                else
                {
                    cerr << "Could not find matching connection\n";
                }

                // TODO: check for correct checksum
                // TODO: find the info to send responses to (header info, sourceIP, etc.)
                // TODO: build packet
                // TODO: send response packet

            }

            //  Data from the Sockets layer above  //
            if (event.handle==sock) 
            {
                cerr << "\nHANDLING DATA FROM SOCKETS LAYER ABOVE\n";
                SockRequestResponse s;
                MinetReceive(sock,s);
                cerr << "Received Socket Request:" << s << endl;

                switch(s.type)
                {
                    case CONNECT:
                    case ACCEPT:
                    {
                        cerr << "\n===ACCEPT===\n";
                        SockRequestResponse reply;
                        reply.type = STATUS;
                        reply.connection = s.connection;
                        reply.bytes = 0;
                        reply.error = EOK;
                        MinetSend(sock, reply);
                        cerr << "\n=== END ACCEPT===\n";
                    }
                    break;
                    case STATUS:
                    {
                        cerr << "\n===STATUS===\n";
                        cerr << "\n===END STATUS===\n";
                    }
                        // no response needed
                    break;
                    case WRITE:
                    {
                        cerr << "\n===WRITE===\n";
                        unsigned bytes = MIN_MACRO(IP_PACKET_MAX_LENGTH-TCP_HEADER_MAX_LENGTH, s.data.GetSize());
                        // create the payload of the packet
                        Packet p(s.data.ExtractFront(bytes));
                        // make IP header because we need to do tcp checksum
                        IPHeader iph;
                        iph.SetProtocol(IP_PROTO_TCP);
                        iph.SetSourceIP(s.connection.src);
                        iph.SetDestIP(s.connection.dest);
                        iph.SetTotalLength(bytes + TCP_HEADER_MAX_LENGTH + IP_HEADER_BASE_LENGTH);
                        // push ip header onto packet
                        p.PushFrontHeader(iph);
                        // make the TCP header
                        TCPHeader tcph;
                        tcph.SetSourcePort(s.connection.srcport, p);
                        tcph.SetDestPort(s.connection.destport, p);
                        tcph.SetHeaderLen(TCP_HEADER_MAX_LENGTH, p);
                        // push the TCP header behind the IP header
                        p.PushBackHeader(tcph);
                        MinetSend(mux, p);
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
