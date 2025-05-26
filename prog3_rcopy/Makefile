# Makefile for CPE464 tcp and udp test code
# updated by Hugh Smith - April 2023

# all target makes UDP test code
# tcpAll target makes the TCP test code


CC= gcc
CFLAGS= -g -Wall
LIBS = 

OBJS = networks.o gethostbyname.o pollLib.o safeUtil.o helperFunctions.o windowing.o 

#uncomment next two lines if your using sendtoErr() library
LIBS += libcpe464.2.21.a -lstdc++ -ldl
CFLAGS += -D__LIBCPE464_ -std=gnu99
VFLAGS= --tool=memcheck --leak-check=full --track-origins=yes --show-reachable=yes --trace-children=yes
ERR_RATE ?= 0.1


all: udpAll

udpAll: rcopy server
tcpAll: myClient myServer

rcopy: rcopy.c $(OBJS) 
	$(CC) $(CFLAGS) -o rcopy rcopy.c $(OBJS) $(LIBS)

server: server.c $(OBJS) 
	$(CC) $(CFLAGS) -o server server.c  $(OBJS) $(LIBS)

myClient: myClient.c $(OBJS)
	$(CC) $(CFLAGS) -o myClient myClient.c  $(OBJS) $(LIBS)

myServer: myServer.c $(OBJS)
	$(CC) $(CFLAGS) -o myServer myServer.c $(OBJS) $(LIBS)

.c.o:
	gcc -c $(CFLAGS) $< -o $@ 

cleano:
	rm -f *.o

clean:
	rm -f myServer myClient rcopy server *.o

### valgrind targets ###
.PHONY: vg-rcopy vg-server

vg-rcopy: rcopy
	@echo "Running rcopy under Valgrind…"
	valgrind $(VFLAGS) ./rcopy <args-to-rcopy>

vg-server: server
	@echo "Running server under Valgrind…"
	valgrind $(VFLAGS) ./server $(ERR_RATE) 


