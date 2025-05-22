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
int createPDU(uint8_t *pduBuffer, uint32_t sequenceNumber, uint8_t flag, uint8_t *payload, int payloadLen, int windowSize)
{
    int index = 0;
    uint16_t zero = 0;

    // Sequence (network byte order)
    sequenceNumber = htonl(sequenceNumber);
    memcpy(pduBuffer, &sequenceNumber, sizeof(sequenceNumber));
    index += sizeof(sequenceNumber);

    // Reserve space for checksum (set to zero for now)
    memcpy(pduBuffer + index, &zero, sizeof(zero));
    index += sizeof(zero);

    // Flag
    memcpy(pduBuffer + index, &flag, sizeof(flag));
    index += sizeof(flag);

    // Payload
    memcpy(pduBuffer + index, payload, payloadLen);
    index += payloadLen;

    // Window size
    memcpy(pduBuffer + index, &windowSize, sizeof(windowSize));
    index += sizeof(windowSize);

    // Append buffer size (total PDU length so far) as an extra field
    uint16_t bufferSize = htons(index);
    memcpy(pduBuffer + index, &bufferSize, sizeof(bufferSize));
    index += sizeof(bufferSize);

    // Final PDU length includes the new buffer size field
    int pduLength = index;

    // Compute the checksum over the entire PDU (with checksum field as 0)
    uint16_t checksum = in_cksum((unsigned short *)pduBuffer, pduLength);
    // Store the checksum into the checksum field (right after sequence number)
    memcpy(pduBuffer + sizeof(sequenceNumber), &checksum, sizeof(checksum));

    return pduLength;
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
    uint8_t pduCopy[pduLength];
    memcpy(pduCopy, pduBuffer, pduLength);
    memset(pduCopy + sizeof(sequenceNumber), 0, sizeof(uint16_t));
    uint16_t calculatedChecksum = in_cksum((unsigned short *)pduCopy, pduLength);

    if (storedChecksum == calculatedChecksum)
    {
        printf("Checksum: %u (valid)\n", storedChecksum);
    }
    else
    {
        printf("Checksum: %u (invalid) - recalculated: %u\n", storedChecksum, calculatedChecksum);
    }

    // Extract the flag
    uint8_t flag;
    memcpy(&flag, pduBuffer + index, sizeof(flag));
    printf("Flag: %u\n", flag);
    index += sizeof(flag);

    // Extract the payload
    int payloadLen = pduLength - index;
    uint8_t payload[MAXBUF];
    memcpy(payload, pduBuffer + index, payloadLen);
    payload[payloadLen] = '\0'; // Null-terminate the payload string

    printf("Payload: %s\n", payload);
    index += payloadLen;
    printf("Payload Length: %d\n", payloadLen);
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
        memcpy(&transmittedChecksum, buffer + length - sizeof(uint16_t), sizeof(uint16_t));

        // Compute checksum on the portion that excludes the checksum field
        uint16_t computedChecksum = in_cksum(buffer, length - sizeof(uint16_t));

        // Compare (make sure to account for endianness if required)
        return (transmittedChecksum == computedChecksum);
}
