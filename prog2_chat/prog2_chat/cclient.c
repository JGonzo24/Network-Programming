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
#include "cclient.h"
#include "makePDU.h"
#include "shared.h"

#define MAX_HANDLE_LEN 100
#define MAX_MSG_SIZE 199

char sender_handle[MAX_HANDLE_LEN] = {0}; // Global variable to store the sender handle
volatile int listInProgress = 0; // Flag to indicate if a list is in progress

CommandType parseCommand(char *buffer)
{
	// Trim leading and trailing spaces
	char trimmedBuffer[MAXBUF];
	int i = 0, j = 0;

	// Skip leading spaces
	while (buffer[i] == ' ' || buffer[i] == '\t')
	{
		i++;
	}

	// Copy the trimmed input
	while (buffer[i] != '\0' && j < MAXBUF - 1)
	{
		trimmedBuffer[j++] = tolower(buffer[i++]);
	}
	trimmedBuffer[j] = '\0'; // Null terminate the string

	// Debug: Print trimmed buffer to verify its content
	// Parse the command
	if (strncasecmp(trimmedBuffer, "%m", 2) == 0)
	{
		return CMD_SEND_MESSAGE;
	}
	else if (strncasecmp(trimmedBuffer, "%b", 2) == 0)
	{
		return CMD_BROADCAST_MESSAGE;
	}
	else if (strncasecmp(trimmedBuffer, "%c", 2) == 0)
	{
		return CMD_MULTICAST_MESSAGE;
	}
	else if (strncasecmp(trimmedBuffer, "%l", 2) == 0)
	{
		return CMD_LIST_HANDLES;
	}
	else
	{
		printf("Invalid buffer: %s\n", trimmedBuffer); // Debug output
		return CMD_INVALID;
	}
}


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

	uint8_t flag = buffer[2];
	// printf("Server response: %d\n", flag);
	if (flag == 2)
	{
		printf("Confirmed login!\n");
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

int handleSendMessage(int socketNum, const char *buffer)
{
	// Extract the destination handle and message from the buffer
	char destinationHandle[MAX_HANDLE_LEN];
	uint8_t text_message[MAXBUF];

	// Parse the command from the buffer
	readMessageCommand(buffer, destinationHandle, text_message);

	// add a byte to the text_message len for the null terminator
	int text_message_len = strlen((char *)text_message);
	text_message_len += 1; // Add 1 for the null terminator


	int sentBytes = 0;
	int chunkSize = 0;
	// Check if the message length exceeds the maximum size
	while (sentBytes < text_message_len)
	{

		chunkSize = MAX_MSG_SIZE;
		if (text_message_len - sentBytes < MAX_MSG_SIZE)
		{
			chunkSize = text_message_len - sentBytes;
		}
		MessagePacket_t packetInfo = constructMessagePacket(destinationHandle, chunkSize, (uint8_t *)text_message + sentBytes, socketNum);
		sentBytes += chunkSize;

		if (packetInfo.packet_len == 0)
		{
			printf("Error constructing message packet.\n");
			return -1;
		}
		
		// Send the message packet in chunks
		// int sent = sendMessageInChunks(socketNum, destinationHandle, packetInfo.packet, packetInfo.packet_len);
		int sent = sendPDU(socketNum, packetInfo.packet, packetInfo.packet_len);
		if (sent < 0)
		{
			printf("Error sending message packet.\n");
			return -1;
		}
		else
		{
			printf("Message sent successfully.\n");
		}
	}
	return 0;
}

void readMessageCommand(const char *buffer, char destinationHandle[100], uint8_t text_message[MAXBUF])
{
	// Tokenize the buffer to extract the command, destination handle, and message
	char *token = strtok((char *)buffer, " ");
	if (token == NULL || strcasecmp(token, "%m") != 0)
	{
		printf("Invalid message format. Use: %%m <destination_handle> <message>\n");
		return;
	}
	// Extract the destination handle
	token = strtok(NULL, " ");
	if (token == NULL)
	{
		printf("Invalid message format. Use: %%m <destination_handle> <message>\n");
		return;
	}
	strncpy(destinationHandle, token, MAX_HANDLE_LEN - 1);

	// Extract the message
	token = strtok(NULL, "\n");
	if (token == NULL)
	{
		printf("Invalid message format. Use: %%m <destination_handle> <message>\n");
		return;
	}
	strncpy((char *)text_message, token, MAXBUF - 1);
	text_message[MAXBUF] = '\0'; // Null terminate only the text message
}


void readMulticastCommand(char *buffer, uint8_t *numHandles, DestHandle_t handles[], char *message)
{
    char *token = strtok(buffer, " ");
    if (!token || strcasecmp(token, "%C") != 0)
    {
        printf("Invalid format. Use: %%C <num_handles> <handle1> <handle2> ... <message>\n");
        return;
    }

    // Parse number of handles
    token = strtok(NULL, " ");
    if (!token || sscanf(token, "%hhd", numHandles) != 1 || *numHandles <= 0)
    {
        printf("Invalid number of handles. Must be > 0.\n");
        return;
    }

    // Now grab each handle and store it into handles[i].handle_name
    for (int i = 0; i < *numHandles; i++)
    {
        token = strtok(NULL, " ");
        if (!token)
        {
            printf("Expected %d handles but found %d.\n", *numHandles, i);
            return;
        }
        strncpy(handles[i].handle_name, token, MAX_HANDLE_LEN - 1);
        handles[i].handle_name[MAX_HANDLE_LEN - 1] = '\0';
        // Optionally, update the dest handle length field
        handles[i].dest_handle_len = strlen(handles[i].handle_name);
    }

    char *msgStart = strtok(NULL, "");
    if (!msgStart)
        msgStart = ""; // allow empty message

    // Copy up to MAX_MSG_SIZE chars, then null-terminate
    strncpy(message, msgStart, MAXBUF - 1);
    message[MAXBUF - 1] = '\0';
}
	
int sendMessageInChunks(int socketNum, char *destinationHandle, uint8_t *fullMessage, int fullMessageLength)
{

	int sent = sendPDU(socketNum, fullMessage, fullMessageLength);
	if (sent < 1)
	{
		printf("ERROR in sending PDU from sendMessageInchunks\n");
		return -1;
	}
	return 1;
}

void handleBroadcastMessage(int socketNum, const char *buffer)
{
	// Extract the message from the buffer
	char message[MAXBUF];

	// Parse the message from the buffer
	if (sscanf(buffer, "%%b %[^\n]", message) != 1)
	{
		printf("Invalid broadcast message format. Use: %%b <message>\n");
		return;
	}
	int message_len = strlen(message);

	// Check if the message is empty	
	if (message_len == 0)
	{
		printf("Message cannot be empty.\n");
		return;
	}

	// Construct the broadcast packet
	uint8_t broadcastPDU[MAXBUF];
	int done = sendBroadcastPDU(broadcastPDU, socketNum, message, sender_handle);
	if (done < 0)
	{
		printf("Error sending broadcast PDU.\n");
		return;
	}
	else
	{
		printf("Broadcast PDU sent successfully.\n");
	}
}


void handleMulticastMessage(int socketNum, char *buffer)
{

	uint8_t numHandles = 0;
	DestHandle_t handles[MAX_DEST_HANDLES];
	char message[MAXBUF];

	readMulticastCommand(buffer, &numHandles, handles, message);

	if (numHandles == 0)
	{
		printf("Invalid multicast message format. Use: %%c <num_handles> <handle1> <handle2> ... <message>\n");
		return;
	}

	// Now, we can construct the multicast packet
	uint8_t multicastPDU[MAXBUF];


	int sentBytes = 0;
	int chunkSize = 0;

	while (sentBytes < strlen(message))
	{
		chunkSize = MAX_MSG_SIZE;
		if (strlen(message) - sentBytes < MAX_MSG_SIZE)
		{
			chunkSize = strlen(message) - sentBytes;
		}
		int pduLen = constructMulticastPDU(multicastPDU, socketNum, sender_handle, numHandles, handles, message + sentBytes);
		sentBytes += chunkSize;

		if (pduLen < 0)
		{
			printf("Error constructing multicast PDU.\n");
			return;
		}

		// Send the multicast PDU
		int sent = sendPDU(socketNum, multicastPDU, pduLen);
		if (sent < 0)
		{
			printf("Error sending multicast PDU.\n");
			return;
		}
		else
		{
			printf("Multicast PDU sent successfully.\n");
		}

	}
}

void handleListHandles(int socketNum, const char *buffer)
{
	// all we need to do is list the current handles in the handle table
	uint8_t listPDU[MAXBUF];
	int done = makeListPDU(listPDU, socketNum);
	if (done < 0)
	{
		printf("Error sending list PDU.\n");
		return;
	}
}

void handleInvalidCommand(int socketNum, const char *buffer)
{
	printf("Handle invalid command.\n");
}

int main(int argc, char *argv[])
{
	int socketNum = 0; // socket descriptor
	strncpy(sender_handle, argv[1], MAX_HANDLE_LEN - 1);
	checkArgs(argc, argv);

	/* set up the TCP Client socket  */
	socketNum = tcpClientSetup(argv[2], argv[3], DEBUG_FLAG);

	printf("Client handle: %s\n", sender_handle);
	clientControl(socketNum);

	return 0;
}

void sendCommand(int socketNum, char *buffer)
{
	CommandType commandType = parseCommand(buffer);
	// Check if the command is valid	
	switch (commandType)
	{
	case CMD_SEND_MESSAGE:
		handleSendMessage(socketNum, buffer);
		break;

	case CMD_BROADCAST_MESSAGE:
		
		handleBroadcastMessage(socketNum, buffer);
		break;

	case CMD_MULTICAST_MESSAGE:

		handleMulticastMessage(socketNum, buffer);
		break;

	case CMD_LIST_HANDLES:

		handleListHandles(socketNum, buffer);
		break;

	case CMD_INVALID:
		
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
	setupPollSet();

	// Add STDIN and the socket to the poll set
	addToPollSet(socketNum);
	addToPollSet(STDIN_FILENO);

	initialConnection(socketNum, 1);

	// Continously loop through the to accept user input and process messages from the user
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
			if (!listInProgress)
			{
				sendToServer(socketNum);
			}
			else
			{
				printf("List in progress, please wait...\n");
			}
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

void printPacket(const uint8_t *packet, size_t length)
{
	printf("Packet (%zu bytes):\n", length);
	for (size_t i = 0; i < length; i++)
	{
		printf("%02x ", packet[i]); // Print each byte in hexadecimal format
		if ((i + 1) % 16 == 0)
		{
			printf("\n"); // Add a newline every 16 bytes for readability
		}
	}
	printf("\n");
}

int initialConnection(int clientSocket, uint8_t flag)
{
	uint8_t *initial_packet = makeInitialPDU();

	int handle_len = strlen(sender_handle);
	int initial_packet_len = 1 + handle_len + 1 + 1; // 2 bytes for length, 1 byte for flag

	int sent = sendPDU(clientSocket, initial_packet, initial_packet_len);

	if (sent < 0)
	{
		printf("Error sending initial packet to server.\n");
		return -1;
	}

	waitForServerResponse(clientSocket);
	return 0;
}

void receiveMessage(uint8_t *buffer, int totalBytes)
{

	int offset = 0;

	offset++;

	uint8_t sender_handle_len = buffer[offset++];

	char sender_handle_rec[sender_handle_len];
	memcpy(sender_handle_rec, buffer + offset, sender_handle_len);
	offset += sender_handle_len;

	uint8_t numDestHandles = buffer[offset++];

	// each destination handle has a length then the handle name
	DestHandle_t handles[numDestHandles];
	for (int i = 0; i < numDestHandles; i++)
	{
		handles[i].dest_handle_len = buffer[offset++];
		memcpy(handles[i].handle_name, buffer + offset, handles[i].dest_handle_len);
		handles[i].handle_name[handles[i].dest_handle_len] = '\0'; // Null-terminate the handle
		offset += handles[i].dest_handle_len;
	}

	int message_length = totalBytes - offset;
	
	if (message_length > 0)
	{
		// Copy the message into a printable string
		char message[message_length + 1];
		memcpy(message, buffer + offset, message_length);
		message[message_length] = '\0'; // null-terminate

		printf("%s: %s\n", sender_handle_rec, message);
	}
	else
	{
		printf("No message payload.\n");
	}
}


int receiveBroadcastMessage(uint8_t *buffer, int totalBytes)
{
	int offset = 0;

	offset++;

	uint8_t sender_handle_len = buffer[offset++];

	char sender_handle_rec[sender_handle_len];
	memcpy(sender_handle_rec, buffer + offset, sender_handle_len);
	offset += sender_handle_len;

	int message_length = totalBytes - offset;
	
	if (message_length > 0)
	{
		// Copy the message into a printable string
		char message[message_length + 1];
		memcpy(message, buffer + offset, message_length);
		message[message_length] = '\0'; // null-terminate

		printf("%s: %s\n", sender_handle_rec, message);
	}
	else
	{
		printf("No message payload.\n");
	}
	return 0;
}

int handleFlagsFromServer(int flag, uint8_t *buffer, int totalBytes)
{
	// Handle the flags from the server
	switch (flag)
	{
	case 0x04:
		printf("Received a message from the server.\n");
		receiveBroadcastMessage(buffer, totalBytes);
		break;
	case 0x05:
		printf("Incoming message...\n");
		receiveMessage(buffer, totalBytes);
		break;
	case 0x06:
		printf("Multicast message command received.\n");
		receiveMessage(buffer, totalBytes);
		break;
	case 0x07:
		printf("Error packet, destination handle does not exist.\n");
		break;
	case 0xB:
		printf("Received a number of handles from the server.\n");
		listInProgress = 1;
		processListHandles(buffer, totalBytes);
		break;
	case 0xC:
		printf("Received a list of handles from the server.\n");
		processListHandles(buffer, totalBytes);
		break;
	case 0xD:
		printf("Done receiving handles from the server.\n");
		listInProgress = 0;
		break;
	default:
		printf("Unknown flag received: %d\n", flag);
		break;
	}
	return 0;
}

void processListHandles(uint8_t *buffer, int totalBytes)
{
	if (buffer[0] == 0xb)
	{
		uint32_t numHandles;
		memcpy(&numHandles, buffer + 1, sizeof(uint32_t));
		numHandles = ntohl(numHandles); // Convert from network byte order to host byte order
		printf("Number of handles: %d\n", numHandles);
	}
	else if(buffer[0] == 0xC)
	{

		int handleLen = buffer[1];

		printf("Handle name: ");
		for (int i = 0; i < handleLen; i++)
		{
			printf("%c", buffer[2 + i]);	
		}
		printf("\n");
	}
	else
	{
		printf("Invalid flag for list handles: %d\n", buffer[0]);
		return;
	}
}

void processMsgFromServer(int socketNum)
{
	uint8_t buffer[MAXBUF]; // data buffer
	int recvBytes = 0;

	recvBytes = recvPDU(socketNum, buffer, MAXBUF);

	handleFlagsFromServer(buffer[0], buffer, recvBytes);
	
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
}

void sendToServer(int socketNum)
{
	uint8_t buffer[MAXBUF];
	int inputLen = readFromStdin(buffer);
	if (inputLen < 1)
	{
		printf("Error reading from stdin.\n");
		return;
	}
	sendCommand(socketNum, (char *)buffer);
}

int readFromStdin(uint8_t *buffer)
{
	char aChar = 0;
	int inputLen = 0;

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
