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

#define MAXBUF 1024
#define DEBUG_FLAG 1

void sendToServer(int socketNum);
int readFromStdin(uint8_t * buffer);
void checkArgs(int argc, char * argv[]);

int main(int argc, char * argv[])
{
	int socketNum = 0;         //socket descriptor
	
	checkArgs(argc, argv);

	/* set up the TCP Client socket  */
	socketNum = tcpClientSetup(argv[1], argv[2], DEBUG_FLAG);
	
	// Loop until ^c
	while (1)
	{
		sendToServer(socketNum);
	}
	
	close(socketNum);
	
	return 0;
}

void sendToServer(int socketNum)
{
	uint8_t buffer[MAXBUF];   //data buffer
	int sendLen = 0;        //amount of data to send
	int sent = 0;            //actual amount of data sent/* get the data and send it   */
	int recvBytes = 0;
	
	sendLen = readFromStdin(buffer);
	printf("read: %s string len: %d (including null)\n", buffer, sendLen);
	
	
	sent = sendPDU(socketNum, buffer, sendLen);

	if (sent < 0)
	{
		perror("sendPDU call failed");
		exit(-1);
	}

	printf("Socket:%d: Sent, Length: %d msg: %s\n", socketNum, sent, buffer);
	
	// just for debugging, recv a message from the server to prove it works.
	recvBytes = safeRecv(socketNum, buffer, MAXBUF, 0);
	if (recvBytes == 0)
	{
		// Server closed connection
		printf("Server has terminated.\n");
		close(socketNum);
		exit(0);
	}
	
	printf("Socket %d: Byte recv: %d message: %s\n", socketNum, recvBytes, buffer);
	
}

int readFromStdin(uint8_t * buffer)
{
	char aChar = 0;
	int inputLen = 0;        
	
	// Important you don't input more characters than you have space 
	buffer[0] = '\0';
	printf("Enter data: ");
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
