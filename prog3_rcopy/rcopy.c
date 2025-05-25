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
void writeinOrder(uint8_t *buf, int len, uint32_t seq, struct sockaddr_in6 *server);
void invalidatePacket(uint32_t seq);

// above your talkToServer()
#define HDRLEN 10
#define TO_MS 10000
ClientConfig parseArgs(int argc, char *argv[]);
typedef enum State STATE;
static int attempts = 0;
static int socketNum = 0;
static ReceiverBuffer receiverBuffer;
static bool initialized = false;
static bool eofSeen = false;

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

	sendErr_init(config.errorRate, DROP_OFF, FLIP_OFF, DEBUG_ON, RSEED_ON);

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
	uint8_t pduFileName[MAXBUF];
	int sequenceNumber = 0;

	// Create the PDU with the filename (flag = 8)
	int pduLength = createPDU(pduFileName, sequenceNumber, FNAME, (uint8_t *)config.fromFileName,
							  strlen(config.fromFileName), config.windowSize, config.bufferSize);

	// this is where we are going to store the data received from the server
	uint8_t dataBuffer[MAXBUF];
	int receivedDataLen = 0;

	while (currentState != DONE)
	{
		switch (currentState)
		{
		case SEND_FILENAME:
			// We need to send the filename to the server, so we need what socket and the server address
			printf("Got to the SEND_FILENAME state\n");
			currentState = sendFileName(server, pduFileName, pduLength, config, sequenceNumber);
			break;

		case WAIT_FOR_DATA:
			// Wait for data from the server
			printf("Got to the WAIT_FOR_DATA state\n");

			// save the recevied data and the data in the dataBuffer
			currentState = waitForData(server, dataBuffer, &receivedDataLen, config);
			break;

		case PROCESS_DATA:
			// Process the data received from the server
			printf("Got to the PROCESS_DATA state\n");

			// we are giving this state the dataBuffer and the receivedDataLen
			currentState = processData(server, dataBuffer, receivedDataLen, config);
			break;

		case DONE:
			printf("Got to the DONE state\n");
			if (outputFile)
			{
				fclose(outputFile);
				outputFile = NULL;
			}
			break;

		case INORDER:
			printf("GOT TO INORDER STATE\n");
			currentState = inOrder(dataBuffer, receivedDataLen, server, config);
			break;

		case BUFFER:
			printf("GOT TO BUFFER STATE\n");
			currentState = inBuffer(server, config);
			break;

		case FLUSH:
			printf("GOT TO FLUSH STATE\n");
			currentState = flush(server);
			break;

		default:
			printf("Got to an unknown state\n");
			break;
		}
	}
	printf("WE SHOULDNT NOT BE GETTING HERE");
	exit(0);
}

int inBuffer(struct sockaddr_in6 *server, ClientConfig config)
{
	printf("WE GOT TO BUFFERING STATE\n\n");
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
	bufPacketLen = safeRecvfrom(socketNum,
								buf, MAXBUF,
								0,
								(struct sockaddr *)server,
								&addressLen);
	if (bufPacketLen < 0)
	{
		perror("INORDER: recv");
		return DONE;
	}
	if (!verifyChecksum(buf, bufPacketLen))
	{
		printf("INORDER: checksum failed for seq=%u, dropping\n", receiverBuffer.nextSeqNum);
		return INORDER; // immediately go back to waiting
	}

	// 2) Pull out seq & flag
	memcpy(&seq, buf, sizeof(seq));
	seq = ntohl(seq);
	memcpy(&flag, buf + 6, sizeof(flag));

	// 4) In‐order arrival?  (this was the missing one!)
	if (seq == receiverBuffer.nextSeqNum)
	{
		// write it and advance
		fwrite(buf + HDRLEN, 1, bufPacketLen - HDRLEN, outputFile);
		fflush(outputFile);
		printf("BUFFER: flushed seq=%u (%d bytes)\n",
			   seq, bufPacketLen - HDRLEN);
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

		receiverBuffer.count++;
		receiverBuffer.highest = seq;

		sendSREJ(socketNum, server, receiverBuffer.nextSeqNum);
		printf("BUFFER: buffered seq=%u, sent SREJ for %u, highest=%u\n",
			   seq, receiverBuffer.nextSeqNum, receiverBuffer.highest);
		return BUFFER;
	}
	// 6) Duplicate → ignore
	else
	{
		// if seq# is less than nextSeqNum, rr expected nextSeqNum
		if (seq < receiverBuffer.nextSeqNum)
		{
			printf("BUFFER: duplicate seq=%u, ignoring\n", seq);
			sendRR(socketNum, server, receiverBuffer.nextSeqNum);
			printf("BUFFER: sent RR for %u\n", receiverBuffer.nextSeqNum);

			// also srej expected nextSeqNum
			sendSREJ(socketNum, server, receiverBuffer.nextSeqNum);
			printf("BUFFER: sent SREJ for %u\n", receiverBuffer.nextSeqNum);
		}
		else
		{
			printf("BUFFER: unexpected seq=%u, ignoring\n", seq);
		}

		return BUFFER;
	}
}

