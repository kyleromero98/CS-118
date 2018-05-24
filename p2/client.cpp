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
  std::string filename;
  char buf[BUF_SIZE];
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
  
  // Create packet
  Packet s_packet(0, 0, WINDOW_SIZE, filename.data(), filename.length());
  Header header_cp = s_packet.h_header();
  
  // Create the stream to send
  char* s_pstream = reinterpret_cast<char*> (&s_packet);

  // Send the packet stream
  sendto(sockfd, (const char *) s_pstream, PACKET_SIZE,
	 MSG_CONFIRM, (const struct sockaddr*) &servaddr,
	 sizeof(servaddr));

  // Packet stream to receive
  char r_pstream[PACKET_SIZE];
  Packet* r_packet = NULL;
  
  // Receive the ACK
  int bytes_recv;
  unsigned int serv_addr_len;
  bytes_recv = recvfrom(sockfd, (char *) r_pstream, PACKET_SIZE,
			MSG_WAITALL, (struct sockaddr*) &servaddr,
			&serv_addr_len);

  printf("Received ACK\n");

  // Interpret the ACK
  r_packet = reinterpret_cast<Packet*> (r_pstream);
  printf("Received Packet:\n");
  printf("seq_num: %d\nack_num: %d\n", r_packet->h_seq_num(), r_packet->h_ack_num());
  
  close(sockfd);
  
}
