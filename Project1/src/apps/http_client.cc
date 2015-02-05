#include "minet_socket.h"
#include <stdlib.h>
#include <ctype.h>

#define BUFSIZE 1024

int write_n_bytes(int fd, char * buf, int count);

int main(int argc, char * argv[]) {
    char * server_name = NULL;
    int server_port = 0;
    char * server_path = NULL;

    int sock = 0;
    int rc = -1;
    int datalen = 0;
    bool ok = true;
    struct sockaddr_in sin;
    FILE * wheretoprint = stdout;
    struct hostent *host = NULL;
    char * req = NULL;

    char buf[BUFSIZE + 1];
    char * bptr = NULL;
    char * bptr2 = NULL;
    char * endheaders = NULL;
   
    struct timeval timeout;
    fd_set set;

    /*parse args */
    if (argc != 5) {
	fprintf(stderr, "usage: http_client k|u server port path\n");
	exit(-1);
    }

    server_name = argv[2];
    server_port = atoi(argv[3]);
    server_path = argv[4];

    /* initialize minet */
    if (toupper(*(argv[1])) == 'K') { 
	minet_init(MINET_KERNEL);
    } else if (toupper(*(argv[1])) == 'U') { 
	minet_init(MINET_USER);
    } else {
 	fprintf(stderr, "First argument must be k or u\n");
	exit(-1);
    }

    // create request
    req = (char*) malloc(100 * sizeof(char));
    strcpy(req, "GET ");
    strcat(req, server_path);
    strcat(req, " HTTP/1.0\r\n\r\n");
    memset(&buf, 0, BUFSIZE+2);

    /* create socket */
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
      fprintf(stderr, "Could not create socket\n");
      return sock;
    }

    // Do DNS lookup
    // Hint: use gethostbyname()
    host = gethostbyname(server_name);
    if (host == NULL) {
      fprintf(stderr, "Could not get host\n");
      close(sock);
      return -1;
    }
    
    /* set address */
    memset(&sin, 0, sizeof(sin));
    sin.sin_family=AF_INET;
    sin.sin_port=htons(server_port);
    sin.sin_addr.s_addr = *(unsigned long *) host->h_addr_list[0];

    /* connect socket */
    if (connect(sock, (struct sockaddr *) &sin, sizeof(sin)) != 0) {
      fprintf(stderr, "Could not connect socket\n");
      close(sock);
      return -1;
    }
    /* send request */
    datalen = send(sock, req, strlen(req)+1, 0);
    if (datalen == -1) {
      fprintf(stderr, "Could not send request\n");
      close(sock);
      return -1;
    }
    /* wait till socket can be read */
    /* Hint: use select(), and ignore timeout for now. */
    //int status = select(1, &set, NULL, NULL, NULL);i
    int fdmax = 0;
    if (fdmax < sock) {
        fdmax = sock;
    }
    do {
      FD_ZERO(&set);
      FD_SET(sock, &set);
      rc = select(fdmax + 1, &set, NULL, NULL, NULL);
    } while (rc == -1);

    /* first read loop -- read headers */
    datalen = read(sock, &buf, BUFSIZE+1);
    if (datalen == -1) {
      fprintf(stderr, "Error in select\n");
      close(sock);
      return -1;
    }

    /* examine return code */   
    //Skip "HTTP/1.0"
    //remove the '\0'
    // Normal reply has return code 200
    int i=0;
    while (buf[i] != 32) 
    {
        i++;
    }
    char code[3];
    memset(&code, 0, 3);
    code[0] = buf[i+1];
    code[1] = buf[i+2];
    code[2] = buf[i+3];

    if (atoi(code) != 200) 
    {
        ok == false;
        while (datalen !=0)
        {
            fprintf(stderr, "%s", buf);
            memset(&buf, 0, BUFSIZE+1);
            datalen = read(sock, &buf, BUFSIZE+1);
        }
    }
    else
    {
        int j=0;
        while (buf[j] != 60)
        {
            j++;
        }
        fprintf(wheretoprint, "%s", buf+(j*sizeof(char)));
        memset(&buf, 0, BUFSIZE+1);
        datalen = read(sock, &buf, BUFSIZE+1);       
        while (datalen !=0)
        {
            fprintf(wheretoprint, "%s", buf);
            memset(&buf, 0, BUFSIZE+1);
            datalen = read(sock, &buf, BUFSIZE+1);
        }
    }

    /*close socket and deinitialize */
    close(sock);

    if (ok) {
	return 0;
    } else {
	return -1;
    }
}

int write_n_bytes(int fd, char * buf, int count) {
    int rc = 0;
    int totalwritten = 0;

    while ((rc = minet_write(fd, buf + totalwritten, count - totalwritten)) > 0) {
	totalwritten += rc;
    }
    
    if (rc < 0) {
	return -1;
    } else {
	return totalwritten;
    }
}


