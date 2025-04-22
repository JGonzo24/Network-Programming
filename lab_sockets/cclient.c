/******************************************************************************
* myClient.c
*
* Writen by Prof. Smith, updated Jan 2023
* Use at your own risk.  
*
*****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdint.h>

#include "networks.h"
#include "safeUtil.h"
#include "sendreceive.h"
#include "pollLib.h"

#define MAXBUF 1024
#define DEBUG_FLAG 1

void sendToServer(int socketNum);
int readFromStdin(uint8_t * buffer);
void checkArgs(int argc, char * argv[]);
void clientControl(int socketNum);
void processMsgFromServer(int socketNum);

int main(int argc, char * argv[])
{
	int socketNum = 0;         //socket descriptor
	
	checkArgs(argc, argv);

	/* set up the TCP Client socket  */
	socketNum = tcpClientSetup(argv[1], argv[2], DEBUG_FLAG);
	
	// call the client control() function
	clientControl(socketNum);
		
	return 0;
}

void clientControl(int socketNum)
{
	setupPollSet();
	
	// Add STDIN and the socket to the poll set
	addToPollSet(socketNum);
	addToPollSet(STDIN_FILENO);

	// Continously loop throug hthe to accept user input and process messages from the user
	while(1)
	{
		int returned_socket = pollCall(-1);

		if (returned_socket < 0)
		{
			perror("Polling failed in clientControl()");
			exit(-1);
		}
		else if (returned_socket == socketNum)
		{
			// If the returned socket is the server socket, a new client is connecting
			processMsgFromServer(socketNum);
		}
		else if (returned_socket == STDIN_FILENO)
		{
			// If the returned socket is a client socket, process its data
			sendToServer(socketNum);
		}
		else
		{
			printf("The returned socket was not a server or client socket!\n");
			return;
		}
	} 
	// If we break out of the loop, print "Server terminated" and the client will exit
	printf("Server terminated.\n");
	close(socketNum);
	exit(0);
}

void processMsgFromServer(int socketNum)
{
	uint8_t buffer[MAXBUF];   //data buffer
	int recvBytes = 0;
	
	// just for debugging, recv a message from the server to prove it works.
	recvBytes = recvPDU(socketNum, buffer, MAXBUF);

	if (recvBytes == 0)
	{
		// Server closed connection
		printf("Server has terminated.\n");
		close(socketNum);
		exit(0);
	}
	
	printf("Socket %d: Bytes recevied: %d message: %s\n", socketNum, recvBytes, buffer);
}

void sendToServer(int socketNum)
{
	uint8_t buffer[MAXBUF];   //data buffer
	int sendLen = 0;        //amount of data to send
	int sent = 0;            //actual amount of data sent/* get the data and send it   */
	int recvBytes = 0;
	
	printf("Enter data: \n");
	fflush(stdout);
	// read from stdin
	sendLen = readFromStdin(buffer);
	printf("\nread: %s string len: %d (including null)\n", buffer, sendLen);
	
	
	sent = sendPDU(socketNum, buffer, sendLen);

	if (sent < 0)
	{
		perror("sendPDU call failed");
		exit(-1);
	}

	printf("Socket %d: Sent, Length: %d msg: %s\n", socketNum, sent, buffer);
	
	// just for debugging, recv a message from the server to prove it works.
	recvBytes = recvPDU(socketNum, buffer, MAXBUF);
	if (recvBytes == 0)
	{
		// Server closed connection
		printf("Server has terminated.\n");
		close(socketNum);
		exit(0);
	}
	
	printf("Socket %d: Bytes received: %d message: %s\n", socketNum, recvBytes, buffer);
	
}

int readFromStdin(uint8_t * buffer)
{
	char aChar = 0;
	int inputLen = 0;        
	
	// Important you don't input more characters than you have space 
	buffer[0] = '\0';
	while (inputLen < (MAXBUF - 1) && aChar != '\n')
	{
		aChar = getchar();
		if (aChar != '\n')
		{
			buffer[inputLen] = aChar;
			inputLen++;
		}
	}
	
	// Null terminate the string
	buffer[inputLen] = '\0';
	inputLen++;
	
	return inputLen;
}

void checkArgs(int argc, char * argv[])
{
	/* check command line arguments  */
	if (argc != 3)
	{
		printf("usage: %s host-name port-number \n", argv[0]);
		exit(1);
	}
}
