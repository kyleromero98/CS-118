CC = g++
CFLAGS = -g -Wall -Wextra

default: server client

server: server.cpp
	$(CC) $(CFLAGS) server.cpp -o server

client: client.cpp
	$(CC) $(CFLAGS) client.cpp -o client

dist:
	tar -z -cf 204747283.904771873.tar.gz server.cpp client.cpp Makefile

clean:
	rm -rf *.o *.so *.tar.gz server client server.dSYM client.dSYM