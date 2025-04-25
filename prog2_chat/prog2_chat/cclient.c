/******************************************************************************
 * myClient.c
 *
 * Writen by Prof. Smith, updated Jan 2023
 * Use at your own risk.
 *
 *****************************************************************************/
#include <ctype.h>
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

#define MAXBUF 1400
#define DEBUG_FLAG 1
#define MAX_MSG_SIZE 199 // Maximum message size (excluding the null terminator)
#define MAX_HANDLE_LEN 100

typedef enum
{
	CMD_SEND_MESSAGE,
	CMD_BROADCAST_MESSAGE,
	CMD_MULTICAST_MESSAGE,
	CMD_LIST_HANDLES,
	CMD_INVALID
} CommandType;

char sender_handle[MAX_HANDLE_LEN];

void sendToServer(int socketNum);
int readFromStdin(uint8_t *buffer);
void checkArgs(int argc, char *argv[]);
void clientControl(int socketNum);
void createInitialPacket();
void processMsgFromServer(int socketNum);
void sendCommand(int socketNum, char *buffer);
CommandType parseCommand(const char *buffer);
void handleSendMessage(int socketNum, const char *buffer);
void handleBroadcastMessage(int socketNum, const char *buffer);
void handleMulticastMessage(int socketNum, const char *buffer);
void handleListHandles(int socketNum, const char *buffer);
void handleInvalidCommand(int socketNum, const char *buffer);
void sendMessageInChunks(int socketNum, const char *sendingHandle, const char *destinationHandle, char *message, int message_len);

CommandType parseCommand(const char *buffer)
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

	if (strncmp(command, "%m", 2) == 0)
	{
		return CMD_SEND_MESSAGE;
	}
	else if (strncmp(command, "%b", 2) == 0)
	{
		return CMD_BROADCAST_MESSAGE;
	}
	else if (strncmp(command, "%c", 2) == 0)
	{
		return CMD_MULTICAST_MESSAGE;
	}
	else if (strncmp(command, "%l", 2) == 0)
	{
		return CMD_LIST_HANDLES;
	}
	else
	{
		return CMD_INVALID;
	}
}



/*
Format for Message Packet for %m command:
----------- 3 byte chat header ------------------
2 bytes: length of the packet
1 byte: command type (0x01 for %m)
-------------------------------------------------
1 byte: sender handle length
x bytes: sender handle name
1 byte: value 1 to specify the message only has one destination handle
---------- Destination Handle --------------
1 byte: destination handle length
x bytes: destination handle name
---------- Message ---------------------------
x bytes: Null terminated message string
*/


void waitForServerResponse(int socketNum)
{
	uint8_t buffer[MAXBUF]; // data buffer
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

	if (recvBytes < MAXBUF)
	{
		buffer[recvBytes] = '\0'; // Null terminate the string
	}


	uint8_t flag = buffer[2];
	if (flag == 2)
	{
		printf("Confirming good message.\n");

	}
	else if (flag == 3)
	{
		printf("Error on initial packet (handle already exists).\n");
		exit(-1);
	}
	else
	{
		printf("Unknown server response.\n");
	}
}
void handleSendMessage(int socketNum, const char *buffer)
{
	char destinationHandle[MAX_HANDLE_LEN];
	char message[MAX_MSG_SIZE];
	int destinationHandleLen = 0;
	int message_len = 0;

	printf("Handle send message command.\n");

	// Parse the handle and message from the buffer
	if (sscanf(buffer, "%%m %s %[^\n]", destinationHandle, message) != 2)
	{
		printf("Invalid send message format. Use: %%m <handle> <message>\n");
		return;
	}

	destinationHandleLen = strlen(destinationHandle);
	message_len = strlen(message);

	if (destinationHandleLen == 0 || message_len == 0)
	{
		printf("Handle or message cannot be empty.\n");
		return;
	}

	// create the message packet 
	uint8_t messagePacket[MAXBUF];

	// First two bytes are the length of the whole packet
	uint16_t total_packet_length = 1 + 1 + strlen(sender_handle) + 1 + 1 + strlen(destinationHandle) + message_len + 1;

	// Convert the length to network order
	uint16_t length_in_network_order = htons(total_packet_length);
	
	// bytes 0 and 1 are the length of the packet
	memcpy(messagePacket, &length_in_network_order, 2);

	// byte 2 is the command type (0x01 for %m)
	uint8_t command_type = 0x01;
	messagePacket[2] = command_type;

	// byte 3 has one byte for the length of the sender handle
	uint8_t sender_handle_len = strlen(sender_handle);
	messagePacket[3] = sender_handle_len;
	// copy the sender handle into the message packet
	memcpy(messagePacket + 4, sender_handle, sender_handle_len);

	// byte 4 + sender_handle_len has one byte for the value 1
	messagePacket[4 + sender_handle_len] = 1;
	// byte 5 + sender_handle_len has one byte for the length of the destination handle
	messagePacket[5 + sender_handle_len] = strlen(destinationHandle);
	// copy the destination handle into the message packet
	memcpy(messagePacket + 6 + sender_handle_len, destinationHandle, strlen(destinationHandle));
	// byte 6 + sender_handle_len + destination_handle_len has the message
	memcpy(messagePacket + 7 + sender_handle_len + strlen(destinationHandle), message, message_len);
	// byte 7 + sender_handle_len + destination_handle_len + message_len has the null terminator
	messagePacket[7 + sender_handle_len + strlen(destinationHandle) + message_len] = '\0';

	// Now that we have the message packet, we can send it to the server
	int bytesSent = sendPDU(socketNum, messagePacket, total_packet_length);
	if (bytesSent < 0)
	{
		printf("Error sending message packet to server.\n");
		return;
	}
	// Print the message packet for debugging
	printf("Message Packet Sent:\n");
	for (int i = 0; i < total_packet_length; i++)
	{
		printf("%02x ", messagePacket[i]);
	}
	printf("\n");
	// Print the sender handle and destination handle for debugging
	printf("Sender Handle: %s, Destination Handle: %s\n", sender_handle, destinationHandle);

	// wait for server response
	waitForServerResponse(socketNum);
}


