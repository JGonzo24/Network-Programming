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
#include "windowing.h"

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
int waitForData(struct sockaddr_in6 *server, uint8_t *pduBuffer, int *pduLength, ClientConfig config);
int processData(struct sockaddr_in6 *server, uint8_t *pduBuffer, int pduLength, ClientConfig config);
int inOrder(uint8_t *inOrderPDU, int inOrderPDULen, struct sockaddr_in6 *server, ClientConfig config);
int inBuffer(struct sockaddr_in6 *server, ClientConfig config);
int flush(struct sockaddr_in6 *server);
void invalidatePacket(uint32_t seq);


#define HDRLEN 10
#define TO_MS 10000
ClientConfig parseArgs(int argc, char *argv[]);
typedef enum State STATE;
static int attempts = 0;
static int socketNum = 0;
static ReceiverBuffer receiverBuffer;
static bool initialized = false;
static FILE *outputFile = NULL;

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

	sendErr_init(config.errorRate, DROP_ON, FLIP_ON, DEBUG_ON, RSEED_ON);

	// argv[6] is the remote machine name
	socketNum = setupUdpClientToServer(&server, config.remoteMachine, config.remotePort);

	talkToServer(&server, config);

	close(socketNum);

	return 0;
}

/**
 * @brief 
 * 
 * @param server 
 * @param config 
 */
void talkToServer(struct sockaddr_in6 *server, ClientConfig config)
{
	STATE currentState = SEND_FILENAME;

	// Create the filename PDU
	uint8_t pduFileName[MAXBUF];
	int sequenceNumber = 0;

	// Create the PDU with the filename (flag = 8)
	int pduLength = createPDU(pduFileName, sequenceNumber, FNAME, (uint8_t *)config.fromFileName, strlen(config.fromFileName), config.windowSize, config.bufferSize);

	// this is where we are going to store the data received from the server
	uint8_t dataBuffer[MAXBUF];
	int receivedDataLen = 0;

	while (currentState != DONE)
	{
		switch (currentState)
		{
		case SEND_FILENAME:
			// We need to send the filename to the server, so we need what socket and the server address
			currentState = sendFileName(server, pduFileName, pduLength, config, sequenceNumber);
			break;

		case WAIT_FOR_DATA:
			// save the recevied data and the data in the dataBuffer
			currentState = waitForData(server, dataBuffer, &receivedDataLen, config);
			break;

		case PROCESS_DATA:
			// we are giving this state the dataBuffer and the receivedDataLen
			currentState = processData(server, dataBuffer, receivedDataLen, config);
			break;

		case INORDER:
			currentState = inOrder(dataBuffer, receivedDataLen, server, config);
			break;

		case BUFFER:
			currentState = inBuffer(server, config);
			break;

		case FLUSH:
			currentState = flush(server);
			break;

		default:
			currentState = DONE;
			break;
		}
	}

	// we are now DONE
	printf("All done, closing output file and exiting.\n");

	if (outputFile != NULL)
	{
		fclose(outputFile);
		outputFile = NULL;
	}
	destroyReceiverBuffer(&receiverBuffer);
	removeFromPollSet(socketNum);
	close(socketNum);
	exit(0);
}
/**
 * @brief 
 * 
 * @param server 
 * @param config 
 * @return int 
 */
