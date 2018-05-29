// connection manager stuff
// TODO:
//       Check if updateSeqnum/updateAcknum actually do what they're supposed to
//       Implement ACK sending 
//       Implement FIN procedure
//       Implement window stuff
//       Implement timeout stuff

#include <cstdlib>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <list>
#include <thread>

class ConnectionManager {
public:
  // established connection and exchanges SYN
  ConnectionManager() {
    seq_num = -1;
    ack_num = -1;
    cwnd = WINDOW_SIZE;
    timeout = RTO; // in ms
  }

  static bool compSeqnum (Packet first, Packet second) {
    return (first.h_seq_num() < second.h_seq_num());
  }

  void updateSeqnum (int bytes) {
    printf("Old Seqnum %d //", seq_num);
    seq_num = (seq_num + bytes) % (MAX_SEQNUM + 1);
    printf(" New Seqnum %d\n", seq_num);
  }

  void updateAcknum (int bytes) {
    printf("Old Acknum %d //", ack_num);
    ack_num = (ack_num + bytes) % (MAX_SEQNUM + 1);
    printf(" New Acknum %d\n", ack_num);
  }

  void sendAck (int sockfd, struct sockaddr *addr_info, size_t addr_len) {
    Packet s_packet(seq_num, ack_num, cwnd, false, false);
    sendPacket (sockfd, addr_info, addr_len, s_packet);
  }

  void sendFin (int sockfd, struct sockaddr *addr_info, size_t addr_len) {
    Packet f_packet(seq_num, ack_num, cwnd, false, true);
    sendPacket (sockfd, addr_info, addr_len, f_packet);
  }
  
  Packet receivePacket (int sockfd, struct sockaddr * addr_info, size_t addr_len) {
    char r_pstream[PACKET_SIZE];
    Packet* r_packet = NULL;
    
    int bytes_recv = recvfrom (sockfd, (char*) r_pstream, PACKET_SIZE, MSG_WAITALL, addr_info, (unsigned int*)&addr_len);
    
    if (bytes_recv < 0) {
      perror("Error with recvfrom");
      exit(-1);
    }
    r_packet = reinterpret_cast<Packet*>(r_pstream);

    logReceivedPacket(r_packet->h_seq_num());

    if (!(r_packet->is_syn() /* || r_packet->is_fin() */))
      updateAcknum(r_packet->packet_size());
	
    return (*r_packet);
  }

  bool sendPacket (int sockfd, const struct sockaddr *addr_info, size_t addr_len, Packet s_packet) {
    char* s_pstream = reinterpret_cast<char*> (&s_packet);
    
    if (sendto(sockfd, (const char*) s_pstream, s_packet.packet_size(), MSG_CONFIRM, addr_info, addr_len) < 0) {
      fprintf(stderr, "Failed sendto() in connect() of ConnectionManager\n");
      return false;
    }

    logSentPacket(s_packet.h_seq_num(), false, s_packet.h_syn(), s_packet.h_fin());

    updateSeqnum(s_packet.packet_size());

    return true;
  }
  
  bool sendFile (int sockfd, struct sockaddr *addr_info, size_t addr_len, char *filename) {
    int fd = -1;
    if ((fd = open(filename, O_RDONLY)) < 0) {
      return false;
    }
    else {
      char file_buf[BUF_SIZE];
      memset(file_buf, 0, BUF_SIZE);
      // Read in the file into a buffer in increments of BUF_SIZE
      while (true) {
	int bytes_read = read(fd, file_buf, BUF_SIZE);
	if (bytes_read == 0)
	  break;
	else if (bytes_read < 0) {
	  fprintf(stderr, "Error reading from the file\n");
	  exit(-1);
	}
	// After reading into the buffer, packetize the data
	Packet s_packet(seq_num, ack_num, cwnd, file_buf, bytes_read, false, false);
	char* s_pstream = reinterpret_cast<char*> (&s_packet);
	// Send the data
	int send_bytes = s_packet.packet_size();
	void *tracker = s_pstream;
        while (send_bytes > 0) {
	  int bytes_written =
	    sendto(sockfd, (const char*) tracker, send_bytes, MSG_CONFIRM, addr_info, addr_len);
	  if (bytes_written < 0) {
	    perror("Error writing to socket\n");
	    exit(-1);
	  }
	  send_bytes -= bytes_written;
	  tracker += bytes_written;
	}
	// Log the sent packet
	logSentPacket(s_packet.h_seq_num(), false, s_packet.is_syn(), s_packet.is_fin());
	// Update seqnum
	updateSeqnum(s_packet.packet_size());
      }
    }
    return true;
  }