void handleBroadcastMessage(int socketNum, const char *buffer)
{
	printf("Handle broadcast message command.\n");
	// Extract the message from the buffer
	char message[MAX_MSG_SIZE];

	// Parse the message from the buffer
	if (sscanf(buffer, "%%b %[^\n]", message) != 1)
	{
		printf("Invalid broadcast message format. Use: %%b <message>\n");
		return;
	}

	int message_len = strlen(message);
	if (message_len == 0)
	{
		printf("Message cannot be empty.\n");
		return;
	}
}
void parseMulticastInput(const char *buffer, int *numHandles, char destinationHandles[9][MAX_HANDLE_LEN], char *message)
{
	// 1) Skip the “%c” and any spaces
	const char *ptr = buffer + 2;
	while (*ptr == ' ')
		ptr++;

	// 2) Parse the number of handles with strtol (gives end‐ptr)
	char *endptr;
	long nh = strtol(ptr, &endptr, 10);
	if (endptr == ptr || nh < 2 || nh > 9)
	{
		printf("Invalid number of handles. Must be between 2 and 9.\n");
		*numHandles = -1; // Indicate error
		return;
	}
	*numHandles = (int)nh;

	// 3) Advance past the count and any spaces
	ptr = endptr;
	while (*ptr == ' ')
		ptr++;

	// 4) Parse each destination handle
	for (int i = 0; i < *numHandles; i++)
	{
		// Find length of this handle token
		int len = 0;
		while (ptr[len] != '\0' && ptr[len] != ' ')
			len++;

		if (len == 0 || len >= MAX_HANDLE_LEN)
		{
			printf("Invalid destination handle format.\n");
			*numHandles = -1; // Indicate error
			return;
		}

		// Copy it into destinationHandles[i]
		memcpy(destinationHandles[i], ptr, len);
		destinationHandles[i][len] = '\0';

		// Advance ptr past this handle and any following spaces
		ptr += len;
		while (*ptr == ' ')
			ptr++;
	}

	// 5) The rest of ptr is the message
	if (*ptr == '\0')
	{
		printf("Invalid format: no message provided.\n");
		*numHandles = -1; // Indicate error
		return;
	}
	strncpy(message, ptr, MAX_MSG_SIZE - 1);
	message[MAX_MSG_SIZE - 1] = '\0'; // Ensure null termination
}

void handleMulticastMessage(int socketNum, const char *buffer)
{
	printf("Handle multicast message command.\n");

	int numHandles = 0;
	char destinationHandles[9][MAX_HANDLE_LEN]; // Maximum of 9 destination handles
	char message[MAX_MSG_SIZE];

	// Parse the input
	parseMulticastInput(buffer, &numHandles, destinationHandles, message);

	if (numHandles == -1)
	{
		// Parsing failed
		return;
	}

	printf("Number of handles: %d\n", numHandles);
	for (int i = 0; i < numHandles; i++)
	{
		printf("Destination Handle %d: %s\n", i + 1, destinationHandles[i]);
	}
	printf("Message: %s\n", message);
}