int inBuffer(struct sockaddr_in6 *server, ClientConfig config)
{
	uint8_t buf[MAXBUF];
	int addressLen = sizeof(*server);
	int bufPacketLen;
	uint32_t seq;
	uint8_t flag;

	int ready = pollCall(TO_MS);
	if (ready <= 0)
	{
		printf("BUFFER: no pkt for 10s → DONE\n");
		return DONE;
	}
	if ((bufPacketLen = safeRecvfrom(socketNum, buf, MAXBUF, 0, (struct sockaddr *)server, &addressLen)) < 0)
	{
		perror("BUFFER: recvfrom failed");
		return DONE;
	}

	if (!verifyChecksum(buf, bufPacketLen))
	{
		printf("BUFFER: checksum failed, ignoring packet\n");
		return BUFFER;
	}

	// 2) Pull out seq & flag
	memcpy(&seq, buf, sizeof(seq));
	seq = ntohl(seq);
	memcpy(&flag, buf + 6, sizeof(flag));

	if (seq == receiverBuffer.nextSeqNum)
	{
		if (flag == EOF_FLAG)
		{
			return DONE;
		}

		size_t written = fwrite(buf + HDRLEN, 1, bufPacketLen - HDRLEN, outputFile);
		// print out the whole packet that you just wrote
		printBytes(buf + HDRLEN, bufPacketLen - HDRLEN);

		if (written != bufPacketLen - HDRLEN)
		{
			perror("INORDER: fwrite failed");
			printf("Expected to write %d bytes, but wrote %zu bytes\n", bufPacketLen - HDRLEN, written);
			exit(-1);
		}
		fflush(outputFile);

		invalidatePacket(seq);

		printf("BUFFER: flushed seq=%u (%d bytes)\n", seq, bufPacketLen - HDRLEN);
		receiverBuffer.nextSeqNum++;

		// now we can fall through to INORDER to flush any more
		return FLUSH;
	}
	// 5) Future packet → buffer + SREJ
	else if (seq > receiverBuffer.nextSeqNum)
	{
		Packet p = {
			.seqNum = seq,
			.flag = DATA,
			.payloadLen = bufPacketLen - HDRLEN,
			.valid = true};

		memcpy(p.data, buf + HDRLEN, bufPacketLen - HDRLEN);
		addPacketToReceiverBuffer(&receiverBuffer, &p);
		printReceiverBuffer(&receiverBuffer);

		receiverBuffer.highest = seq;
		return BUFFER;
	}
	// 6) Duplicate → ignore
	else
	{
		// if seq# is less than nextSeqNum, rr expected nextSeqNum
		if (seq < receiverBuffer.nextSeqNum)
		{
			sendRR(socketNum, server, receiverBuffer.nextSeqNum);
			sendSREJ(socketNum, server, receiverBuffer.nextSeqNum);
		}
		else
		{
			printf("BUFFER: unexpected seq=%u, ignoring\n", seq);
		}

		return BUFFER;
	}
}



/**
 * @brief 
 * 
 * @param server 
 * @return int 
 */
