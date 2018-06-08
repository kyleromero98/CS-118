//packet stuff
#include "constants.h"

struct Header {
  int seq_num;
  int ack_num;
  short cwnd;
  bool syn_bit;
  bool fin_bit;
  int data_size;
};

class Packet {
public:
  // No data packet
  Packet(int seq_num, int ack_num, short cwnd, bool isSyn, bool isFin) {
    header.seq_num = seq_num;
    header.ack_num = ack_num;
    header.cwnd = cwnd;
    header.syn_bit = isSyn;
    header.fin_bit = isFin;
    header.data_size = 0;
    memset(data, 0, BUF_SIZE);
  }
  // If sending data
  Packet(int seq_num, int ack_num, short cwnd, const char* in_data, int data_size, bool isSyn, bool isFin) {
    header.seq_num = seq_num;
    header.ack_num = ack_num;
    header.cwnd = cwnd;
    header.syn_bit = isSyn;
    header.fin_bit = isFin;
    memset(data, 0, BUF_SIZE);
    if (data_size > BUF_SIZE) {
      fprintf(stderr, "Could not create packet, data size exceeded BUF_SIZE\n");
      exit(-1);
    } else {
      header.data_size = data_size;
    }
    if (in_data != NULL) {
      memcpy(data, in_data, data_size);
    } else {
      fprintf(stderr, "no data was sent\n");
      exit(1);
    }
  }
  bool valid_seq() const {
    return (header.seq_num >= 0 && header.seq_num <= 30720);
  }
  bool valid_ack() const {
    return (header.seq_num >= 0 && header.seq_num <= 30720);
  }
  bool is_syn() const {
    return header.syn_bit;
  }
  bool is_fin() const {
    return header.fin_bit;
  }
  int data_size() const {
    return header.data_size;
  }
  int packet_size() const {
    return HEADER_SIZE + header.data_size;
  }
  // Accessor Functions
  int h_seq_num() const {
    return header.seq_num;
  }
  int h_ack_num() const {
    return header.ack_num;
  }
  short h_cwnd() const {
    return header.cwnd;
  }
  char* p_data() {
    return data;
  }
  Header h_header() const {
    return header;
  }
  
  void dump() const {
    printf("THIS IS A PACKET DUMP\n");
    printf("seq_num: %d, ack_num: %d, pkt_size: %d\n", header.seq_num, header.ack_num, packet_size());
    printf("data_len: %d, data: \n", header.data_size);
    printf("%s\n", data);
  }
  
private:
  Header header;
  char data[BUF_SIZE];
};

