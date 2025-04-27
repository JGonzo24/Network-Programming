#ifndef __SENDRECEIVE_H__
#define __SENDRECEIVE_H__


#include <stdint.h>
// include types
#include <stdio.h>

int sendPDU(int clientSocket, uint8_t * dataBuffer, int lengthOfData);
int recvPDU(int socketNumber, uint8_t * dataBuffer, int bufferSize); 
void printPacket(const uint8_t *packet, size_t length);

#endif