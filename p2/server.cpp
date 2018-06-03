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

#include "packet.h"
#include "constants.h"
#include "connman.h"

int main (int argc, char* argv[]) {
  int sockfd;
  std::string hostname;
  int portno;
  struct sockaddr_in servaddr, cliaddr;

  // Parse args
  if (argc != 3) {
    fprintf(stderr, "Incorrect number of arguments\n");
    exit(1);
  }
  else {
    hostname = argv[1];
    portno = atoi(argv[2]);
  }
  
  // Create socket
  if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    perror("Could not create socket");
    exit(1);
  }

  // Setup server and client info
  memset(&servaddr, 0, sizeof(servaddr));
  memset(&cliaddr, 0, sizeof(cliaddr));
  
  servaddr.sin_family = AF_INET;
  servaddr.sin_port = htons(portno);
  servaddr.sin_addr.s_addr = inet_addr(hostname.data());
  
  // Bind the socket
  if (bind(sockfd, (const struct sockaddr*) &servaddr,
	   sizeof(servaddr)) < 0)
    {
      perror("Could not bind the socket");
      exit(1);
    }

  ConnectionManager reliableConnection(true);
  if (!reliableConnection.listen(sockfd, (struct sockaddr *) &cliaddr, sizeof(cliaddr))) {
    fprintf(stderr, "Could not listen on socket for client\n");
    exit(1);
  }

  if (!reliableConnection.accept(sockfd, (struct sockaddr *) &cliaddr, sizeof(cliaddr))) {
    fprintf(stderr, "Failed to establish connection\n");
    exit(1);
  }
  
  // Listen for the initial file request

  // Send file
  if (!reliableConnection.sendFile(sockfd, (struct sockaddr *) &cliaddr, sizeof(cliaddr))) {
    fprintf(stderr, "Failed to send file\n");
  }

  // FIN procedure
  reliableConnection.serv_fin(sockfd, (struct sockaddr *) &cliaddr, sizeof(cliaddr));
  reliableConnection.wait_fin(sockfd, (struct sockaddr *) &cliaddr, sizeof(cliaddr));
  close(sockfd);
}
