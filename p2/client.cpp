#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <sstream>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <fstream>

// our included files
#include "packet.h"
#include "constants.h"
#include "connman.h"

int main (int argc, char* argv[]) {
  int sockfd;
  char *filename;
  struct sockaddr_in servaddr;

  // Parse args
  if (argc != 2) {
    fprintf(stderr, "Incorrect number of arguments\n");
    exit(1);
  }
  else {
    filename = argv[1];
  }
  
  // Create socket
  if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    fprintf(stderr, "Could not create socket\n");
    exit(1);
  }

  // Setup server info
  memset(&servaddr, 0, sizeof(servaddr));

  servaddr.sin_family = AF_INET;
  servaddr.sin_port = htons(PORTNO);
  servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");

  ConnectionManager reliableConnection;
  if (reliableConnection.connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr), filename) == false) {
    fprintf(stderr, "Failed to establish reliable connection\n");
    close(sockfd);
    exit(1);
  }

  reliableConnection.receiveFile(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr), filename);

  // FIN Procedure
  // Send FINACK
  reliableConnection.sendFin(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr));  
  
  close(sockfd);
}