int flush(struct sockaddr_in6 *server)
{
	printReceiverBuffer(&receiverBuffer);
	// Try to flush as many in‐order packets as we have
	for (;;)
	{
		uint32_t exp = receiverBuffer.nextSeqNum;
		int idx = exp % receiverBuffer.size;
		Packet *p = &receiverBuffer.buffer[idx];

		// if the slot doesn’t contain exactly the packet we expect, we’re done
		if (!p->valid || p->seqNum != exp)
		{
			break;
		}

		
		// check if the packet is EOF
		if (p->flag == EOF_FLAG)
		{
			printf("FLUSH: EOF packet received, closing output file\n");
			return DONE;
		}

		size_t written = fwrite(p->data, 1, p->payloadLen, outputFile);
		printBytes(p->data, p->payloadLen);
		fflush(outputFile);

		if (written != p->payloadLen)
		{
			perror("INORDER: fwrite failed");
			printf("Expected to write %d bytes, but wrote %zu bytes\n", p->payloadLen, written);
			exit(-1);
		}

		// mark that slot empty
		p->valid = false;

		// advance to next expected sequence
		receiverBuffer.nextSeqNum++;
	}

	// If we still have buffered packets *beyond* what we've just flushed, we need SREJ+RR
	if (receiverBuffer.nextSeqNum < receiverBuffer.highest &&
		(receiverBuffer.buffer[receiverBuffer.nextSeqNum % receiverBuffer.size].valid) == false)
	{

		printf("FLUSH: missing seq=%u, sending SREJ + RR\n", receiverBuffer.nextSeqNum);
		if (sendSREJ(socketNum, server, receiverBuffer.nextSeqNum) < 0)
		{
			perror("FLUSH: sendSREJ failed");
			exit(-1);
		}
		if (sendRR(socketNum, server, receiverBuffer.nextSeqNum) < 0)
		{
			perror("FLUSH: sendRR failed");
			exit(-1);
		}
		return BUFFER;
	}

	// If we've flushed everything we ever expect (nextSeq==highest), hand back to INORDER
	else if (receiverBuffer.nextSeqNum == receiverBuffer.highest)
	{
		printf("FLUSH: nextSeqNum=%u == highest=%u → back to INORDER\n",
			   receiverBuffer.nextSeqNum, receiverBuffer.highest);

		// write to disk
		size_t dataLen = receiverBuffer.buffer[receiverBuffer.nextSeqNum % receiverBuffer.size].payloadLen;

		// check if the packet is EOF
		if (receiverBuffer.buffer[receiverBuffer.nextSeqNum % receiverBuffer.size].flag == EOF_FLAG)
		{
			printf("FLUSH: EOF packet received, closing output file\n");
			fclose(outputFile);
			outputFile = NULL;
			removeFromPollSet(socketNum);
			close(socketNum);
			return DONE;
		}

		size_t written = fwrite(receiverBuffer.buffer[receiverBuffer.nextSeqNum % receiverBuffer.size].data, 1, dataLen, outputFile);
		printBytes(receiverBuffer.buffer[receiverBuffer.nextSeqNum % receiverBuffer.size].data, dataLen);

		if (written < 0)
		{
			perror("FLUSH: fwrite failed");
			exit(-1);
		}
		if (written != dataLen)
		{
			perror("INORDER: fwrite failed");
			printf("Expected to write %zu bytes, but wrote %zu bytes\n", dataLen, written);
			exit(-1);
		}
		fflush(outputFile);
		// invalidate the packet
		invalidatePacket(receiverBuffer.nextSeqNum);
		receiverBuffer.nextSeqNum++;

		// rr for nextSeqNum
		if (sendRR(socketNum, server, receiverBuffer.nextSeqNum) < 0)
		{
			perror("FLUSH: sendRR failed");
			exit(-1);
		}
		return INORDER;
	}

	// 5) Otherwise, we still need more packets before we can flush again
	return INORDER;
}

/**
 * @brief 
 * 
 * @param inOrderPDU 
 * @param inOrderPDULen 
 * @param server 
 * @param config 
 * @return int 
 */
