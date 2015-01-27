#include "minet_socket.h"
#include <stdlib.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/stat.h>

#define BUFSIZE 1024
#define FILENAMESIZE 100
#define MAX_QUEUE_LENGTH 10

int handle_connection(int, int, int, fd_set);
int writenbytes(int,char *,int);
int readnbytes(int,char *,int);

int main(int argc,char *argv[])
{
  int server_port;
  int server_sock, client_sock;
  struct sockaddr_in server_sa, client_sa;
  int rc,i;
  fd_set master;
  fd_set read_fds;
  int fdmax;

  /* parse command line args */
  if (argc != 3)
  {
    fprintf(stderr, "usage: http_server1 k|u port\n");
    exit(-1);
  }
  server_port = atoi(argv[2]);
  if (server_port < 1500)
  {
    fprintf(stderr,"INVALID PORT NUMBER: %d; can't be < 1500\n",server_port);
    exit(-1);
  }

  /* initialize fd sets */
  FD_ZERO(&master);
  FD_ZERO(&read_fds);

  /* initialize and make socket */
  server_sock = socket(AF_INET, SOCK_STREAM, 0);
  if (server_sock < 0)
  {
    fprintf(stderr, "Error creating socket. Exiting.\n");
    return -1;
  }

  /* set server address*/
  memset(&server_sa, 0, sizeof(server_sa));
  server_sa.sin_family = AF_INET;
  server_sa.sin_addr.s_addr = htonl(INADDR_ANY);
  server_sa.sin_port = htons(server_port);

  /* bind listening socket */
  if (bind(server_sock, (struct sockaddr *) &server_sa, sizeof(server_sa)) < 0)
  {
    fprintf(stderr, "Error binding socket. Exiting.\n");
    return -1;
  }

  /* start listening */
  if (listen(server_sock, MAX_QUEUE_LENGTH) < 0)
  {
    fprintf(stderr, "Error setting listener. Exiting.\n");
    return -1;
  }

  FD_SET(server_sock, &master);
  fdmax = server_sock;

  /* connection handling loop */
  while(1)
  {
    /* create read list */
    read_fds = master;

    /* do a select */
    if (select(fdmax+1, &read_fds, NULL, NULL, NULL) == -1) 
    {
      fprintf(stderr, "Error in select usage.\n");
      return -1;
    }

    /* process sockets that are ready */
    for(i = 0; i <= fdmax; i++) 
    {
      /* for the accept socket, add accepted connection to connections */
      if (FD_ISSET(i, &read_fds)) 
      { 
        if (i == server_sock)
        {
          memset(&client_sa, 0, sizeof(client_sa));
          rc = sizeof(client_sa);
          client_sock = accept(server_sock, (struct sockaddr *) &client_sa, (socklen_t*) &rc); 
          if (client_sock < 0)
          {
            fprintf(stderr, "Error accepting socket. Exiting.\n");
            return -1;
          }
          else
          {
            FD_SET(client_sock, &master);
            if (client_sock > fdmax)
            {
              fdmax = client_sock;
            }
            fprintf(stdout, "[SELECT] new connection from %s on socket %d\n", inet_ntoa(client_sa.sin_addr), client_sock);
          }
        }
        else /* for a connection socket, handle the connection */
        {
          rc = handle_connection(i, server_sock, fdmax, master);
        }
      }
    }
  }
}

int handle_connection(int client_sock, int server_sock, int fdmax, fd_set master)
{
  char filename[FILENAMESIZE+1];
  int rc;
  int fd;
  struct stat filestat;
  char buf[BUFSIZE+1];
  char *headers;
  char *endheaders;
  char *bptr;
  int datalen=0;
  char *ok_response_f = "\nHTTP/1.0 200 OK\r\n"\
                      "Content-type: text/plain\r\n"\
                      "Content-length: %d \r\n\r\n";
  char ok_response[100];
  char *notok_response = "\nHTTP/1.0 404 FILE NOT FOUND\r\n"\
                         "Content-type: text/html\r\n\r\n"\
                         "<html><body bgColor=black text=white>\n"\
                         "<h2>404 FILE NOT FOUND</h2>\n"\
                         "</body></html>\n";
  bool ok=true;

  /* first read loop -- get request and headers*/
  memset(&buf, 0, BUFSIZE);
  memset(&filename, 0, FILENAMESIZE);
  datalen = read(client_sock, &buf, BUFSIZE);
  fprintf(stdout, "[REQUEST] %s", buf);

  /* parse request to get file name */
  /* Assumption: this is a GET request and filename contains no spaces*/
  int i=0, j=0;
  bool copy = false;
  while (buf[i] != 0) {
    if (buf[i] == 32 && j==0)
    {
      // " " ascii number is 32
      // first loop, start copying into filename
      copy = true;
    }
    else if (buf[i] == 32 && copy)
    {
      // finished copying filename
      break;
    }
    else if (copy)
    {
      filename[j] = buf[i];
      j++;
    }
    i++;
  }
  fprintf(stdout, "[FILE] %s\n", filename);

  /* try opening the file */
  FILE* stream;
  if (access(filename, F_OK) != -1)
  {
    fprintf(stdout, "[FILE] File found!\n\n");
    stream = fopen(filename, "r");
    fd = fileno(stream);
    fstat(fd, &filestat);
  }
  else
  {
    fprintf(stderr, "[FILE] File not found.\n");
    char cwd[1024];
    fprintf(stdout, "[FILE-CWD] %s\n\n", getcwd(cwd, 1024));
    ok = false;
  }

  /* send response */
  if (ok)
  {
    int count_left = filestat.st_size;
    /* send headers */
    memset(&ok_response, 0, 100);
    sprintf(ok_response, ok_response_f, count_left);
    fprintf(stdout, "[RES] Response: %s\n", ok_response);
    datalen = send(client_sock, ok_response, strlen(ok_response)+1, 0);
    if (datalen < 0)
    {
      fprintf(stderr, "[SOCK] Could not send headers to client socket.\n");
      return -1;
    }
    /* send file */
    int to_copy;
    while (count_left > 0)
    {
      for(i; i <= fdmax; i++)
      {
        if (FD_ISSET(i, &master) && i != server_sock && i != client_sock)
        {
          memset(&buf, 0, BUFSIZE);
          to_copy = (count_left > 1000) ? 1000 : count_left;
          fprintf(stdout, "[RES] Copying %d bytes.", to_copy);
          fread(buf, sizeof(char), to_copy, stream);
          datalen = send(client_sock, buf, BUFSIZE, 0);
          if (datalen < 0)
          {
            fprintf(stderr, "[SOCK] Could not send headers to client socket.\n");
            return -1;
          }
          count_left -= to_copy;

        }
      }
    }
  }
  else	// send error response
  {
    memset(&buf, 0, BUFSIZE);
    datalen = send(client_sock, notok_response, strlen(notok_response)+1, 0);
  }

  /* close socket and free space */
  close(client_sock);
  FD_CLR(i, &master);
  if (ok)
    return 0;
  else
    return -1;
}

int readnbytes(int fd,char *buf,int size)
{
  int rc = 0;
  int totalread = 0;
  while ((rc = minet_read(fd,buf+totalread,size-totalread)) > 0)
    totalread += rc;

  if (rc < 0)
  {
    return -1;
  }
  else
    return totalread;
}

int writenbytes(int fd,char *str,int size)
{
  int rc = 0;
  int totalwritten =0;
  while ((rc = minet_write(fd,str+totalwritten,size-totalwritten)) > 0)
    totalwritten += rc;

  if (rc < 0)
    return -1;
  else
    return totalwritten;
}