void handleListHandles(int socketNum, const char *buffer)
{
	printf("Handle list handles command.\n");
}

void handleInvalidCommand(int socketNum, const char *buffer)
{
	printf("Handle invalid command.\n");
}

int main(int argc, char *argv[])
{
	int socketNum = 0; // socket descriptor
	strncpy(sender_handle, argv[1], MAX_HANDLE_LEN - 1);
	sender_handle[MAX_HANDLE_LEN - 1] = '\0'; // Ensure null termination
	checkArgs(argc, argv);


	/* set up the TCP Client socket  */
	socketNum = tcpClientSetup(argv[2], argv[3], DEBUG_FLAG);
	
	printf("Client handle: %s\n", sender_handle);
	// call the client control() function
	clientControl(socketNum);

	return 0;
}

void sendCommand(int socketNum, char *buffer)
{
	CommandType commandType = parseCommand(buffer);

	switch (commandType)
	{
	case CMD_SEND_MESSAGE:
		// Handle send message command
		printf("Send message command detected.\n");
		// call handleSendMessage(socketNum, buffer);
		handleSendMessage(socketNum, buffer);

		break;

	case CMD_BROADCAST_MESSAGE:
		// Handle broadcast message command
		printf("Broadcast message command detected.\n");
		// call handleBroadcastMessage(socketNum, buffer);
		handleBroadcastMessage(socketNum, buffer);

		break;

	case CMD_MULTICAST_MESSAGE:
		// Handle multicast message command
		printf("Multicast message command detected.\n");
		// call handleMulticastMessage(socketNum, buffer);
		handleMulticastMessage(socketNum, buffer);

		break;

	case CMD_LIST_HANDLES:
		// Handle list handles command
		printf("List handles command detected.\n");
		// call handleListHandles(socketNum, buffer);
		handleListHandles(socketNum, buffer);

		break;

	case CMD_INVALID:
		// Handle invalid command
		printf("Invalid command detected.\n");
		// call handleInvalidCommand(socketNum, buffer);
		handleInvalidCommand(socketNum, buffer);

		break;

	default:
		// Handle unknown command
		printf("Unknown command detected.\n");
		break;
	}
}

void clientControl(int socketNum)
{
	// init valid handle here 
	/*3-byte chat-header, then 1 byte handle length then the handle (*/

	// 1 byte for the value 1 (one destination handle)
	// 1 byte for the length of the destination handle

    createInitialPacket();

    setupPollSet();

	// Add STDIN and the socket to the poll set
	addToPollSet(socketNum);
	addToPollSet(STDIN_FILENO);

	// Continously loop throug hthe to accept user input and process messages from the user
	while (1)
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

void initialConnection(int clientSocket, uint8_t flag)
{
    // First thing is to create the initial packet and have the flag be 1

	// First two bytes are the length of the whole packet

	/*
	- First two bytes are the length of the whole packet
	- byte 2 is the flag
	- byte 3 has the length of the handle
	- copy the handle into the packet
	*/
	uint16_t total_packet_length = 4 + strlen(sender_handle);
	uint8_t initialPacket[total_packet_length];
	// Convert the length to network order
	uint16_t length_in_network_order = htons(total_packet_length);
	memcpy(initialPacket, &length_in_network_order, 2);
	// byte 2 is the flag
	initialPacket[2] = flag;
	// byte 3 has one byte for the length of the sender handle
	uint8_t sender_handle_len = strlen(sender_handle);
	initialPacket[3] = sender_handle_len;
	// copy the sender handle into the message packet
	memcpy(initialPacket + 4, sender_handle, sender_handle_len);

	// Now that we have the message packet, we can send it to the server
	int bytesSent = sendPDU(clientSocket, initialPacket, total_packet_length);

	

}

void processMsgFromServer(int socketNum)
{
	uint8_t buffer[MAXBUF]; // data buffer
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

	if (recvBytes < MAXBUF)
	{
		buffer[recvBytes] = '\0'; // Null terminate the string
	}


	printf("Socket %d: Bytes recevied: %d message: %s\n", socketNum, recvBytes, buffer);



}

void sendToServer(int socketNum)
{
	uint8_t buffer[MAXBUF]; // data buffer
	int sendLen = readFromStdin(buffer);
	sendCommand(socketNum, (char *)buffer); // Returns 0 on success, -1 on failure
}

int readFromStdin(uint8_t *buffer)
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

void checkArgs(int argc, char *argv[])
{
	/* check command line arguments  */
	if (argc != 4)
	{
		printf("usage: %s handle host-name port-number \n", argv[0]);
		exit(1);
	}
}
