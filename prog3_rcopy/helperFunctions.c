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
    // Convert checksum to network byte order
    checksum = htons(checksum);
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
    sequenceNumber = ntohs(sequenceNumber);
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
int createAckPDU(uint8_t* ackBuff, uint32_t sequenceNumber, uint8_t flag)
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


void printBytes(const uint8_t* buffer, int length)
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
    memcpy(&transmittedChecksum, buffer + 4, sizeof(transmittedChecksum));
    transmittedChecksum = ntohs(transmittedChecksum);

    // Create a copy of the buffer and zero out the checksum field
    uint8_t bufferCopy[length];
    memcpy(bufferCopy, buffer, length);
    memset(bufferCopy + 4, 0, sizeof(uint16_t));
    // Calculate the checksum over the entire buffer
    uint16_t calculatedChecksum = in_cksum((unsigned short *)bufferCopy, length);

    return (transmittedChecksum == calculatedChecksum);
}