  bool receiveFile (int sockfd, struct sockaddr *addr_info, size_t addr_len, char *filename) {
    int fd = open(filename, O_CREAT | O_WRONLY, 0666);

    char r_pstream[PACKET_SIZE];
    Packet* r_packet = NULL;
    
    int bytes_recv = 0;
    // Continue in the loop if there is more data to receive
    while ((bytes_recv = recvfrom (sockfd, (char*) r_pstream, PACKET_SIZE, MSG_WAITALL,
				   addr_info, (unsigned int*)&addr_len)) != 0)
      {
	// Error handling for recvfrom
	if (bytes_recv < 0) {
	  perror("Error with recvfrom");
	  exit(-1);
	}
	r_packet = reinterpret_cast<Packet*>(r_pstream);

	// Log received packet
	logReceivedPacket(r_packet->h_seq_num());

	// Check the FIN bit
	if (r_packet->is_fin()) break;
	
	// Add the received packet into the linked list if it was not expected
	if (r_packet->h_seq_num() != ack_num) {
	  p_list.push_back(*r_packet);
	}
	// Otherwise write the data to the file
	else {
	  // Update ACK and write the data to a file
	  write(fd, r_packet->p_data(), r_packet->h_data_size());
	  updateAcknum(r_packet->packet_size());
	}
	// Send ACK
	//sendAck(sockfd, addr_info, addr_len);
      }

    // If the list is not empty, add the out of order packets to the file
    if (!p_list.empty()) {
      p_list.sort(compSeqnum);
      std::list<Packet>::iterator it;
      for (it = p_list.begin(); it != p_list.end(); it++) {
	write(fd, (*it).p_data(), (*it).h_data_size());
	updateAcknum((*it).h_data_size());
      }
    }

    return true;
  }

  bool connect (int fd, struct sockaddr *addr_info, size_t addr_len, char *filename) {
    if (seq_num != -1 || ack_num != -1) {
      fprintf(stderr, "Failed to connect, sequence number already generated");
      return false;
    }
    
    // establish initial seq_num
    srand (time(NULL));
    seq_num = rand() % (MAX_SEQNUM + 1);

    // create dataless SYN packet
    Packet s_packet(seq_num, 0, cwnd, true, false);

    sendPacket(fd, addr_info, addr_len, s_packet);
    
    // Receive the SYNACK packet
    
    Packet r_packet = receivePacket(fd, addr_info, addr_len);

    if (r_packet.is_syn()) {
      ack_num = (r_packet.h_seq_num() + HEADER_SIZE) % (MAX_SEQNUM + 1);
      //printf("initial ack_num = %d\n", ack_num);
    } else {
      fprintf(stderr, "Received packet was not SYN");
      return false;
    }

    // Send the initial file request
    Packet req_packet(seq_num, ack_num, cwnd, filename, strlen(filename), false, false);
    sendPacket(fd, addr_info, addr_len, req_packet);

    return true;
  }
  
  bool listen(int sockfd, struct sockaddr *addr_info, size_t addr_len) {
    if (ack_num != -1 || seq_num != -1) {
      fprintf(stderr, "Failed to listen(), sequence number already generated");
      return false;
    }

    Packet r_packet = receivePacket(sockfd, addr_info, addr_len);
    
    if (r_packet.is_syn()) {
      srand (time(NULL) + 1);
      seq_num = rand() % (MAX_SEQNUM + 1);
      ack_num = (r_packet.h_seq_num() + 16) % (MAX_SEQNUM + 1);
    } else {
      fprintf(stderr, "Received packet was not SYN");
      return false;
    }
    return true;
  }

  bool accept(int sockfd, const struct sockaddr *addr_info, size_t addr_len) {
    if (seq_num == -1 && ack_num == -1) {
      fprintf(stderr, "Failed to accept, has not received connection request from client\n");
      return false;
    }
    
    // create dataless SYN packet
    Packet s_packet(seq_num, ack_num, cwnd, true, false);
    
    sendPacket(sockfd, addr_info, addr_len, s_packet);
    
    return true;
  }

  bool logSentPacket (int seq_num, bool isRetransmit, bool isSyn, bool isFin) {
    // invalid packet
    if (isSyn && isFin) {
      return false;
    }
    std::stringstream ss;
    ss << "Sending packet " << seq_num;

    // checking for special packets
    if (isRetransmit) {
      ss << " Retransmission";
    }
    if (isSyn) {
      ss << " SYN";
    }
    if (isFin) {
      ss << " FIN";
    }
    ss << "\n";
    std::string message = ss.str();
    fprintf(stderr, "%s", message.c_str());
    return true;
  }

  bool logReceivedPacket (int ack_num) {
    std::stringstream ss;
    ss << "Receiving packet " << ack_num << "\n";
    std::string message = ss.str();
    fprintf(stderr, "%s", message.c_str());
    return true;
  }
private:
  // current state variables
  int seq_num;
  int ack_num;
  int cwnd;
  int timeout;
  int window_base;
  bool con_idle;
  std::list<Packet> p_list;
};
