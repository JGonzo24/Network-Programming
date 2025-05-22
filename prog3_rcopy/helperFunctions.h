/**
 * @file helperFunctions.h
 * @author Joshua Gonzalez
 * @brief 
 * @version 0.1
 * @date 2025-05-10
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#ifndef HELPERFUNCTIONS_H
#define HELPERFUNCTIONS_H
#include <stdint.h>
#include <sys/types.h>
#include "checksum.h"
#include <string.h>
#include <stdbool.h>

#define MAXBUF 80
#define ACKBUF 8


int createPDU(uint8_t* pduBuffer, uint32_t sequenceNumber, uint8_t flag, uint8_t* payload, int payloadLen, int windowSize);
void printPDU(uint8_t* pduBuffer, int pduLength);
int createAckPDU(uint8_t* ackBuff, uint32_t sequenceNumber, uint8_t flag);
void printBytes(const uint8_t* buffer, int length);
bool verifyChecksum(const uint8_t *buffer, int length);





#endif // HELPERFUNCTIONS_H
