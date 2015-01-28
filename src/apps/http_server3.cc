// #include "minet_socket.h"
// #include <stdlib.h>
// #include <fcntl.h>
// #include <ctype.h>
// #include <sys/stat.h>


// #define FILENAMESIZE 100
// #define BUFSIZE 1024
// #define MAX_QUEUE_LENGTH 10

// typedef enum \
// {NEW,READING_HEADERS,WRITING_RESPONSE,READING_FILE,WRITING_FILE,CLOSED} states;

// typedef struct connection_s connection;
// typedef struct connection_list_s connection_list;

// struct connection_s
// {
//   int sock;
//   int fd;
//   char filename[FILENAMESIZE+1];
//   char buf[BUFSIZE+1];
//   char *endheaders;
//   bool ok;
//   long filelen;
//   states state;
//   int headers_read,response_written,file_read,file_written;

//   connection *next;
// };

// struct connection_list_s
// {
//   connection *first, *last;
// };

// void add_connection(int,connection_list *);
// void insert_connection(int,connection_list *);
// void init_connection(connection *con);


// int writenbytes(int,char *,int);
// int readnbytes(int,char *,int);
// void read_headers(connection *);
// void write_response(connection *);
// void read_file(connection *);
// void write_file(connection *);

// int main(int argc,char *argv[])
// {
//   int server_port;
//   int server_sock, client_sock;
//   struct sockaddr_in server_sa, client_sa;
//   int rc;
//   fd_set readlist, writelist;
//   connection_list connections;
//   connection *i;
//   int maxfd;

//   /* parse command line args */
//   if (argc != 3)
//   {
//     fprintf(stderr, "usage: http_server3 k|u port\n");
//     exit(-1);
//   }
//   server_port = atoi(argv[2]);
//   if (server_port < 1500)
//   {
//     fprintf(stderr,"INVALID PORT NUMBER: %d; can't be < 1500\n",server_port);
//     exit(-1);
//   }

//   /* initialize and make socket */
//   server_sock = socket(AF_INET, SOCK_STREAM, 0);
//   if (server_sock < 0)
//   {
//     fprintf(stderr, "Error creating socket. Exiting.\n", );
//     return -1;
//   }

//   /* set server address*/
//   memset(&server_sa, 0, sizeof(server_sa));
//   server_sa.sin_family = AF_INET;
//   server_sa.sin_addr.s_addr = htonl(INADDR_ANY);
//   server_sa.sin_port = htons(server_port);

//   /* bind listening socket */
//   if (bind(server_sock, (struct sockaddr *) &server_sa, sizeof(server_sa)) < 0)
//   {
//     fprintf(stderr, "Error binding socket. Exiting.\n", );
//     return -1;
//   }

//   /* start listening */
//   if (listen(server_sock, MAX_QUEUE_LENGTH) < 0)
//   {
//     fprintf(stderr, "Error setting listener. Exiting.\n", );
//     return -1;
//   }

//   /* initialize connections list */
//   init_connection(i);
//   connections->first = i;
//   connections->last = i;

//   maxfd = server_sock;

//   /* connection handling loop */
//   while(1)
//   {
//     /* create read and write lists */
//     // NOT SURE HOW TO DO THIS:
//     // Do we loop through all of the connections in the connection list
//     // and look to see what is read/write and then create those read/write
//     // lists?

//     /* do a select */
//     if (select(maxfd+1, &read_fds, NULL, NULL, NULL) == -1)
//     {
//       fprintf(stderr, "Error in select usage.\n", );
//       return -1;
//     }

//     /* process sockets that are ready */
//     for (i = 0; i <= maxfd; i++)
//     {
//       if (FD_ISSET(i, &read_fds)) // should this just be read, what about write?
//       {
//         /* if socket is the server sock */
//         if (i == server_sock)
//         {
//           memset(&client_sa, 0, sizeof(client_sa));
//           rc = szieof(cloent_sa);
//           client_sock = accept(server_scok, (struct sockaddr *) &client_sa, (socklen_t*) &rc);
//           if (client_sock == EAGAIN)
//           {
//             //handle eagain case
//           }
//           else if (client_sock == EWOULDBLOCK)
//           {
//             //handle ewouldbblock case
//           }
//           else if (client_sock < 0)
//           {
//             fprintf(stderr, "Error accepting socket. Exiting.\n");
//             return -1;
//           }
//           else
//           {
//             //add client sock to open connection list?
//             if (client_sock > fdmax)
//             {
//               fdmax = client_sock;
//             }
//           }
//         }
//         /* if socket is a connection socket */
//         else
//         {
//           // what are we doing here?
//         }
//       }
//     }
//   }
// }

