/**
 * @file helperFunctions.c
 * @author Joshua Gonzalez
 * @brief This file contains helper functions for the project including the createPDU()
 * @version 0.1
 * @date 2025-05-10
 *
 * @copyright Copyright (c) 2025
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include "checksum.h"
#include "helperFunctions.h"
#include <arpa/inet.h> // For htons and ntohs
#include <stdbool.h>
#include "srej.h"
#include "safeUtil.h"

static uint32_t ackPduSeq = 0;

/**
 * @brief This function creates a PDU (Protocol Data Unit) with the given parameters.
 *
 * @param pduBuffer
 * @param sequenceNumber
 * @param flag
 * @param payload
 * @param payloadLen
 * @return int
 */
int createPDU(uint8_t *pduBuffer, uint32_t sequenceNumber, uint8_t flag, uint8_t *payload, int payloadLen, int windowSize, int16_t bufferSize)
{
    int index = 0;
    uint16_t zero = 0;

    // Sequence Number (4 bytes) in network byte order
    sequenceNumber = htonl(sequenceNumber);
    memcpy(pduBuffer, &sequenceNumber, sizeof(sequenceNumber));
    index += sizeof(sequenceNumber);
    // Reserve space for checksum (2 bytes, set to zero for now)
    memcpy(pduBuffer + index, &zero, sizeof(zero));
    index += sizeof(zero);
    // Flag (1 byte)
    memcpy(pduBuffer + index, &flag, sizeof(flag));
    index += sizeof(flag);

    // Window Size (1 bytes)
    uint8_t netWindowSize = (uint8_t)windowSize;
    memcpy(pduBuffer + index, &netWindowSize, 1);
    index += sizeof(netWindowSize);

    // Buffer Size (2 bytes)
    uint16_t netBufferSize = htons(bufferSize);
    memcpy(pduBuffer + index, &netBufferSize, sizeof(netBufferSize));
    index += sizeof(netBufferSize);

    // Payload (variable length)
    memcpy(pduBuffer + index, payload, payloadLen);
    index += payloadLen;

    // Compute checksum over the entire PDU
    uint16_t checksum = in_cksum((unsigned short *)pduBuffer, index);

    // Store the checksum in the previously reserved spot (after the sequence number)
    memcpy(pduBuffer + sizeof(sequenceNumber), &checksum, sizeof(checksum));

    return index;
}

/**
 * @brief This function prints the PDU (Protocol Data Unit) with the given parameters.
 *
 * @param pduBuffer
 * @param pduLength
 */

void printPDU(uint8_t *pduBuffer, int pduLength)
{
    int index = 0;
    // Extract the sequence number
    uint32_t sequenceNumber;
    memcpy(&sequenceNumber, pduBuffer, sizeof(sequenceNumber));
    sequenceNumber = ntohl(sequenceNumber);
    printf("Sequence Number: %u\n", sequenceNumber);
    index += sizeof(sequenceNumber);

    // Get the stored checksum from the PDU
    uint16_t storedChecksum;
    memcpy(&storedChecksum, pduBuffer + index, sizeof(storedChecksum));
    index += sizeof(storedChecksum);

    // Create a copy and zero out the checksum field for recalculation
    bool verified = verifyChecksum(pduBuffer, pduLength);
    if (verified)
    {
        printf("Checksum verified successfully.\n");
    }
    else
    {
        printf("Checksum verification failed!\n");
    }

    // Extract the flag
    uint8_t flag;
    memcpy(&flag, pduBuffer + index, sizeof(flag));
    printf("Flag: %u\n", flag);
    index += sizeof(flag);

    // Extract the window size
    uint8_t windowSize;
    memcpy(&windowSize, pduBuffer + index, sizeof(windowSize));
    printf("Window Size: %u\n", windowSize);
    index += sizeof(windowSize);

    // Extract the buffer size
    uint16_t bufferSize;
    memcpy(&bufferSize, pduBuffer + index, sizeof(bufferSize));
    bufferSize = ntohs(bufferSize);
    printf("Buffer Size: %u\n", bufferSize);
    index += sizeof(bufferSize);

    // Extract the payload
    uint8_t payload[MAXBUF];
    memcpy(payload, pduBuffer + index, pduLength - index);
    printf("Payload: ");
    printBytes(payload, pduLength - index);
    index += pduLength - index;

    // Print the entire PDU in hex format
    printf("PDU Buffer: ");
    printBytes(pduBuffer, pduLength);
}

/**
 * @brief Create a Ack PDU object
 *
 * @param ackBuff
 * @param sequenceNumber
 * @param flag
 * @return int
 */
int createAckPDU(uint8_t *ackBuff, uint32_t sequenceNumber, uint8_t flag)
{
    int index = 0;
    uint16_t zero = 0;

    // Sequence Number (4 bytes) in network byte order
    sequenceNumber = htonl(sequenceNumber);
    memcpy(ackBuff, &sequenceNumber, sizeof(sequenceNumber));
    index += sizeof(sequenceNumber);

    // Reserve space for checksum (2 bytes, set to zero for now)
    memcpy(ackBuff + index, &zero, sizeof(zero));
    index += sizeof(zero);

    // Flag (1 byte)
    memcpy(ackBuff + index, &flag, sizeof(flag));
    index += sizeof(flag);

    // Pad with one extra byte (0) to make total length 8 bytes
    ackBuff[index] = 0;
    index++;

    // At this point index should equal 8
    int ackLength = index;

    // Compute checksum over the entire ACK packet
    uint16_t checksum = in_cksum((unsigned short *)ackBuff, ackLength);

    // Store the checksum in the previously reserved spot (after the sequence number)
    memcpy(ackBuff + sizeof(sequenceNumber), &checksum, sizeof(checksum));

    return ackLength;
}

