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

char sender_handle[MAX_HANDLE_LEN] = {0}; // Global variable to store the sender handle

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
	printf("Waiting for server response...\n");
	fflush(stdout);
	recvBytes = recvPDU(socketNum, buffer, MAXBUF);

	if (recvBytes == 0)
	{
		// Server closed connection
		printf("Server has terminated.\n");
		close(socketNum);
		exit(0);
	}

	uint8_t flag = buffer[2];
	printf("Server response: %d\n", flag);
	if (flag == 2)
	{
		printf("Confirming login message.\n");
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
	printf("Handle send message command.\n");
	// Extract the destination handle and message from the buffer
	char destinationHandle[MAX_HANDLE_LEN];
	uint8_t text_message[MAX_MSG_SIZE];

	// Parse the command from the buffer
	readMessageCommand(buffer, destinationHandle, text_message);

	// add a byte to the text_message len for the null terminator
	int text_message_len = strlen((char *)text_message);
	text_message_len += 1; // Add 1 for the null terminator

	// Debug: Print the extracted values
	// printf("-----------------------------------------\n");
	// printf("Sender Handle: %s\n", sender_handle);
	// printf("Destination Handle: %s\n", destinationHandle);
	// printf("Message: %s\n", text_message);
	// printf("-----------------------------------------\n");

	// print both in bytes
	// printf("----------------------------------------\n");
	// printf("Sender Handle in bytes: ");
	// for (int i = 0; i < strlen(sender_handle); i++)
	// {
	// 	printf("%02x ", sender_handle[i]);
	// }
	// printf("\n");
	// printf("Destination Handle in bytes: ");
	// for (int i = 0; i < strlen(destinationHandle); i++)
	// {
	// 	printf("%02x ", destinationHandle[i]);
	// }
	// printf("\n");
	// printf("Message in bytes: ");
	// for (int i = 0; i < text_message_len; i++)
	// {
	// 	printf("%02x ", text_message[i]);
	// }
	// printf("\n");
	// printf("-----------------------------------------\n");

	// int senderHandleLen = strlen(sender_handle);
	// int destinationHandleLen = strlen(destinationHandle);
	// printf("---------------------------------------\n");
	// printf("Destination Handle Length: %d\n", destinationHandleLen);
	// printf("Text Message Length: %d\n", text_message_len);
	// printf("Sender Handle Length: %d\n", senderHandleLen);
	// printf("---------------------------------------\n");

	// Construct the message packet
	MessagePacket_t packetInfo = constructMessagePacket(destinationHandle, text_message_len, (uint8_t *)text_message, socketNum);
	if (packetInfo.packet_len == 0)
	{
		printf("Error constructing message packet.\n");
		return -1;
	}
	printf("----------------------------------------\n");
	printf("Message packet text length: %d\n", packetInfo.text_message_len);
	printf("-----------------------------------------\n");
	// Send the message packet in chunks
	int sent = sendMessageInChunks(socketNum, destinationHandle, packetInfo.packet, packetInfo.packet_len);
	if (sent < 0)
	{
		printf("Error sending message packet.\n");
		return -1;
	}
	else
	{
		printf("Message sent successfully.\n");
	}
	return 0;
}

void readMessageCommand(const char *buffer, char destinationHandle[100], uint8_t text_message[199])
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
	strncpy((char *)text_message, token, MAX_MSG_SIZE - 1);
	text_message[MAX_MSG_SIZE] = '\0'; // Null terminate only the text message
}

#include <stdio.h>
#include <string.h>

#define MAX_HANDLE_LEN 100
#define MAX_MSG_SIZE 199

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

    // The rest of the buffer (including spaces) is the message
    // strtok(NULL, "") returns the remainder of the string
    char *msgStart = strtok(NULL, "");
    if (!msgStart)
        msgStart = ""; // allow empty message

    // Copy up to MAX_MSG_SIZE chars, then null-terminate
    strncpy(message, msgStart, MAX_MSG_SIZE - 1);
    message[MAX_MSG_SIZE - 1] = '\0';

    // --- Debug printout ---
    printf("Parsed multicast command:\n");
    printf("  Number of handles: %d\n", *numHandles);
    for (int i = 0; i < *numHandles; i++)
        printf("    Handle %d: %s\n", i + 1, handles[i].handle_name);
    printf("  Message: %s\n", message);
}
	
