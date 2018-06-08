// the reliable data transfer functions for both the client and server of our CS 118 P2

#include <cstdlib>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <list>
#include <poll.h>
#include <chrono>
#include <iostream>
#include <ctime>
#include <iomanip>
#include <string>

class ConnectionManager {
public:

  // setting up stuff for timing
  typedef std::chrono::high_resolution_clock Time;
  typedef std::chrono::milliseconds ms;
  typedef std::chrono::duration<float> fsec;
  
  // packet data struct for storing packet data
  struct packet_data {
    Packet *packet;
    Time::time_point time_sent;
    Time::time_point time_retrans;
  };
    
  // established connection and exchanges SYN
  ConnectionManager(bool isServer) {
    seq_num = -1;
    ack_num = -1;
    cwnd = WINDOW_SIZE;
    timeout = RTO; // in ms
    is_server = isServer;
  }

  // compares the the timing of two packets
  // returns true if second arrived later than the first
  static bool compTime (packet_data first, packet_data second) {
    return (second.time_sent > first.time_sent);
  }

  // compares the sequence numbers of two packets
  // returns true if the second seq number is greater than the first
  static bool compSeqs (packet_data first, packet_data second) {
    int diff = (second.packet->h_seq_num()) - (first.packet->h_seq_num());
    // if difference too great, account for overflow by reversing return value
    if (std::abs(diff) > (MAX_SEQNUM / 2)) {
      if (diff > 0) {
	return false;
      } else {
	return true;
      }
    } else {
      if (diff > 0) {
	return true;
      } else {
	return false;
      }
    }
    return true;
  }

  // updates the sequence number by bytes, wraps around
  void updateSeqnum (int bytes) {
    seq_num = (seq_num + bytes) % (MAX_SEQNUM + 1);
  }

  // updates the ackNum by bytes, wraps around
  void updateAcknum (int bytes) {
    ack_num = (ack_num + bytes) % (MAX_SEQNUM + 1);
  }

  // sends an ACK packet
  void sendAck (int sockfd, struct sockaddr *addr_info, size_t addr_len, int p_ack_num) {
    if (p_ack_num == -1) {
      Packet *s_packet = new Packet(seq_num, ack_num, cwnd, false, false);
      sendPacket (sockfd, addr_info, addr_len, s_packet, false);
    } else {
      Packet *s_packet = new Packet(seq_num, p_ack_num, cwnd, false, false);
      sendPacket (sockfd, addr_info, addr_len, s_packet, false);
    }
  }

  // client FIN procedure
  void cli_fin (int sockfd, struct sockaddr *addr_info, size_t addr_len) {
    Packet *f_packet = new Packet(seq_num, ack_num, cwnd, false, true);
    sendPacket (sockfd, addr_info, addr_len, f_packet, false);
  }

  // server FIN procedure
  void serv_fin (int sockfd, struct sockaddr *addr_info, size_t addr_len) {
    // Create FIN packet to send
    Packet *f_packet = new Packet(seq_num, ack_num, cwnd, false, true);

    // set up the polling
      
    struct pollfd polldata[1];

    polldata[0].fd = sockfd;
    polldata[0].events = POLLIN | POLLHUP | POLLERR;
    polldata[0].revents = 0;
    
    int poll_status = 0;

    bool recv_ack = false;
    bool is_retransmit = false;
    
    // poll for ACKs from client
    
    while (true) {
      // Send the initial packet
      sendPacket(sockfd, addr_info, addr_len, f_packet, is_retransmit);

      // Wait for one RTO
      
      if ((poll_status = poll(polldata, 1, RTO)) < 0) {
	perror("Polling from client error");
	exit(1);
      }
      else if (poll_status >= 1) {
	// we got an event from the client
	if (polldata[0].revents & POLLIN) {
	  // receive the packet
	  Packet* rp_packet = receivePacket(sockfd, addr_info, addr_len);
	  
	  Packet r_packet = (*rp_packet);

	  // checking to see if the received packet is the FINACK
	  if ((r_packet.is_fin()) &&
	      ((f_packet->h_seq_num() ==
	       r_packet.h_ack_num() - f_packet->packet_size() ||
	       f_packet->h_seq_num() ==
		(r_packet.h_ack_num() - f_packet->packet_size()) + MAX_SEQNUM + 1))) {
	    updateAcknum(r_packet.packet_size());
	    sendAck(sockfd, addr_info, addr_len, -1);
	    recv_ack = true;
	    free(f_packet);
	  }
	}
      }
      
      // If the ack was received, stop polling
      if (recv_ack) {
	break;
      }
      else {
	is_retransmit = true;
      }
    }
  }