void writeinOrder(uint8_t *buf, int len, uint32_t seq, struct sockaddr_in6 *server)
{
	// got the missing one, deliver + ack + flush rest
	int w = fwrite(buf, 1, len, outputFile);
	fflush(outputFile);
	printf("INORDER WRITINGG: flushed seq=%u (%d bytes)\n", seq, w);

	invalidatePacket(seq);
	receiverBuffer.nextSeqNum++;
	receiverBuffer.highest = seq;
	invalidatePacket(seq);

	int sent = sendRR(socketNum, server, receiverBuffer.nextSeqNum);

	if (sent < 0)
	{
		perror("INORDER: sendRR failed\n");
		exit(-1);
	}
}

int flush(struct sockaddr_in6 *server)
{
	// As long as we have buffered packets, see if the next‐expected is in the buffer
	while (receiverBuffer.count > 0)
	{
		// compute the slot where nextSeqNum would live
		int idx = receiverBuffer.nextSeqNum % receiverBuffer.size;
		Packet *p = &receiverBuffer.buffer[idx];

		// if that slot really holds our nextSeqNum, deliver it
		if ((uint32_t)p->seqNum == receiverBuffer.nextSeqNum && p->valid)
		{

			int w = fwrite(p->data, 1, p->payloadLen, outputFile);
			fflush(outputFile);
			printf("FLUSH: wrote buffered seq=%u (%d bytes)\n",
				   p->seqNum, w);

			// invalidate the packet in the buffer
			p->valid = false;
			receiverBuffer.nextSeqNum++;
			receiverBuffer.count--;

			// send RR for the next expected sequence number
			if (sendRR(socketNum, server, receiverBuffer.nextSeqNum) < 0)
			{
				perror("FLUSH: sendRR failed");
				exit(-1);
			}
		}
		// else if expected < highest and not valid in the buffer
		else if (receiverBuffer.nextSeqNum < receiverBuffer.highest && !p->valid)
		{
			// srej expected
			printf("FLUSH: seq=%u not in buffer, sending SREJ for %u\n",
				   receiverBuffer.nextSeqNum, receiverBuffer.nextSeqNum);
			if (sendSREJ(socketNum, server, receiverBuffer.nextSeqNum) < 0)
			{
				perror("FLUSH: sendSREJ failed");
				exit(-1);
			}

			// RR expected nextSeqNum
			if (sendRR(socketNum, server, receiverBuffer.nextSeqNum) < 0)
			{
				perror("FLUSH: sendRR failed");
				exit(-1);
			}
			printf("FLUSH: sent SREJ for %u, RR for %u\n",
				   receiverBuffer.nextSeqNum, receiverBuffer.nextSeqNum);

			return BUFFER; // stay in BUFFER state to wait for more packets
		}

		// if expected == highest, write to disk, increment expected, RR expected, go back to inorder
		if (receiverBuffer.nextSeqNum == receiverBuffer.highest)
		{
			printf("FLUSH: nextSeqNum %u is highest, flushing to disk\n", receiverBuffer.nextSeqNum);
			writeinOrder(p->data, p->payloadLen, p->seqNum, server);
			// after writing, we can go back to INORDER state
			return INORDER;
		}
		else
		{
			printf("FLUSH: nextSeqNum %u not highest %u, waiting for more packets\n",
				   receiverBuffer.nextSeqNum, receiverBuffer.highest);
		}
	}
	// after flushing everything in order, go back to INORDER state
	if (eofSeen)
	{
		printf("FLUSH: EOF seen, closing file and exiting.\n");
		fclose(outputFile);
		outputFile = NULL;
		removeFromPollSet(socketNum);
		close(socketNum);
		return DONE;
	}
	printf("FLUSH: No more packets to flush, staying in INORDER state.\n");
	return INORDER;
}

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

		printf("INORDER: Received packet with seqNum %u and flag %u\n", seqNum, flag);

		// Handle EOF flag
		if (flag == EOF_FLAG)
		{
			eofSeen = true;
			
			//create the EOF PDU
			uint8_t eofPDU[MAXBUF];
			int eofPDULen = createPDU(eofPDU, seqNum, EOF_FLAG, NULL, 0, receiverBuffer.size, receiverBuffer.size);
			printf("INORDER: Received EOF flag, making the EOF PDU of length %d\n", eofPDULen);

			// send the EOF PDU to the server
			if (safeSendto(socketNum, eofPDU, eofPDULen, 0, (struct sockaddr *)server, addressLen) < 0)
			{
				perror("INORDER: send EOF failed");
				exit(-1);
			}
			printf("INORDER: Sent EOF PDU to server, closing file and exiting.\n");
			// Ensure that all buffered packets are flushed before exiting
			printReceiverBuffer(&receiverBuffer);
			return DONE;
		}

		// If the sequence number matches the next expected sequence number
		if (seqNum == receiverBuffer.nextSeqNum)
		{
			printf("INORDER: Packet %u is in order, writing to file.\n", seqNum);
			fwrite(inOrderPDU + HDRLEN, 1, inOrderPDULen - HDRLEN, outputFile);
			fflush(outputFile);

			// Invalidate the packet and update the next expected sequence number
			receiverBuffer.nextSeqNum++;
			printf("INORDER: Updated nextSeqNum to %u\n", receiverBuffer.nextSeqNum);

			if (sendRR(socketNum, server, receiverBuffer.nextSeqNum) < 0)
			{
				perror("INORDER: sendRR failed");
				exit(-1);
			}

			continue; // Continue to process more packets
		}
		else if (seqNum > receiverBuffer.nextSeqNum)
		{
			printf("INORDER: Packet %u is out of order, buffering and sending SREJ for %u\n", seqNum, receiverBuffer.nextSeqNum);

			// Buffer the out-of-order packet
			addPacketToReceiverBuffer(&receiverBuffer, &(Packet){.seqNum = seqNum, .flag = DATA, .payloadLen = inOrderPDULen - HDRLEN, .valid = true});
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
		printf("Receiver buffer initialized with size %d\n", receiverBuffer.size);
	}

	// Extract the sequence number and flag from the data PDU.
	uint32_t seqNum;
	uint8_t flag;
	memcpy(&seqNum, pduBuffer, sizeof(seqNum));
	seqNum = ntohl(seqNum);
	memcpy(&flag, pduBuffer + 6, sizeof(flag));

	// Handle EOF flag
	if (flag == EOF_FLAG)
	{
		eofSeen = true;
		printf("Received the EOF flag, still waiting for RR\n");
		// Create the EOF PDU
		uint8_t eofPDU[MAXBUF];
		int eofPDULen = createPDU(eofPDU, seqNum, EOF_FLAG, NULL, 0, receiverBuffer.size, receiverBuffer.size);
		printf("Creating EOF PDU of length %d\n", eofPDULen);
		// Send the EOF PDU to the server
		if (safeSendto(socketNum, eofPDU, eofPDULen, 0, (struct sockaddr *)server, sizeof(*server)) < 0)
		{
			perror("Error sending EOF PDU");
			exit(-1);
		}
		printReceiverBuffer(&receiverBuffer);
		
		while (receiverBuffer.count > 0)
		{
			// Flush any buffered packets
			int flushState = flush(server);
			if (flushState == DONE)
			{
				printf("Flushed all buffered packets, exiting.\n");
				fclose(outputFile);
				outputFile = NULL;
				removeFromPollSet(socketNum);
				close(socketNum);
				return DONE;
			}
		}
	}

	printf("INTIAL RECEIVER BUFFER:\n");
	printReceiverBuffer(&receiverBuffer);

	// Process DATA flag.
	if (flag == DATA)
	{
		if (seqNum == receiverBuffer.nextSeqNum)
		{
			// return INORDER STATE
			printf("Received in-order packet with seqNum %u\n", seqNum);
			// we need to write this buffer to the output file
			fwrite(pduBuffer + HDRLEN, 1, pduLength - HDRLEN, outputFile);
			fflush(outputFile);
			printf("Wrote %d bytes to output file for seqNum %u\n", pduLength - HDRLEN, seqNum);
			// Invalidate the packet and update the next expected sequence number
			receiverBuffer.nextSeqNum++;
			printf("Updated nextSeqNum to %u\n", receiverBuffer.nextSeqNum);
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
			printf("Received out-of-order packet with seqNum %u, buffering it\n", seqNum);
			Packet p = {
				.seqNum = seqNum,
				.flag = DATA,
				.payloadLen = pduLength - HDRLEN,
				.valid = true};
			memcpy(p.data, pduBuffer + HDRLEN, pduLength - HDRLEN);
			addPacketToReceiverBuffer(&receiverBuffer, &p);
			printReceiverBuffer(&receiverBuffer);

			sendSREJ(socketNum, server, receiverBuffer.nextSeqNum); // Send SREJ for the next expected sequence number
			return BUFFER;											// Transition to BUFFER state to handle buffered packets
		}
		else if (seqNum < receiverBuffer.nextSeqNum)
		{
			printf("Received duplicate packet with seqNum %u, ignoring it\n", seqNum);
			if (sendRR(socketNum, server, receiverBuffer.nextSeqNum) < 0)
			{
				perror("Error sending RR for duplicate packet");
				exit(-1);
			}
			return INORDER; // Stay in INORDER state to process more packets
		}
	}
	printf("PROCESS DATA SHOULD NOT GET HERE\n");
	return DONE;
}

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

		if (received < 0)
		{
			perror("Error receiving file data");
			removeFromPollSet(socketNum);
			close(socketNum);
			return DONE;
		}
		printf("Received file data (bytes: %d), transitioning to PROCESS_DATA\n", received);

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
			int recieved = safeRecvfrom(socketNum, ackBuffer, sizeof(ackBuffer), 0, (struct sockaddr *)server, &addrLen);

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
				int ctrlLen = createAckPDU(fileOK, sequenceNum, FILE_OK_ACK);
				safeSendto(socketNum, fileOK, ctrlLen, 0, (struct sockaddr *)server, sizeof(*server));
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

void invalidatePacket(uint32_t seq)
{
	int index = seq % receiverBuffer.size;
	receiverBuffer.buffer[index].valid = false;
}