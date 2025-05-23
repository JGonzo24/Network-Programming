// Client side - UDP Code
// By Hugh Smith	4/1/2017

/**
 * @file rcopy.c
 * @author Joshua Gonzalez
 * @brief This is a simple UDP client program that sends data to a server
 * @version 0.1
 * @date 2025-05-10
 *
 * @copyright Copyright (c) 2025
 *
 */

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

#include "gethostbyname.h"
#include "networks.h"
#include "safeUtil.h"
#include "helperFunctions.h"
#include "cpe464.h"
#include "pollLib.h"
#include "srej.h"

typedef struct
{
	char *fromFileName;
	char *toFileName;
	int windowSize;
	int bufferSize;
	double errorRate;
	char *remoteMachine;
	int remotePort;
} ClientConfig;

void talkToServer(struct sockaddr_in6 *server, ClientConfig config);
int readFromStdin(char *buffer);
int checkArgs(int argc, char *argv[]);
int sendFileName(struct sockaddr_in6 *server, uint8_t *pduBuffer, int pduLength, ClientConfig config, int sequenceNum);
int waitForData(struct sockaddr_in6 *server, uint8_t *pduBuffer, int pduLength, ClientConfig config);
int processData(struct sockaddr_in6 *server, uint8_t *pduBuffer, int pduLength, ClientConfig config);

ClientConfig parseArgs(int argc, char *argv[]);
typedef enum State STATE;
static int attempts = 0;
static int socketNum = 0;

enum State
{
	SEND_FILENAME,
	WAIT_FOR_ACK,
	WAIT_FOR_DATA,
	PROCESS_DATA,
	DONE,
	INORDER,
	BUFFER,
	FLUSH
};

/**
 * @brief This is the main function for the UDP client program
 *
 * @param argc
 * @param argv
 * @return int
 * @note rcopy from-filename to-filename window-size buffer-size error-rate remote-machine remote-port
 */
int main(int argc, char *argv[])
{
	struct sockaddr_in6 server; // Supports 4 and 6 but requires IPv6 struct

	setupPollSet();

	ClientConfig config = parseArgs(argc, argv);

	sendErr_init(config.errorRate, DROP_ON, FLIP_ON, DEBUG_ON, RSEED_OFF);

	// argv[6] is the remote machine name
	socketNum = setupUdpClientToServer(&server, config.remoteMachine, config.remotePort);

	talkToServer(&server, config);

	close(socketNum);

	return 0;
}

/**
 * @brief
 *
 * @param socketNum
 * @param server
 * @param fromFileName
 * @param config
 */

void talkToServer(struct sockaddr_in6 *server, ClientConfig config)
{
	STATE currentState = SEND_FILENAME;

	// Create the filename PDU
	uint8_t pduBuffer[MAXBUF];
	int sequenceNumber = 0;

	// Create the PDU with the filename (flag = 8)
	int pduLength = createPDU(pduBuffer, sequenceNumber, FNAME, (uint8_t*)config.fromFileName, 
					strlen(config.fromFileName), config.windowSize, config.bufferSize);

	// Print the PDU
	printf("printing PDU\n");
	printPDU(pduBuffer, pduLength);

	while (currentState != DONE)
	{
		switch (currentState)
		{
		case SEND_FILENAME:
			// We need to send the filename to the server, so we need what socket and the server address
			printf("Got to the SEND_FILENAME state\n");
			currentState = sendFileName(server, pduBuffer, pduLength, config, sequenceNumber);
			break;

		case WAIT_FOR_ACK:
			printf("Got to the WAIT_FOR_ACK state\n");
			break;

		case WAIT_FOR_DATA:
			// Wait for data from the server
			printf("Got to the WAIT_FOR_DATA state\n");
			currentState = waitForData(server, pduBuffer, pduLength, config);
			break;

		case PROCESS_DATA:
			// Process the data received from the server
			printf("Got to the PROCESS_DATA state\n");
			currentState = processData(server, pduBuffer, pduLength, config);
			break;

		case DONE:
			printf("Got to the DONE state\n");
			break;

		case INORDER:
			printf("GOT TO INORDER STATE\n");
			break;

		case BUFFER:
			printf("GOT TO BUFFER STATE\n");
			break;
		case FLUSH:
			printf("GOT TO FLUSH STATE\n");
			break;
		}
	}
}

int processData(struct sockaddr_in6 *server, uint8_t *pduBuffer, int pduLength, ClientConfig config)
{
	// Now we will process the Data that was just received


	printf("GOT TO PROCESS DATA STATE\n");
	int timeout = 10000;
	int pollGood = pollCall(timeout);
	exit(0);

	return PROCESS_DATA;
}