int sendMessageInChunks(int socketNum, char *destinationHandle, uint8_t *fullMessage, int fullMessageLength)
{

	int sent = sendPDU(socketNum, fullMessage, fullMessageLength);
	if (sent < 1)
	{
		printf("ERROR in sending PDU from sendMessageInchunks\n");
		return -1;
	}
	else
	{
		printf("All good\n");
	}
	return 1;
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


void handleMulticastMessage(int socketNum, char *buffer)
{
	printf("Handle multicast message command.\n");
	uint8_t* multicastPDU = constructMulticastPacket(buffer, socketNum);	
	
	// print out the pdu in bytes
	printf("We got back home");
	
}

void handleListHandles(int socketNum, const char *buffer)
{
	printf("Handle list handles command.\n");

	// get the arguments from the buffer
	char *token = strtok((char *)buffer, " ");
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
	// call the client control() function
	clientControl(socketNum);

	return 0;
}

void sendCommand(int socketNum, char *buffer)
{
	CommandType commandType = parseCommand(buffer);
	printf("command type %d\n", commandType);
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
	// printf("Parsing received PDU, length: %d\n", totalBytes);

	// printf("PDU contents: ");
	// for (int i = 0; i < totalBytes; i++) {
	//     printf("%02x ", buffer[i]);
	// }
	// printf("\n");

	int offset = 0;

	// Step 1: Extract the flag
	// flag = buffer[offset++];
	offset++;
	// printf("Flag: %d\n", flag);

	// Step 2: Extract sender handle length
	uint8_t sender_handle_len = buffer[offset++];
	// printf("Sender handle length: %d\n", sender_handle_len);

	// Step 3: Extract sender handle (not null-terminated in PDU)
	char sender_handle_rec[sender_handle_len];
	memcpy(sender_handle_rec, buffer + offset, sender_handle_len);
	offset += sender_handle_len;
	// printf("Sender handle: %s\n", sender_handle_rec);

	// Step 4: Extract destination flag
	//uint8_t destination_flag = buffer[offset++];
	offset++;
	// printf("Destination flag (should be 1): %d\n", destination_flag);

	// Step 5: Extract destination handle length
	uint8_t destination_handle_len = buffer[offset++];
	// printf("Destination handle length: %d\n", destination_handle_len);

	// Step 6: Extract destination handle (also not null-terminated)
	char destination_handle[destination_handle_len];
	memcpy(destination_handle, buffer + offset, destination_handle_len);
	offset += destination_handle_len;
	// printf("Destination handle: %s\n", destination_handle);

	// Step 7: Extract the message
	int message_length = totalBytes - offset;
	// printf("Message length: %d\n", message_length);

	if (message_length > 0)
	{
		// Copy the message into a printable string
		char message[message_length + 1];
		memcpy(message, buffer + offset, message_length);
		message[message_length] = '\0'; // null-terminate

		// printf("Message in bytes: ");
		// for (int i = 0; i < message_length; i++) {
		//     printf("%02x ", (uint8_t)message[i]);
		// }
		// printf("\n");

		printf("%s: %s\n", sender_handle, message);
	}
	else
	{
		printf("No message payload.\n");
	}
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
	receiveMessage(buffer, recvBytes);
}

void sendToServer(int socketNum)
{
	uint8_t buffer[MAXBUF];
	int inputLen = readFromStdin(buffer);
	if (inputLen > 0)
	{
		printf("command entered: %s\n", buffer);
	}
	else
	{
		printf("No valid input received.\n");
	}
	sendCommand(socketNum, (char *)buffer);
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
