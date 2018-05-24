// defined constants

#define PACKET_SIZE 1024
#define HEADER_SIZE 16
#define BUF_SIZE PACKET_SIZE - HEADER_SIZE
#define PORTNO 1234
#define MAX_SEQNUM 30720
#define WINDOW_SIZE 5120
#define RTO 500 // In milliseconds
#define MAX_FILENAME 32