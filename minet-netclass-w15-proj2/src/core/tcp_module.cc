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


using std::cout;
using std::endl;
using std::cerr;
using std::string;

int main(int argc, char *argv[])
{
    MinetHandle mux, sock;

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

    cerr<<"STARTING..."<<endl;

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
                Packet p;
                MinetReceive(mux,p);
                unsigned tcphlen=TCPHeader::EstimateTCPHeaderLength(p);
                cerr << "estimated header len="<<tcphlen<<"\n";
                p.ExtractHeaderFromPayload<TCPHeader>(tcphlen);
                IPHeader ipl=p.FindHeader(Headers::IPHeader);
                TCPHeader tcph=p.FindHeader(Headers::TCPHeader);

                cerr << "TCP Packet: IP Header is "<<ipl<<" and ";
                cerr << "TCP Header is "<<tcph << " and ";

                cerr << "Checksum is " << (tcph.IsCorrectChecksum(p) ? "VALID" : "INVALID");

                // TODO: check for correct checksum
                // TODO: find the info to send responses to (header info, sourceIP, etc.)
                // TODO: build packet
                // TODO: send response packet

            }

            //  Data from the Sockets layer above  //
            if (event.handle==sock) 
            {
                SockRequestResponse s;
                MinetReceive(sock,s);
                cerr << "Received Socket Request:" << s << endl;

                switch(s.type)
                {
                    case CONNECT:
                    case ACCEPT:
                        // TODO: create and send response that connection is ok
                        break;
                    case STATUS:
                        // no response needed
                        break;
                    case WRITE:
                        // TODO: build new packet
                        // TODO: add header to packet (IP and TCP)
                        // TODO: send packet and request a response
                        // TODO:: send response request??
                        break;
                    case FORWARD:
                        // TODO: find connection of request
                        // TODO: request response to that connection?
                        break;
                    case CLOSE:
                        // TODO: find connection of request
                        // TODO: create and send request
                        break; 
                    default: 
                        // TODO: responsd to request with
                        break;

                }
            }
        }
    }
    return 0;
}