// void read_headers(connection *con)
// {
//   /* first read loop -- get request and headers*/

//   /* parse request to get file name */
//   /* Assumption: this is a GET request and filename contains no spaces*/
  
//   /* get file name and size, set to non-blocking */
    
//     /* get name */
    
//     /* try opening the file */
      
// 	/* set to non-blocking, get size */
  
//   write_response(con);
// }

// void write_response(connection *con)
// {
//   int sock2 = con->sock;
//   int rc;
//   int written = con->response_written;
//   char *ok_response_f = "HTTP/1.0 200 OK\r\n"\
//                       "Content-type: text/plain\r\n"\
//                       "Content-length: %d \r\n\r\n";
//   char ok_response[100];
//   char *notok_response = "HTTP/1.0 404 FILE NOT FOUND\r\n"\
//                          "Content-type: text/html\r\n\r\n"\
//                          "<html><body bgColor=black text=white>\n"\
//                          "<h2>404 FILE NOT FOUND</h2>\n"\
//                          "</body></html>\n";
//   /* send response */
//   if (con->ok)
//   {
//     /* send headers */
//   }
//   else
//   {
//   }  
// }

// void read_file(connection *con)
// {
//   int rc;

//     /* send file */
//   rc = read(con->fd,con->buf,BUFSIZE);
//   if (rc < 0)
//   { 
//     if (errno == EAGAIN)
//       return;
//     fprintf(stderr,"error reading requested file %s\n",con->filename);
//     return;
//   }
//   else if (rc == 0)
//   {
//     con->state = CLOSED;
//     minet_close(con->sock);
//   }
//   else
//   {
//     con->file_read = rc;
//     con->state = WRITING_FILE;
//     write_file(con);
//   }
// }

// void write_file(connection *con)
// {
//   int towrite = con->file_read;
//   int written = con->file_written;
//   int rc = writenbytes(con->sock, con->buf+written, towrite-written);
//   if (rc < 0)
//   {
//     if (errno == EAGAIN)
//       return;
//     minet_perror("error writing response ");
//     con->state = CLOSED;
//     minet_close(con->sock);
//     return;
//   }
//   else
//   {
//     con->file_written += rc;
//     if (con->file_written == towrite)
//     {
//       con->state = READING_FILE;
//       con->file_written = 0;
//       read_file(con);
//     }
//     else
//       printf("shouldn't happen\n");
//   }
// }

// int readnbytes(int fd,char *buf,int size)
// {
//   int rc = 0;
//   int totalread = 0;
//   while ((rc = minet_read(fd,buf+totalread,size-totalread)) > 0)
//     totalread += rc;

//   if (rc < 0)
//   {
//     return -1;
//   }
//   else
//     return totalread;
// }  

// int writenbytes(int fd,char *str,int size)
// {
//   int rc = 0;
//   int totalwritten =0;
//   while ((rc = minet_write(fd,str+totalwritten,size-totalwritten)) > 0)
//     totalwritten += rc;
  
//   if (rc < 0)
//     return -1;
//   else
//     return totalwritten;
// }


// // inserts a connection in place of a closed connection
// // if there are no closed connections, appends the connection 
// // to the end of the list

// void insert_connection(int sock,connection_list *con_list)
// {
//   connection *i;
//   for (i = con_list->first; i != NULL; i = i->next)
//   {
//     if (i->state == CLOSED)
//     {
//       i->sock = sock;
//       i->state = NEW;
//       return;
//     }
//   }
//   add_connection(sock,con_list);
// }
 
// void add_connection(int sock,connection_list *con_list)
// {
//   connection *con = (connection *) malloc(sizeof(connection));
//   con->next = NULL;
//   con->state = NEW;
//   con->sock = sock;
//   if (con_list->first == NULL)
//     con_list->first = con;
//   if (con_list->last != NULL)
//   {
//     con_list->last->next = con;
//     con_list->last = con;
//   }
//   else
//     con_list->last = con;
// }

// void init_connection(connection *con)
// {
//   con->headers_read = 0;
//   con->response_written = 0;
//   con->file_read = 0;
//   con->file_written = 0;
// }