  // wait procedure after the server FIN procedure
  void wait_fin (int sockfd, struct sockaddr *addr_info, size_t addr_len) {
   
    // set up the polling
      
    struct pollfd polldata[1];

    polldata[0].fd = sockfd;
    polldata[0].events = POLLIN | POLLHUP | POLLERR;
    polldata[0].revents = 0;
    
    int poll_status = 0;
    
    // poll for extra packets from client
    
    // Wait for one RTO
      
    if ((poll_status = poll(polldata, 1, 2 * RTO)) < 0) {
      perror("Polling from client error");
      exit(1);
    }
    else if (poll_status >= 1) {
      // we got an event from the client
      if (polldata[0].revents & POLLIN) {
	// receive the packet
	Packet* rp_packet = receivePacket(sockfd, addr_info, addr_len);
	  
	Packet r_packet = (*rp_packet);
	
	// checking to see if the received packet is expected
	if (ack_num == r_packet.h_seq_num()) {
	  updateAcknum(r_packet.packet_size());
	}
      }
    }
  }
  
  // will receive a packet
  Packet* receivePacket (int sockfd, struct sockaddr * addr_info, size_t addr_len) {

    // declare and init buffer
    char r_pstream[PACKET_SIZE];
    memset(r_pstream, 0, PACKET_SIZE);

    // initialize receiving packet
    Packet* r_packet = NULL;
    
    int bytes_recv = recvfrom (sockfd, r_pstream, PACKET_SIZE, MSG_WAITALL, addr_info, (unsigned int*)&addr_len);
    
    if (bytes_recv < 0) {
      perror("Error with recvfrom() in receivePacket()");
      exit(1);
    }

    // reinterpret the received bytes
    r_packet = reinterpret_cast<Packet*>(r_pstream);
        
    // log the received packet
    logReceivedPacket(r_packet->h_seq_num());

    return r_packet;
  }

  // will send a packet
  bool sendPacket (int sockfd, struct sockaddr *addr_info, size_t addr_len, Packet *s_packet, bool isRetransmit) {
    // cast to packet
    char* s_pstream = reinterpret_cast<char*> (s_packet);

    // initialize how much we need to send and the bytes written
    int send_bytes = s_packet->packet_size();
    int bytes_written = -1;

    // pointer to keep track of where we are
    char *tracker = s_pstream;

    // loop to make sure all data gets sent
    while (send_bytes > 0) {
      // send the data
      bytes_written = sendto(sockfd, (const char*) tracker, send_bytes, MSG_CONFIRM, addr_info, addr_len);

      // check for errors
      if (bytes_written < 0) {
	perror("Error writing to socket\n");
	exit(-1);
      }
      if (bytes_written == 0) {
	printf("0 bytes written\n");
      }
      // debugging thing
      if (bytes_written != send_bytes) {
	fprintf(stderr, "THIS MESSAGE MEANS YOU NEED THE THING\n");
      }
      
      // update meta
      send_bytes -= bytes_written;
      tracker += bytes_written;
    }
        
    // Log the sent packet
    logSentPacket(s_packet->h_seq_num(), isRetransmit, s_packet->is_syn(), s_packet->is_fin());

    // Update seqnum
    if (!isRetransmit) {
      updateSeqnum(s_packet->packet_size());
    }
    
    return true;
  }
  
