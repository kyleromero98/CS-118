// connection manager stuff
#include <cstdlib>

class ConnectionManager {
public:
  // established connection and exchanges SYN
  ConnectionManager() {
    seq_num = -1;
    ack_num = -1;
    cwnd = WINDOW_SIZE;
    timeout = RTO; // in ms
  }

  bool sendFile () {
    return false;
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
    char* s_pstream = reinterpret_cast<char*> (&s_packet);
    logSentPacket(s_packet.h_seq_num(), false, s_packet.h_syn(), s_packet.h_fin());
    if (sendto(fd, (const char*) s_pstream, PACKET_SIZE, MSG_CONFIRM, addr_info, addr_len) < 0) {
      fprintf(stderr, "Failed sendto() in connect() of ConnectionManager\n");
      seq_num = -1;
      return false;
    }
    seq_num += PACKET_SIZE;
    
    char r_pstream[PACKET_SIZE];
    Packet* r_packet = NULL;

    int bytes_recv = 0;
    bytes_recv = recvfrom (fd, (char*) r_pstream, PACKET_SIZE, MSG_WAITALL,
			   addr_info, (unsigned int*)&addr_len);
    
    if (bytes_recv < 0) {
      fprintf(stderr, "Failed recvfrom() in connect() of ConnectionManager\n");
      return false;
    }

    r_packet = reinterpret_cast<Packet*>(r_pstream);
    logReceivedPacket(r_packet->h_seq_num());

    if (r_packet->is_syn()) {
      ack_num = (r_packet->h_seq_num() + 16) % (MAX_SEQNUM + 1);
    } else {
      fprintf(stderr, "Received packet was not SYN");
      return false;
    }

    Packet req_packet(seq_num, ack_num, cwnd, filename, strlen(filename), false, false);
    char* req_pstream = reinterpret_cast<char*> (&req_packet);
    logSentPacket(req_packet.h_seq_num(), false, req_packet.h_syn(), req_packet.h_fin());
    if (sendto(fd, (const char*) req_pstream, PACKET_SIZE, MSG_CONFIRM, addr_info, addr_len) < 0) {
      fprintf(stderr, "Failed sendto() in connect() of ConnectionManager\n");
      return false;
    }
    seq_num += PACKET_SIZE;
    return true;
  }

  bool listen(int fd, struct sockaddr *addr_info, size_t addr_len) {
    if (ack_num != -1 || seq_num != -1) {
      fprintf(stderr, "Failed to listen(), sequence number already generated");
      return false;
    }
    
    char r_pstream[PACKET_SIZE];
    Packet* r_packet = NULL;

    int bytes_recv = 0;
    bytes_recv = recvfrom (fd, (char*) r_pstream, PACKET_SIZE, MSG_WAITALL, addr_info, (unsigned int*)&addr_len);
    if (bytes_recv < 0) {
      fprintf(stderr, "Failed recvfrom() in listen() of ConnectionManager\n");
      return false;
    }

    r_packet = reinterpret_cast<Packet*>(r_pstream);
    logReceivedPacket(r_packet->h_seq_num());

    if (r_packet->is_syn()) {
      srand (time(NULL) + 1);
      seq_num = rand() % (MAX_SEQNUM + 1);
      ack_num = (r_packet->h_seq_num() + 16) % (MAX_SEQNUM + 1);
    } else {
      fprintf(stderr, "Received packet was not SYN");
      return false;
    }
    return true;
  }

  bool accept(int fd, const struct sockaddr *addr_info, size_t addr_len) {
    if (seq_num == -1 && ack_num == -1) {
      fprintf(stderr, "Failed to accept, has not received connection request from client\n");
      return false;
    }
    
    // create dataless SYN packet
    Packet s_packet(seq_num, ack_num, cwnd, true, false);
    char* s_pstream = reinterpret_cast<char*> (&s_packet);
    logSentPacket(s_packet.h_seq_num(), false, s_packet.h_syn(), s_packet.h_fin());
    if (sendto(fd, (const char*) s_pstream, PACKET_SIZE, MSG_CONFIRM, addr_info, addr_len) < 0) {
      fprintf(stderr, "Failed sendto() in connect() of ConnectionManager\n");
      return false;
    }
    seq_num += PACKET_SIZE;
    return true;
  }
  
private:
  // current state variables
  int seq_num;
  int ack_num;
  int cwnd;
  int timeout;

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
      ss << "FIN";
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
};