int inOrder(uint8_t *inOrderPDU, int inOrderPDULen, struct sockaddr_in6 *server, ClientConfig config)
{
	uint32_t seqNum;
	uint8_t flag;
	int addressLen = sizeof(*server);


	while (1)
	{
		int ready = pollCall(TO_MS);
		if (ready < 0)
		{
			return DONE;
		}

		int newPacketLen = safeRecvfrom(socketNum, inOrderPDU, MAXBUF, 0, (struct sockaddr *)server, &addressLen);
		if (newPacketLen < 0)
		{
			perror("INORDER: recv");
			return DONE;
		}

		if (!verifyChecksum(inOrderPDU, newPacketLen))
		{
			printf("INORDER: checksum failed for seq=%u, dropping\n", receiverBuffer.nextSeqNum);
			continue;
		}

		// Extract the sequence number and flag from the in-order PDU.
		memcpy(&seqNum, inOrderPDU, sizeof(seqNum));
		seqNum = ntohl(seqNum);
		memcpy(&flag, inOrderPDU + 6, sizeof(flag));

		// If the sequence number matches the next expected sequence number
		if (seqNum == receiverBuffer.nextSeqNum)
		{

			receiverBuffer.highest = seqNum;

			// check if the packet is EOF
			if (flag == EOF_FLAG)
			{
				sendRR(socketNum, server, receiverBuffer.nextSeqNum + 1); // Send RR for the next expected sequence number
				continue;
			}

			size_t written = fwrite(inOrderPDU + HDRLEN, 1, newPacketLen - HDRLEN, outputFile);
			if (written != newPacketLen - HDRLEN)
			{
				perror("INORDER: fwrite failed");
				printf("Expected to write %d bytes, but wrote %zu bytes\n", newPacketLen - HDRLEN, written);
				exit(-1);
			}
			fflush(outputFile);

			receiverBuffer.nextSeqNum++;

			if (sendRR(socketNum, server, receiverBuffer.nextSeqNum) < 0)
			{
				perror("INORDER: sendRR failed");
				exit(-1);
			}

			continue; // Continue to process more packets
		}
		else if (seqNum > receiverBuffer.nextSeqNum)
		{
			// Buffer the out-of-order packet
			receiverBuffer.highest = seqNum;
			Packet p = {
				.seqNum = seqNum,
				.flag = DATA,
				.payloadLen = newPacketLen - HDRLEN,
				.valid = true};
			memcpy(p.data, inOrderPDU + HDRLEN, newPacketLen - HDRLEN);
			addPacketToReceiverBuffer(&receiverBuffer, &p);

			sendSREJ(socketNum, server, receiverBuffer.nextSeqNum); // Send SREJ for the next expected sequence number
			return BUFFER;											// Transition to BUFFER state to handle buffered packets
		}
		else if (seqNum < receiverBuffer.nextSeqNum)
		{
			if (sendRR(socketNum, server, receiverBuffer.nextSeqNum) < 0)
			{
				perror("INORDER: sendRR failed");
				exit(-1);
			}
		}
		return INORDER; // Stay in INORDER state to process more packets
	}
}

/**
 * @brief 
 * 
 * @param server 
 * @param pduBuffer 
 * @param pduLength 
 * @param config 
 * @return int 
 */
int processData(struct sockaddr_in6 *server, uint8_t *pduBuffer, int pduLength, ClientConfig config)
{
	// First initialize the receiver buffer if not already done.
	if (!initialized)
	{
		initialized = true;
		initReceiverBuffer(&receiverBuffer, config.windowSize);
		outputFile = fopen(config.toFileName, "wb");
		if (!outputFile)
		{
			perror("Error opening output file");
			removeFromPollSet(socketNum);
			close(socketNum);
			return DONE;
		}
	}

	// Extract the sequence number and flag from the data PDU.
	uint32_t seqNum;
	uint8_t flag;
	memcpy(&seqNum, pduBuffer, sizeof(seqNum));
	seqNum = ntohl(seqNum);
	memcpy(&flag, pduBuffer + 6, sizeof(flag));


	// Process DATA flag.
	if (flag == DATA)
	{
		// If the sequence number matches the next expected sequence number
		if (seqNum == receiverBuffer.nextSeqNum)
		{

			if (flag == EOF_FLAG)
			{
				printf("Received EOF packet, closing output file\n");
				return DONE;
			}

			size_t written = fwrite(pduBuffer + HDRLEN, 1, pduLength - HDRLEN, outputFile);

			if (written != pduLength - HDRLEN)
			{
				perror("INORDER: fwrite failed");
				printf("Expected to write %d bytes, but wrote %zu bytes\n", pduLength - HDRLEN, written);
				exit(-1);
			}
			fflush(outputFile);

			// Invalidate the packet and update the next expected sequence number
			receiverBuffer.nextSeqNum++;
			if (sendRR(socketNum, server, receiverBuffer.nextSeqNum) < 0)
			{
				perror("Error sending RR");
				exit(-1);
			}

			return INORDER;
		}
		else if (seqNum > receiverBuffer.nextSeqNum)
		{
			// Buffer the out-of-order packet
			Packet p = {
				.seqNum = seqNum,
				.flag = DATA,
				.payloadLen = pduLength - HDRLEN,
				.valid = true};
			memcpy(p.data, pduBuffer + HDRLEN, pduLength - HDRLEN);
			addPacketToReceiverBuffer(&receiverBuffer, &p);
			printReceiverBuffer(&receiverBuffer);

			receiverBuffer.highest = seqNum;
			sendSREJ(socketNum, server, receiverBuffer.nextSeqNum); // Send SREJ for the next expected sequence number
			return BUFFER;											// Transition to BUFFER state to handle buffered packets
		}
		else if (seqNum < receiverBuffer.nextSeqNum)
		{
			if (sendRR(socketNum, server, receiverBuffer.nextSeqNum) < 0)
			{
				perror("Error sending RR for duplicate packet");
				exit(-1);
			}
			return INORDER; // Stay in INORDER state to process more packets
		}
	}
	return DONE;
}

