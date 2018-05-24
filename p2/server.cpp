#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <fstream>

#include "packet.h"
#include "consants.h"


int main (int argc, char* argv[]) {
  int sockfd;
  std::string hostname;
  int portno;
  char buf[BUF_SIZE];
  struct sockaddr_in servaddr, cliaddr;

  // Parse args
  if (argc != 3) {
    fprintf(stderr, "Incorrect number of arguments\n");
    exit(-1);
  }
  else {
    hostname = argv[1];
    portno = atoi(argv[2]);
  }

  printf("hostname: %s\n", hostname.data());
  
  // Create socket
  if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    perror("Could not create socket");
    exit(-1);
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
      exit(-1);
    }
  
  // Packet stream to receive
  char r_pstream[PACKET_SIZE];
  Packet* r_packet = NULL;
  
  // Receive the packet
  int bytes_recv;
  unsigned int cli_addr_len = sizeof(cliaddr);
  bytes_recv = recvfrom(sockfd, (char *) r_pstream, PACKET_SIZE,
			MSG_WAITALL, (struct sockaddr*) &cliaddr,
			&cli_addr_len);

  // Interpret the packet
  r_packet = reinterpret_cast<Packet*> (r_pstream);
  printf("Received Packet:\n");
  printf("seq_num: %d\nack_num: %d\n", r_packet->h_seq_num(), r_packet->h_ack_num());
  printf("data: %s\n", r_packet->p_data());
  // Create packet
  Packet s_packet(r_packet->h_seq_num() + 1, r_packet->h_ack_num() + 1, WINDOW_SIZE, NULL, 0);

  // Create the stream to send
  char* s_pstream = reinterpret_cast<char*> (&s_packet);

  // Send the packet stream
  if ((sendto(sockfd, (const char *) s_pstream, PACKET_SIZE,
	      MSG_CONFIRM, (const struct sockaddr*) &cliaddr,
	      cli_addr_len)) < 0) {
    perror("UDP soiled it\n");
    exit(-1);
  }

}
