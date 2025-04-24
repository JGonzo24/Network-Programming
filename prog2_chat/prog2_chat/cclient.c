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
#include "handle_table.h"
#include <poll.h> // Include poll.h for pollfd structure
#include "pollLib.h"

#define MAXBUF 1024
#define DEBUG_FLAG 1
#define MAX_MSG_SIZE 200

void sendToServer(int socketNum);
int readFromStdin(uint8_t * buffer);
void checkArgs(int argc, char * argv[]);
void clientControl(int socketNum);
void processMsgFromServer(int socketNum);
int chooseCommand(char* buffer);
CommandType parseCommand(const char* buffer);
void handleSendMessage(int socketNum, const char* buffer);
void handleBroadcastMessage(int socketNum, const char* buffer);
void handleMulticastMessage(int socketNum, const char* buffer);
void handleListHandles(int socketNum, const char* buffer);
void handleInvalidCommand(int socketNum, const char* buffer);



typedef enum
{
	CMD_SEND_MESSAGE,
	CMD_BROADCAST_MESSAGE,
	CMD_MULTICAST_MESSAGE,
	CMD_LIST_HANDLES,
	CMD_INVALID
} CommandType;


CommandType parseCommand(const char* buffer)
{
	// Ensure that the command is case insensitive
	char command[MAXBUF];
	int i = 0;
	while (buffer[i] != '\0' && i < MAXBUF - 1)
	{
		command[i] = tolower(buffer[i]);
		i++;
	}
	command[i] = '\0'; // Null terminate the string

	if (strncmp(buffer, "%m", 2) == 0) 
	{
		return CMD_SEND_MESSAGE;
	}
	else if (strncmp(buffer, "%b",2) == 0) 
	{
		return CMD_BROADCAST_MESSAGE;
	}
	else if (strncmp(buffer, "%c", 2) == 0) 
	{
		return CMD_MULTICAST_MESSAGE;
	}
	else if (strncmp(buffer, "%l", 2) == 0) 
	{
		return CMD_LIST_HANDLES;
	}
	else
	{
		return CMD_INVALID;
	}
}


void handleSendMessage(int socketNum, const char* buffer)
{
	// First thing to check is the size of the message, if greater than 200 then break it up so you don't send more than 200 bytes
	int buffer_length = strlen(buffer);
	char message[MAX_MSG_SIZE];
	int bytes_sent = 0;

	// Get the PDU ready
	uint8_t message_packet_len[3];
	uint8_t flag; 
	uint8_t sender_handle_len;
	uint8_t sender_handle[32];
	uint


	while (bytes_sent < buffer_length)
	{
		int bytes_to_send = buffer_length - bytes_sent;
		if (bytes_to_send > MAX_MSG_SIZE)
		{
			bytes_to_send = MAX_MSG_SIZE;
		}
		
		strncpy(message, buffer + bytes_sent, bytes_to_send);
		message[bytes_to_send] = '\0'; // Null terminate the message
		bytes_sent += bytes_to_send;
		
		sendPDU(socketNum, (uint8_t*)message, bytes_to_send);
		printf("Sent message: %s\n", message);
	}
}

int main(int argc, char * argv[])
{
	int socketNum = 0;         //socket descriptor
	char *handle = argv[1];
	checkArgs(argc, argv);

	/* set up the TCP Client socket  */
	socketNum = tcpClientSetup(argv[2], argv[3], DEBUG_FLAG);
	
	printf("Handle: %s\n", handle);


	
	// call the client control() function
	clientControl(socketNum);
		
	return 0;
}

int chooseCommand(char* buffer)
{
	CommandType commandType = parseCommand(buffer);
	
	switch (commandType)
	{
		case CMD_SEND_MESSAGE:
			// Handle send message command
			printf("Send message command detected.\n");
			
			break;
		
		case CMD_BROADCAST_MESSAGE:
			// Handle broadcast message command
			printf("Broadcast message command detected.\n");
			break;

		case CMD_MULTICAST_MESSAGE:
			// Handle multicast message command
			printf("Multicast message command detected.\n");
			break;

		case CMD_LIST_HANDLES:
			// Handle list handles command
			printf("List handles command detected.\n");
			break;	

		case CMD_INVALID:
			// Handle invalid command
			printf("Invalid command detected.\n");
			break;

		default:
			// Handle unknown command
			printf("Unknown command detected.\n");
			break;
	}
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
		printf("$: ");
		fflush(stdout);
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
	

	// read from stdin
	sendLen = readFromStdin(buffer);
	
	int command_success = choose_command(buffer); // Returns 0 on success, -1 on failure

	// printf("\nread: %s string len: %d (including null)\n", buffer, sendLen);
	
	
	// sent = sendPDU(socketNum, buffer, sendLen);

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
	if (argc != 4)
	{
		printf("usage: %s handle host-name port-number \n", argv[0]);
		exit(1);
	}
}
