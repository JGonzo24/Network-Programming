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

#define MAXBUF 80


int createPDU(uint8_t* pduBuffer, uint32_t sequenceNumber, uint8_t flag, uint8_t* payload, int payloadLen);
void printPDU(uint8_t* pduBuffer, int pduLength);




#endif // HELPERFUNCTIONS_H
