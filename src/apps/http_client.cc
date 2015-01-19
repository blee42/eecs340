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
    //fprintf(wheretoprint, "path: %s", &server_path);

    // create request
    req = "GET /index.html HTTP/1.0\r\n\r\n";
    // strcat(req, " HTTP/1.0\r\n\r\n");

    /* create socket */
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1)
      return sock;

    // Do DNS lookup
    // Hint: use gethostbyname()
    host = gethostbyname(server_name);
    if (host == NULL) {
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
      close(sock);
      return -1;
    }
    /* send request */
    datalen = send(sock, req, strlen(req)+1, 0);
    if (datalen == -1) {
      close(sock);
      return -1;
    }
    fprintf(wheretoprint, "sended socket\n");
    /* wait till socket can be read */
    /* Hint: use select(), and ignore timeout for now. */
    //int status = select(1, &set, NULL, NULL, NULL);i
    int socket_fd, status;
    do {
      FD_ZERO(&set);
      FD_SET(socket_fd, &set);
      status = select(socket_fd + 1, &set, NULL, NULL, NULL);
    } while (status == -1);
    if (status == -1) {
      close(sock);
      return -1;
    }
    fprintf(wheretoprint, "selected socket?\n");

    /* first read loop -- read headers */
    datalen = read(sock, &buf, BUFSIZE);
    if (datalen == -1) {
      close(sock);
      return -1;
    }
    fprintf(wheretoprint, "%s\n", buf);
   
    /* examine return code */   
    //Skip "HTTP/1.0"
    //remove the '\0'
    // Normal reply has return code 200

    /* print first part of response */

    /* second read loop -- print out the rest of the response */
    
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