int waitForData(struct sockaddr_in6 *server, uint8_t *pduBuffer, int pduLength, ClientConfig config)
{
	// First we need to send
	int timeout = 1000;
	int readySocket;
	int received;


	printf("WORKING HERE ON GETTING DATA");

	readySocket = pollCall(timeout);

	if (readySocket != -1)
	{
		uint8_t dataBuffer[MAXBUF];
		socklen_t addrLen = sizeof(*server);
		received = recvfrom(socketNum, dataBuffer, sizeof(dataBuffer), 0, (struct sockaddr *)server, &addrLen);

		if (received < 0)
		{
			perror("Error receiving file data");
			removeFromPollSet(socketNum);
			close(socketNum);
			return DONE;
		}
		printf("Received file data (bytes: %d), transitioning to PROCESS_DATA\n", received);

		// Here you would write the received data to the file, update your file pointer, etc.
		// For now, we simply transition to PROCESS_DATA.
		return PROCESS_DATA;
	}
	else
	{
		// Poll time out
		// close socket
		perror("Timeout waiting for file data in waitForData");
		removeFromPollSet(socketNum);
		close(socketNum);

		// Recreate socket
		socketNum = setupUdpClientToServer(server, config.remoteMachine, config.remotePort);
		addToPollSet(socketNum);

		// Increment counter
		attempts++;
		return SEND_FILENAME;
	}
}

/**
 * @brief This function sends the filename to the server and waits for an ack
 *
 * @param socketNum
 * @param server
 * @return int
 */
int sendFileName(struct sockaddr_in6 *server, uint8_t *pduBuffer, int pduLength, ClientConfig config, int sequenceNum)
{
	int timeout = 1000;

	// print the server port number
	int serverPort = ntohs(server->sin6_port);
	printf("Server port number: %d\n", serverPort);

	addToPollSet(socketNum);

	// We can send up to 10 times
	int readySocket = -1;

	while (attempts < 10)
	{
		printf("Attempt %d to send filename\n", attempts);
		// Send the filename to the server

		int bytesSent = safeSendto(socketNum, pduBuffer, pduLength, 0, (struct sockaddr *)server, sizeof(*server));
		if (bytesSent < 0)
		{
			perror("Error sending filename");
			exit(-1);
		}

		readySocket = pollCall(timeout);

		// If we got a good socket
		if (readySocket != -1)
		{
			// We have a socket that is ready to read now, get the ack
			uint8_t ackBuffer[ACKBUF];

			socklen_t addrLen = sizeof(*server);
			int recieved = recvfrom(socketNum, ackBuffer, sizeof(ackBuffer), 0, (struct sockaddr *)server, &addrLen);

			// Check for errors, if received bad file then go to DONE state
			if (recieved < 0)
			{
				perror("Error receiving ack");
				removeFromPollSet(socketNum);
				close(socketNum);
				exit(-1);
			}

			// Read the Ack
			uint8_t flag;
			memcpy(&flag, ackBuffer + 6, sizeof(flag));

			// going to declare that the flag for a good ack from the server is 35
			if (flag == FNAME_OK)
			{
				printf("Got an ack from the server\n");

				// Now that we have the ack, open the output file (where the data will be stored)
				FILE *f = fopen(config.toFileName, "wb");
				if (f == NULL)
				{
					perror("Error: opening output file");
					removeFromPollSet(socketNum);
					close(socketNum);
					return DONE;
				}

				// if we can open the output file, then send file ok ACK and then poll(1 sec)

				// Declaring file ok ACK == 36
				uint8_t fileOK[ACKBUF];
				createAckPDU(fileOK, sequenceNum, FILE_OK_ACK);
				safeSendto(socketNum, fileOK, sizeof(fileOK), 0, (struct sockaddr *)server, sizeof(*server));
				return WAIT_FOR_DATA;
			}
			else
			{
				// We got a nack, so we need to resend the filename
				printf("Got a nack from the server, resending filename\n");
				attempts++;
				continue;
			}
		}
		else
		{
			// Timeout occurred, resend the filename
			perror("Timeout occurred, resending filename");
			removeFromPollSet(socketNum);
			close(socketNum);

			// Recreate the socket
			socketNum = setupUdpClientToServer(server, config.remoteMachine, config.remotePort);
			addToPollSet(socketNum);
			// Send the filename again
			attempts++;
			return SEND_FILENAME;
		}
	}

	// If the server does not respond after 10 attempts, exit
	if (attempts == 10 && readySocket == -1)
	{
		fprintf(stderr, "Server did not respond after 10 attempts\n");
		removeFromPollSet(socketNum);
		close(socketNum);
		return DONE;
	}

	return 0;
}

int readFromStdin(char *buffer)
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

ClientConfig parseArgs(int argc, char *argv[])
{
	ClientConfig config;

	if (argc != 8)
	{
		fprintf(stderr, "Usage: %s from-filename to-filename window-size buffer-size error-rate remote-machine remote-port\n", argv[0]);
		exit(-1);
	}

	config.fromFileName = argv[1];
	config.toFileName = argv[2];
	config.windowSize = atoi(argv[3]);
	config.bufferSize = atoi(argv[4]);
	if (config.bufferSize < 400 || config.bufferSize > 1400)
	{
		fprintf(stderr, "Buffer size must be between 400 and 1400\n");
		exit(-1);
	}

	config.errorRate = atof(argv[5]);
	if (config.errorRate < 0.0 || config.errorRate >= 1.0)
	{
		fprintf(stderr, "Error rate must be between 0 and 1\n");
		exit(-1);
	}

	config.remoteMachine = argv[6];
	config.remotePort = atoi(argv[7]);

	return config;
}