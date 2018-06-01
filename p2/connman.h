// connection manager stuff
// TODO:
//       Check if we need to be memset'ing the reinterpreted casts?
//       Change window to act more like selective repeat
//       Implement FIN ACKing of last packet
//       Implement timeout stuff

#include <cstdlib>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <list>
#include <thread>
#include <poll.h>

class ConnectionManager {
public:
  // established connection and exchanges SYN
  ConnectionManager(bool isServer) {
    seq_num = -1;
    ack_num = -1;
    cwnd = WINDOW_SIZE;
    timeout = RTO; // in ms
    is_server = isServer;
  }

  static bool compSeqnum (Packet first, Packet second) {
    return (first.h_seq_num() < second.h_seq_num());
  }

  void updateSeqnum (int bytes) {
    //printf("Old Seqnum %d //", seq_num);
    seq_num = (seq_num + bytes) % (MAX_SEQNUM + 1);
    //printf(" New Seqnum %d\n", seq_num);
  }

  void updateAcknum (int bytes) {
    //printf("Old Acknum %d //", ack_num);
    ack_num = (ack_num + bytes) % (MAX_SEQNUM + 1);
    //printf(" New Acknum %d\n", ack_num);
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
    // declare and set buffer
    char r_pstream[PACKET_SIZE];
    memset(r_pstream, 0, PACKET_SIZE);
    Packet* r_packet = NULL;
    
    int bytes_recv = recvfrom (sockfd, (char*) r_pstream, PACKET_SIZE, MSG_WAITALL, addr_info, (unsigned int*)&addr_len);
    
    if (bytes_recv < 0) {
      perror("Error with recvfrom");
      exit(-1);
    }
    r_packet = reinterpret_cast<Packet*>(r_pstream);
        
    // log the received packet
    logReceivedPacket(r_packet->h_seq_num());

    if (!(r_packet->is_syn() /* || r_packet->is_fin() */))
      updateAcknum(r_packet->packet_size());

    return (*r_packet);
  }

  bool sendPacket (int sockfd, const struct sockaddr *addr_info, size_t addr_len, Packet s_packet) {
    // cast to packet
    char* s_pstream = reinterpret_cast<char*> (&s_packet);

    // Send the data
    int send_bytes = s_packet.packet_size();
    int bytes_written = -1;
    void *tracker = s_pstream;

    // loop to make sure all data gets sent
    while (send_bytes > 0) {
      bytes_written = sendto(sockfd, (const char*) tracker, send_bytes, MSG_CONFIRM, addr_info, addr_len);
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

    return true;
  }
  
  bool sendFile (int sockfd, struct sockaddr *addr_info, size_t addr_len, char *filename) {
    int filefd = -1;
    int bytes_read = -1;

    if ((filefd = open(filename, O_RDONLY)) < 0) {
      perror("Failed to open file");
      return false;
    }
    else {
      char file_buf[BUF_SIZE];
      memset(file_buf, 0, BUF_SIZE);

      // set up the polling

      struct pollfd polldata[1];

      polldata[0].fd = sockfd;
      polldata[0].events = POLLIN | POLLHUP | POLLERR;
      polldata[0].revents = 0;

      int poll_status = 0;
      
      // Read in the file into a buffer in increments of BUF_SIZE
      while (bytes_read != 0) {
       
	// poll for ACKs from client
	if ((poll_status = poll(polldata, 1, 10)) < 0) {
	  perror("Polling from client error");
	  return false;
	} else if (poll_status >= 1) {
	  
	  // we got an event from the client
	  if (polldata[0].revents & POLLIN) {
	    // receive the packet
	    Packet r_packet = receivePacket(sockfd, addr_info, addr_len);

	    // check to see if the ACK matches any outstanding packets
	    std::list<Packet>::const_iterator iter = p_list.begin();
	    while (iter != p_list.end()){
	      //fprintf(stderr, "Received packet ack num: %d\n", r_packet.h_ack_num());
	      //fprintf(stderr, "Outstanding packet seq num: %d\n", iter->h_seq_num());
	      if (iter->h_seq_num() == r_packet.h_ack_num() - iter->packet_size()) {
		iter = p_list.erase(iter);
	      } else {
		++iter;
	      }
	      updateSize();
	    }
	  }
	}
	
	// done polling so now we check the timeouts
	// insert timeout code here
	
	// send out new packets until we hit the cwnd
	while (out_packet_size <= cwnd - PACKET_SIZE) {

	  // read next group of bytes from file
	  bytes_read = read(filefd, file_buf, BUF_SIZE);

	  // check for error while reading from file
	  if (bytes_read == 0) {
	    break; // reached EOF
	  } else if (bytes_read < 0) {
	    fprintf(stderr, "Error reading from the file\n");
	    exit(1);
	  }

	  // make and send the packets
	  Packet s_packet(seq_num, ack_num, cwnd, file_buf, bytes_read, false, false);
	  sendPacket(sockfd, (const struct sockaddr*)addr_info, addr_len, s_packet);

	  // add to list of outstanding packets
	  p_list.push_back(s_packet);
	  updateSize();
	}
      }

      // the file is done being sent, but we cant start FIN until all outstanding packets are ACKed
      // therefore, we keep polling
      while (out_packet_size != 0) {
	
	// poll for ACKs from client
	if ((poll_status = poll(polldata, 1, 10)) < 0) {
	  perror("Polling from client error");
	  return false;
	} else if (poll_status >= 1) {
	  
	  // we got an event from the client
	  if (polldata[0].revents & POLLIN) {
	    // receive the packet
	    Packet r_packet = receivePacket(sockfd, addr_info, addr_len);
	    
	    // check to see if the ACK matches any outstanding packets
	    std::list<Packet>::const_iterator iter = p_list.begin();
	    while (iter != p_list.end()){
	      if (iter->h_seq_num() == r_packet.h_ack_num() - iter->packet_size()) {
		iter = p_list.erase(iter);
	      } else {
		++iter;
	      }
	      updateSize();
	    }
	  }
	}
      }
    }
    return true;
  }

  bool receiveFile (int sockfd, struct sockaddr *addr_info, size_t addr_len, const char *filename) {
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
	  //fprintf(stderr, "out of order packet\n");
	}
	// Otherwise write the data to the file
	else {
	  // Update ACK and write the data to a file
	  write(fd, r_packet->p_data(), r_packet->h_data_size());
	  updateAcknum(r_packet->packet_size());
	}
	// Send ACK
	sendAck(sockfd, addr_info, addr_len);
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
  uint32_t cwnd;
  uint32_t out_packet_size;
  int timeout;
  int window_base;
  bool con_idle;
  bool is_server;
  std::list<Packet> p_list;

  void updateSize() {
    int counter = 0;
    std::list<Packet>::const_iterator iter = p_list.begin();
    while (iter != p_list.end()) {
      //check size of each packet and add it to counter
      counter += iter->packet_size();
      iter++;
    }
    out_packet_size = counter;
  }
};
