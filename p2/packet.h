#include <stdint.h>

struct packet {
  uint16_t seq_num;
  uint16_t ack_num;
  uint8_t flags;
  uint16_t cwnd;
  uint8_t data [1016];
};