# Compiler and flags
CC = gcc
CFLAGS = -g -Wall -std=gnu99
LIBS = 

# Object files
CLIENT_OBJS = networks.o gethostbyname.o pollLib.o safeUtil.o sendreceive.o handle_table.o makePDU.o shared.o
SERVER_OBJS = networks.o gethostbyname.o pollLib.o safeUtil.o sendreceive.o handle_table.o shared.o

# Targets
all: cclient server

cclient: cclient.o $(CLIENT_OBJS)
	$(CC) $(CFLAGS) -o cclient cclient.o $(CLIENT_OBJS) $(LIBS)

server: server.o $(SERVER_OBJS)
	$(CC) $(CFLAGS) -o server server.o $(SERVER_OBJS) $(LIBS)

# Pattern rule for compiling .c files to .o
.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

# Clean object files only
cleano:
	rm -f *.o

# Clean everything
clean:
	rm -f server cclient *.o