  bool sendFile (int sockfd, struct sockaddr *addr_info, size_t addr_len) {
    // set this to initial values
    int filefd = -1;
    int bytes_read = -1;
    Packet *rp_packet = NULL;
    cwnd_base = seq_num;
 
    // open the file that we need to read
    if ((filefd = open(filename, O_RDONLY)) < 0) {
      perror("Failed to open file");
      return false;
    } else {
      // init our data buffer
      char file_buf[BUF_SIZE];
      memset(file_buf, 0, BUF_SIZE);
      
      // set up the polling
      
      struct pollfd polldata[1];

      polldata[0].fd = sockfd;
      polldata[0].events = POLLIN | POLLHUP | POLLERR;
      polldata[0].revents = 0;

      int poll_status = 0;
      
      // Read in the file into a buffer in increments of BUF_SIZE
      while (true) {
	// poll for ACKs from client
	// polling for 10 ms and from 1 fd
	if ((poll_status = poll(polldata, 1, 10)) < 0) {
	  perror("Polling from client error");
	  return false;
	} else if (poll_status >= 1) {
	  // we got an event from the client
	  if (polldata[0].revents & POLLIN) {
	    // receive the packet
	    rp_packet = receivePacket(sockfd, addr_info, addr_len);
	    Packet r_packet = (*rp_packet);
	    // check to see if the ACK matches any outstanding packets
	    std::list<packet_data>::const_iterator iter = p_list.begin();
	    while (iter != p_list.end()){
	      // checking the sequence numbers
	      if (iter->packet->h_seq_num() == r_packet.h_ack_num() - iter->packet->packet_size()
		  || iter->packet->h_seq_num() == (r_packet.h_ack_num() - iter->packet->packet_size()) + MAX_SEQNUM + 1) {
		updateAcknum(iter->packet->packet_size());
		free(iter->packet);
		iter = p_list.erase(iter);
	      } else {
		//fprintf(stderr, "received packet: %d\n", r_packet.h_ack_num() - iter->packet->packet_size());
		//fprintf(stderr, "packet in p_list: %d\n", iter->packet->h_seq_num());
		++iter;
	      }
	    }
	  }
	}
	
	//now we check the timeouts by iterating through outstanding packets
	std::list<packet_data>::const_iterator iter = p_list.begin();
	while (iter != p_list.end()){
	  // get current time
	  auto current_time = Time::now();

	  fsec sec_duration = current_time - iter->time_retrans;
	  ms ms_duration = std::chrono::duration_cast<ms>(sec_duration);

	  // printf("ms_duration: %lu\n", ms_duration.count());
	  if (ms_duration.count() > (timeout)) {
	    //printf("Timeout has occurred.\n");
	    sendPacket(sockfd, addr_info, addr_len, iter->packet, true);	    
	    packet_data data;
	    data.packet = iter->packet;
	    data.time_retrans = Time::now();
	    data.time_sent = iter->time_sent;
	    iter = p_list.erase(iter);
	    p_list.push_back(data);

	    fprintf(stderr, "p_list size: %lu\n", p_list.size());
	  } else {
	    ++iter;
	  }
	}

	// send out new packets until fill up the cwnd
	while (p_list.empty()
	       || seq_num < ((getCwndBase() + cwnd) % (MAX_SEQNUM + 1))
	       || (((getCwndBase() + cwnd) != (getCwndBase() + cwnd) % (MAX_SEQNUM + 1)) &&
		   seq_num >= getCwndBase())) {       

	  // read next group of bytes from file
	  memset(file_buf, 0, BUF_SIZE);
	  bytes_read = read(filefd, file_buf, BUF_SIZE);

	  // check for error while reading from file
	  if (bytes_read == 0) {
	    break; // reached EOF
	  } else if (bytes_read < 0) {
	    fprintf(stderr, "Error reading from the file\n");
	    exit(1);
	  }

	  // initialize the packet
	  Packet *s_packet = new Packet(seq_num, ack_num, cwnd, file_buf, bytes_read, false, false);
	  packet_data data;
	  data.packet = s_packet;
	  data.time_sent = Time::now();
	  data.time_retrans = Time::now();

	  // send the packet
	  sendPacket(sockfd, addr_info, addr_len, s_packet, false);
	  // add to list of outstanding packets
	  p_list.push_back(data);
	  fprintf(stderr, "p_list size: %lu\n", p_list.size());
	}
	
	if (bytes_read == 0 && p_list.empty()) {
	  break;
	}
      }
    }
    return true;
  }

