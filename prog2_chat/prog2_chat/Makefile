# Makefile for CPE464 TCP chat program
# Written by Joshua (based on Hugh Smith's 2019 template)

# Compiler and flags
CC = gcc
CFLAGS = -g -Wall -std=gnu99
LIBS = 

# Object files
CLIENT_OBJS = networks.o gethostbyname.o pollLib.o safeUtil.o sendreceive.o handle_table.o makePDU.o
SERVER_OBJS = networks.o gethostbyname.o pollLib.o safeUtil.o sendreceive.o handle_table.o 

# Targets
all: cclient server2

cclient: cclient.c $(CLIENT_OBJS)
	$(CC) $(CFLAGS) -o cclient cclient.c $(CLIENT_OBJS) $(LIBS)

server2: server2.c $(SERVER_OBJS)
	$(CC) $(CFLAGS) -o server2 server2.c $(SERVER_OBJS) $(LIBS)

# Pattern rule for compiling .c files to .o
.c.o:
	$(CC) $(CFLAGS) -c $< -o $@ 

# Clean object files only
cleano:
	rm -f *.o

# Clean everything
clean:
	rm -f server2 cclient *.o
