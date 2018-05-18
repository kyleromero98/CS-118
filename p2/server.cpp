#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "packet.h"

using namespace std;

// max packet size
#define PACK_SIZE 1024
// i/o streams
#define STD_IN 0
#define STD_OUT 1
#define STD_ERR 2

int main(int argc, char** argv)
{
  // check for hostname and port no
  if (argc != 3) {
    fprintf(stderr, "Incorrect number of arguments; correct usage: ./server [hostname] [port]\n");
    exit(1);
  }

  int portNumber = atoi(argv[2]);

  int socket_fd = -1;
  struct sockaddr_in server;
  struct sockaddr_in client;
  socklen_t client_len = 0;

  // create server socket
  if ((socket_fd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
    fprintf(stderr, "Socket Error: unable to create socket, %s\n", strerror(errno));
    exit(1);
  }
  
  // init server
  bzero((char *) &server, sizeof(server));
  server.sin_family = AF_INET;
  server.sin_addr.s_addr = inet_addr(argv[1]);
  server.sin_port = htons((unsigned short)portNumber);
  
  // bind to socket
  if (bind(socket_fd, (struct sockaddr *) &server, sizeof(server)) == -1) {
    fprintf(stderr, "Bind Error: Unable to bind socket to server, %s\n", strerror(errno));
    exit(1);
  }

  // init necessary buffer vars
  char buf[PACK_SIZE];
  memset(buf, 0, PACK_SIZE);
  int bytes_read = 0;
  client_len = sizeof(client); 
  // listen phase
  while (1) {
    // read datagram from client
    bytes_read = recvfrom(socket_fd, buf, PACK_SIZE, 0, (struct sockaddr *) &client, &client_len);
    // error check the bytes read
    if (bytes_read < 0) {
      fprintf(stderr, "Read Error: Error reading SYN datagram from client, %s\n", strerror(errno));
      exit(1);
    }
    // check for syn bit
    struct packet synpack
  }

  // accept phase
  while (1) {

  }

  // communication phase
  while (1) {
    
  }
  exit(0);
}