  bool receiveFile (int sockfd, struct sockaddr *addr_info, size_t addr_len, const char *filename) {

    if (p_list.size() != 0) {
      fprintf(stderr, "Error: p_list was not empty when we started receiving file\n");
      return false;
    }
    
    // opens the file for writing to 'received.data'
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
	//printf("This is the ACK\n");
	// set the stream to a packet
	r_packet = reinterpret_cast<Packet*>(r_pstream);
	Packet* tmp_packet = new Packet(r_packet->h_seq_num(), r_packet->h_ack_num(), r_packet->h_cwnd(),
					r_packet->p_data(), r_packet->data_size(), r_packet->is_syn(),
					r_packet->is_fin());
	
	// Log received packet
	logReceivedPacket(r_packet->h_seq_num());

	// Check the FIN bit
	if (r_packet->is_fin()) {
	  updateAcknum(r_packet->packet_size());
	  break;
	}

	sendAck(sockfd, addr_info, addr_len, ((r_packet->h_seq_num()) + (r_packet->packet_size())) % (MAX_SEQNUM + 1));

	// the packet was the one we expected
	if (r_packet->h_seq_num() == ack_num) {
	  //update the ack num
	  updateAcknum(r_packet->packet_size());
	  fprintf(stderr, "updating ack_num by: %d\n", r_packet->packet_size());
	  
	  // append the data to the file
	  if (!appendToFile(fd, r_packet->p_data(), r_packet->data_size())) {
	    return false;
	  }

	  // sort the list according to the length of it
	  p_list.sort(compSeqs);
	  
	  // append other data to file that might be there
	  std::list<packet_data>::iterator it = p_list.begin();
	  while (!p_list.empty() && it->packet->h_seq_num() == ack_num) {
	    // matched front of list so update ACK
	    // append to file
	    if (!appendToFile(fd, it->packet->p_data(), it->packet->data_size())) {
	      return false;
	    }

	    updateAcknum(it->packet->packet_size());
	    free(it->packet);
	    it = p_list.erase(it);
	  }
	} else {
	  // add out of order packet to list
	  packet_data data;
	  data.packet = tmp_packet;
	  data.time_sent = Time::now();
	  
	  p_list.push_back(data);
	}
      }

