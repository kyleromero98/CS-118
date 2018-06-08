#Makefile for our CS118 P2

CC = g++
CFLAGS = -g -Wpointer-arith -Wsign-compare -Wall -Wextra -std=c++11 -pthread

default: client server cc_server

server:
	$(CC) $(CFLAGS) server.cpp -o $@

cc_server:
	$(CC) $(CFLAGS) cc_server.cpp -o $@

client:
	$(CC) $(CFLAGS) client.cpp -o $@

dist:
	tar -z -cf 204747283.904771873.tar.gz server.cpp client.cpp constants.h Makefile packet.h connman.h README report.pdf cc_server.cpp connman_cc.h

clean:
	rm -f *.o *.so *.tar.gz server client cc_server received.data

pls:
	make clean && make
