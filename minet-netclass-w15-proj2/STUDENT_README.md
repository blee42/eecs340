Project 2: TCP

Kevin Chen & Brittany Lee.

We spend many, many, many, many long hard hours coding this together. The majority of this project was pair programmed, with small sections taken off to the side and fleshed out or debugged.

Our implementation is not perfect. We've found that the receiving side (tcp_server) functions properly in almost every single case we tested, though the send side is significantly more flawed. We hae found that both the server and client occasionally will not connect -- we've been unable to fully ascertain if that's as a result of the our code, Minet, or connections between TLAB computers.

A few more notes about tcp_client side testing -- we've found that our client does connect and talk satisfactorily with our implementation of tcp_server, but we've unable to accomplish the same with netcat. Since netcat does not print debug messages for us, we opted to focus on making tcp_server and tcp_client work together.

We did not extensively test out our closing handshake, though we're fairly confident that it matches what we're supposed to do according to the RFC documentation -- we're not certain we meticulously maintained proper ACK and SEQ numbers throughout the entire program, but we think we're pretty close.