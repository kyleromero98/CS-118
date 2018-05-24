//packet stuff
#include "constants.h"

class Offsets {
public:
  int seq_num = 0;
  int ack_num = sizeof(int);
  int cwnd = ack_num + sizeof(int);
  int syn_bit = cwnd + sizeof(short);
  int fin_bit = syn_bit + sizeof(bool);
  int data_size = fin_bit + sizeof(bool);
  int data = data_size + sizeof(int);
} Offsets;

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
  Packet(int seq_num, int ack_num, short cwnd) {
    header.seq_num = seq_num;
    header.ack_num = ack_num;
    header.cwnd = cwnd;
    header.syn_bit = false;
    header.fin_bit = false;
    header.data_size = 0;
    memset(data, 0, BUF_SIZE);
  }
  // If sending data
  Packet(int seq_num, int ack_num, short cwnd, const char* in_data, int data_size) {
    header.seq_num = seq_num;
    header.ack_num = ack_num;
    header.cwnd = cwnd;
    header.syn_bit = false;
    header.fin_bit = false;
    memset(data, 0, BUF_SIZE);
    if (data_size > BUF_SIZE) {
      fprintf(stderr, "Could not create packet\n");
      exit(-1);
    }
    else {
      header.data_size = data_size;
    }
    if (in_data != NULL) {
      if (strlen(in_data) > BUF_SIZE) {
	fprintf(stderr, "Could not create packet\n");
	exit(-1);
      }
      else {
	memcpy(data, in_data, data_size);
      }
    }
  }
  bool valid_seq() {
    return (header.seq_num >= 0 && header.seq_num <= 30720);
  }
  bool valid_ack() {
    return (header.seq_num >= 0 && header.seq_num <= 30720);
  }
  bool is_syn() {
    return header.syn_bit;
  }
  bool is_fin() {
    return header.fin_bit;
  }
  int data_size() {
    return header.data_size;
  }
  int packet_size() {
    return HEADER_SIZE + header.data_size;
  }
  // Accessor Functions
  int h_seq_num() {
    return header.seq_num;
  }
  int h_ack_num() {
    return header.ack_num;
  }
  short h_cwnd() {
    return header.cwnd;
  }
  bool h_syn() {
    return header.syn_bit;
  }
  bool h_fin() {
    return header.fin_bit;
  }
  int h_data_size() {
    return header.data_size;
  }
  char* p_data() {
    return data;
  }
  Header h_header() {
    return header;
  }
private:
  Header header;
  char data[BUF_SIZE];
};

class Con_Manager {
  
};