/**
 * @brief 
 * 
 * @param server 
 * @param pduBuffer 
 * @param pduLength 
 * @param config 
 * @return int 
 */
int waitForData(struct sockaddr_in6 *server, uint8_t *pduBuffer, int *pduLength, ClientConfig config)
{
	// First we need to send
	int timeout = 1000;
	int readySocket;
	int received;

	readySocket = pollCall(timeout);

	if (readySocket != -1)
	{
		// save the data into the pduBuffer
		int addrLen = sizeof(*server);
		received = safeRecvfrom(socketNum, pduBuffer, MAXBUF, 0, (struct sockaddr *)server, &addrLen);
		if (!verifyChecksum(pduBuffer, received))
		{
			return WAIT_FOR_DATA; // Stay in WAIT_FOR_DATA state to wait for more data
		}

		if (received < 0)
		{
			perror("Error receiving file data");
			removeFromPollSet(socketNum);
			close(socketNum);
			return DONE;
		}

		// save the length of the received data
		*pduLength = received;

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

			int addrLen = sizeof(*server);
			int received = safeRecvfrom(socketNum, ackBuffer, sizeof(ackBuffer), 0, (struct sockaddr *)server, &addrLen);

			if (!verifyChecksum(ackBuffer, received))
			{
				printf("Checksum verification failed for received ack, ignoring packet.\n");
				continue; // Stay in SEND_FILENAME state to wait for a valid ack
			}
			// Check for errors, if received bad file then go to DONE state
			if (received < 0)
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
				// Now that we have the ack, open the output file (where the data will be stored)
				FILE *f = fopen(config.toFileName, "wb");
				if (f == NULL)
				{
					printf("Error on open of output file %s\n", config.toFileName);
					return DONE;
				}
				fclose(f);

				// Declaring file ok ACK == 36
				uint8_t fileOK[ACKBUF];
				int ctrlLen = createAckPDU(fileOK, sequenceNum, FILE_OK_ACK);
				safeSendto(socketNum, fileOK, ctrlLen, 0, (struct sockaddr *)server, sizeof(*server));
				return WAIT_FOR_DATA;
			}
			else
			{
				printf("Got a nack from the server, resending filename\n");
				attempts++;
				continue;
			}
		}
		else
		{
			removeFromPollSet(socketNum);
			close(socketNum);

			// Recreate the socket
			socketNum = setupUdpClientToServer(server, config.remoteMachine, config.remotePort);
			addToPollSet(socketNum);
			// Send the filename again
			attempts++;
			continue;
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

/**
 * @brief 
 * 
 * @param buffer 
 * @return int 
 */
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

/**
 * @brief 
 * 
 * @param argc 
 * @param argv 
 * @return ClientConfig 
 */
ClientConfig parseArgs(int argc, char *argv[])
{
	ClientConfig config;

	if (argc != 8)
	{
		fprintf(stderr, "Usage: %s from-filename to-filename window-size buffer-size error-rate remote-machine remote-port\n", argv[0]);
		exit(-1);
	}

	// if either of the file names are too long, exit
	if (strlen(argv[1]) > 100 || strlen(argv[2]) > 100)
	{
		fprintf(stderr, "File names must be less than %d characters\n", 100);
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

/**
 * @brief 
 * 
 * @param seq 
 */
void invalidatePacket(uint32_t seq)
{
	int index = seq % receiverBuffer.size;
	receiverBuffer.buffer[index].valid = false;
}