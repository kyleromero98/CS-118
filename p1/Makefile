#Makefile for our server program

CC = gcc
CFLAGS = -g -Wall -Wextra

default: server

server: server.c
	$(CC) $(CFLAGS) server.c -o $@

dist:
	tar -z -cf 204747283.tar.gz server.c Makefile README 404.html report.pdf

clean:
	rm -f *.o *.so *.tar.gz server
