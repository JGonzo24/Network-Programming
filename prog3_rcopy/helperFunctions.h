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
#include <arpa/inet.h> // For htons and ntohs
#include <netinet/in.h>

#define MAXBUF 1407
#define MAXPAYLOAD 1400
#define ACKBUF 8


int createPDU(uint8_t* pduBuffer, uint32_t sequenceNumber, uint8_t flag, uint8_t* payload, 
            int payloadLen, int windowSize, int16_t bufferSize);
void printPDU(uint8_t* pduBuffer, int pduLength);
int createAckPDU(uint8_t* ackBuff, uint32_t sequenceNumber, uint8_t flag);
void printBytes(const uint8_t* buffer, int length);
bool verifyChecksum(const uint8_t *buffer, int length);

int createRRPDU(uint8_t* rrBuff, uint32_t sequenceNumber, uint8_t flag, uint32_t rrSequenceNumber);
int createSREJPDU(uint8_t* srejBuff, uint32_t sequenceNumber, uint8_t flag, uint32_t srejSequenceNumber);

int sendRR(int socketNum, struct sockaddr_in6 *server, uint32_t nextDataSeq);
int sendSREJ(int socketNum, struct sockaddr_in6 *server, uint32_t srejSeq);






#endif // HELPERFUNCTIONS_H