void printBytes(const uint8_t *buffer, int length)
{
    for (int i = 0; i < length; i++)
    {
        printf("%02X ", buffer[i]);
    }
    printf("\n");
}

bool verifyChecksum(const uint8_t *buffer, int length)
{
    // Extract the transmitted checksum
    uint16_t transmittedChecksum;

    // +4 to skip the sequence number (4 bytes)
    memcpy(&transmittedChecksum, buffer + 4, sizeof(transmittedChecksum));

    // Create a copy of the buffer and zero out the checksum field
    uint8_t bufferCopy[length];
    memcpy(bufferCopy, buffer, length);
    memset(bufferCopy + 4, 0, sizeof(uint16_t));
    // Calculate the checksum over the entire buffer
    uint16_t calculatedChecksum = in_cksum((unsigned short *)bufferCopy, length);

    if (calculatedChecksum == transmittedChecksum)
    {
        printf("Checksum verification successful.\n");
        return true; // Checksum matches
    }
    else
    {
        printf("Checksum verification failed: calculated %u, transmitted %u\n", calculatedChecksum, transmittedChecksum);
        return false; // Checksum does not match
    }
}

// Create the RR PDU

int createRRPDU(uint8_t *rrBuff, uint32_t sequenceNumber, uint8_t flag, uint32_t rrSequenceNumber)
{
    // First is the packet sequence number
    int index = 0;
    uint16_t zero = 0;
    // Sequence Number (4 bytes) in network byte order
    sequenceNumber = htonl(sequenceNumber);
    memcpy(rrBuff, &sequenceNumber, sizeof(sequenceNumber));
    index += sizeof(sequenceNumber);

    // Reserve space for checksum (2 bytes, set to zero for now)
    memcpy(rrBuff + index, &zero, sizeof(zero));
    index += sizeof(zero);
    // Flag (1 byte)
    memcpy(rrBuff + index, &flag, sizeof(flag));
    index += sizeof(flag);

    // Last is the RR sequence number (the sequence number of packet to be received)
    rrSequenceNumber = htonl(rrSequenceNumber);
    memcpy(rrBuff + index, &rrSequenceNumber, sizeof(rrSequenceNumber));
    index += sizeof(rrSequenceNumber);

    // Add the checksum back in
    uint16_t checksum = in_cksum((unsigned short *)rrBuff, index);
    // Convert checksum to network byte order
    memcpy(rrBuff + sizeof(sequenceNumber), &checksum, sizeof(checksum));

    // Return the length of the RR PDU
    return index;
}

int createSREJPDU(uint8_t *srejBuff, uint32_t sequenceNumber, uint8_t flag, uint32_t srejSequenceNumber)
{
    // First is the packet sequence number
    int index = 0;
    uint16_t zero = 0;

    // Sequence Number (4 bytes) in network byte order
    sequenceNumber = htonl(sequenceNumber);
    memcpy(srejBuff, &sequenceNumber, sizeof(sequenceNumber));
    index += sizeof(sequenceNumber);

    // Reserve space for checksum (2 bytes, set to zero for now)
    memcpy(srejBuff + index, &zero, sizeof(zero));
    index += sizeof(zero);

    // Flag (1 byte)
    memcpy(srejBuff + index, &flag, sizeof(flag));
    index += sizeof(flag);

    // SREJ sequence number (4 bytes) - the sequence number being SREJ'ed
    srejSequenceNumber = htonl(srejSequenceNumber);
    memcpy(srejBuff + index, &srejSequenceNumber, sizeof(srejSequenceNumber));
    index += sizeof(srejSequenceNumber);

    // Compute checksum over the entire SREJ packet
    uint16_t checksum = in_cksum((unsigned short *)srejBuff, index);
    // Store the checksum in the previously reserved spot (after the sequence number)
    memcpy(srejBuff + sizeof(sequenceNumber), &checksum, sizeof(checksum));

    return index; // Should be 11 bytes total
}

int sendRR(int socketNum, struct sockaddr_in6 *server, uint32_t nextDataSeq)
{
    ackPduSeq++;
    uint8_t rrBuff[RR_PDU_LEN];
    int rrLen = createRRPDU(rrBuff, ackPduSeq, RR, nextDataSeq);
    int bytesSent = safeSendto(socketNum, rrBuff, rrLen, 0, (struct sockaddr *)server, sizeof(*server));
    if (bytesSent < 0)
    {
        perror("Error sending RR");
        return -1;
    }
    return bytesSent;
}

int sendSREJ(int socketNum, struct sockaddr_in6 *server, uint32_t srejSeq)
{
    ackPduSeq++;
    uint8_t srejBuff[SREJ_PDU_LEN];
    int srejLen = createSREJPDU(srejBuff, ackPduSeq, SREJ, srejSeq);
    int bytesSent = safeSendto(socketNum, srejBuff, srejLen, 0, (struct sockaddr *)server, sizeof(*server));
    if (bytesSent < 0)
    {
        perror("Error sending SREJ");
        return -1;
    }
    return bytesSent;
}

