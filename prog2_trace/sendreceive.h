#ifndef __SENDRECEIVE_H__
#define __SENDRECEIVE_H__


#include <stdint.h>


int sendPDU(int clientSocket, uint8_t * dataBuffer, int lengthOfData);
int recvPDU(int socketNumber, uint8_t * dataBuffer, int bufferSize); 

#endif