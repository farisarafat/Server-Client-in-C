FLAGS=-g -o
CC=gcc

all: server netclient

netclient: netclient.o libnetfiles.o
	$(CC) libnetfiles.o netclient.o -o netclient -lm

server: netfileserver.c libnetfiles.h
	$(CC) $(FLAGS) server netfileserver.c -pthread

libnetfiles.o:
	$(CC) -g -O -c libnetfiles.c 

clean:
	$(RM) test client server netclient *.o *.exe