    // If the list is not empty, add the out of order packets to the file
    if (!p_list.empty()) {
      p_list.sort(compSeqs);
      std::list<packet_data>::iterator it;
      for (it = p_list.begin(); it != p_list.end(); it++) {
	if (!appendToFile(fd, it->packet->p_data(), it->packet->data_size())) {
	  return false;
	}
	updateAcknum(it->packet->data_size());
      }
    }
    return true;
  }

  bool appendToFile(int fd, char* data, int bytes) {
    if (write(fd, data, bytes) < 0) {
      perror("Error writing to received.data");
      return false;
    }
    return true;
  }

  bool connect (int fd, struct sockaddr *addr_info, size_t addr_len, char *filename) {
    if (seq_num != -1 || ack_num != -1) {
      fprintf(stderr, "Failed to connect, sequence number already generated");
      return false;
    }
    
    // establish initial seq_num
    seq_num = 0;

    // create dataless SYN packet
    Packet *s_packet = new Packet(seq_num, 0, cwnd, true, false);

    // Send the packet and wait for ACK
  
    // set up the polling
      
    struct pollfd polldata[1];

    polldata[0].fd = fd;
    polldata[0].events = POLLIN | POLLHUP | POLLERR;
    polldata[0].revents = 0;
    
    int poll_status = 0;

    bool recv_ack = false;
    bool is_retransmit = false;
    
    // poll for ACKs from client
    
    while (true) {
      // Send the initial packet
      sendPacket(fd, addr_info, addr_len, s_packet, is_retransmit);

      // Get time

      // Wait for one RTO
      
      if ((poll_status = poll(polldata, 1, RTO)) < 0) {
	perror("Polling from client error");
	return false;
      }
      else if (poll_status >= 1) {
	// we got an event from the client
	if (polldata[0].revents & POLLIN) {
	  // receive the packet
	  Packet* rp_packet = receivePacket(fd, addr_info, addr_len);


	  Packet r_packet = (*rp_packet);

	  // checking to see if the received packet is the ACK
	  if ((s_packet->h_seq_num() ==
	       r_packet.h_ack_num() - s_packet->packet_size() ||
	       s_packet->h_seq_num() ==
	       (r_packet.h_ack_num() - s_packet->packet_size()) + MAX_SEQNUM + 1)) {

	    updateAcknum(r_packet.packet_size());
	    recv_ack = true;
	    free(s_packet);
	  }
	  if (r_packet.is_syn()) {
	    recv_ack = true;
	    ack_num = (r_packet.h_seq_num() + r_packet.packet_size()) % (MAX_SEQNUM + 1);
	  }
	}
      }
      
      // If the ack was received, stop polling
      if (recv_ack) {
	break;
      }
      else {
	is_retransmit = true;
      }
    }
      
    // Send the initial file request
    Packet *req_packet = new Packet(seq_num, ack_num, cwnd, filename, strlen(filename), false, false);
    sendPacket(fd, addr_info, addr_len, req_packet, false);
    return true;
  }
  
  bool listen(int sockfd, struct sockaddr *addr_info, size_t addr_len) {
    if (ack_num != -1 || seq_num != -1) {
      perror("Failed to listen(), sequence number already generated");
      return false;
    }

    Packet *rp_packet = receivePacket(sockfd, addr_info, addr_len);
    Packet r_packet = (*rp_packet);
    
    seq_num = 0;
    ack_num = ((r_packet.h_seq_num() + HEADER_SIZE) % (MAX_SEQNUM + 1));
    
    if (!(r_packet.is_syn())) {
      perror("Received packet was not SYN");
      return false;
    }
    
    return true;
  }

  bool accept(int sockfd, struct sockaddr *addr_info, size_t addr_len) {
    if (seq_num == -1 && ack_num == -1) {
      fprintf(stderr, "Failed to accept, has not received connection request from client\n");
      return false;
    }
    // create dataless SYNACK packet
    Packet *s_packet = new Packet(seq_num, ack_num, cwnd, true, false);

    // Send the SYNACK, wait for initial file request
    
    // set up the polling
      
    struct pollfd polldata[1];

    polldata[0].fd = sockfd;
    polldata[0].events = POLLIN | POLLHUP | POLLERR;
    polldata[0].revents = 0;
    
    int poll_status = 0;

    bool recv_ack = false;
    bool is_retransmit = false;
    
    // poll for ACKs from client
    
    while (true) {
      // Send the initial packet
      sendPacket(sockfd, addr_info, addr_len, s_packet, is_retransmit);

      // Wait for one RTO
      
      if ((poll_status = poll(polldata, 1, RTO)) < 0) {
	perror("Polling from client error");
	return false;
      }
      else if (poll_status >= 1) {
	// we got an event from the client
	if (polldata[0].revents & POLLIN) {
	  // receive the packet
	  Packet* rp_packet = receivePacket(sockfd, addr_info, addr_len);


	  Packet r_packet = (*rp_packet);

	  // checking to see if the received packet is the ACK
	  if ((s_packet->h_seq_num() ==
	       r_packet.h_ack_num() - s_packet->packet_size() ||
	       s_packet->h_seq_num() ==
	       (r_packet.h_ack_num() - s_packet->packet_size()) + MAX_SEQNUM + 1)) {
	    //printf("prev ack_num: %d\n", ack_num);
	    //ack_num = (r_packet.h_seq_num() + r_packet.packet_size()) % (MAX_SEQNUM + 1);
	    updateAcknum(r_packet.packet_size());
	    //printf("updated ack_num: %d\n", ack_num);
	    recv_ack = true;
	    filename = r_packet.p_data();
	    free(s_packet);
	  }
	}
      }
      
      // If the ack was received, stop polling
      if (recv_ack) {
	break;
      }
      else {
	is_retransmit = true;
      }
    }
    
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
  // curent congestion window size
  int cwnd;
  int cwnd_base;
  // current timeout; always 500ms
  int timeout;
  // the last unACKed packet seq_num
  bool con_idle;
  // just to tell if we are server or not
  bool is_server;
  // requested filename
  char* filename;
  
  // stores unACKed packets in case we need to retransmit
  std::list<packet_data> p_list;

  int getCwndBase() {
    p_list.sort(compTime);
    return p_list.front().packet->h_seq_num();
  }
};
