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
int createPDU(uint8_t* pduBuffer, uint32_t sequenceNumber, uint8_t flag, uint8_t* payload, int payloadLen)
{
    int index = 0;
    uint16_t zero = 0;

    // Sequence passed in network order
    sequenceNumber = htons(sequenceNumber);
    memcpy(pduBuffer, &sequenceNumber, sizeof(sequenceNumber));
    index += sizeof(sequenceNumber);

    // checksum using in_cksum
    memcpy(pduBuffer + index, &zero, sizeof(zero));
    index += sizeof(zero);

    // Flag
    memcpy(pduBuffer + index, &flag, sizeof(flag));
    index += sizeof(flag);

    // Payload
    memcpy(pduBuffer + index, payload, payloadLen);
    index += payloadLen;

    // PDU length
    int pduLength = index;

    // Compute the checksum over the entire PDU (with checksum field as 0)
    uint16_t checksum = in_cksum((unsigned short*)pduBuffer, pduLength);
    // Store the final checksum in the checksum field
    memcpy(pduBuffer + sizeof(sequenceNumber), &checksum, sizeof(checksum));    

    return pduLength;
}


/**
 * @brief This function prints the PDU (Protocol Data Unit) with the given parameters.
 * 
 * @param pduBuffer 
 * @param pduLength 
 */

void printPDU(uint8_t* pduBuffer, int pduLength)
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
    uint16_t calculatedChecksum = in_cksum((unsigned short*)pduCopy, pduLength);

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