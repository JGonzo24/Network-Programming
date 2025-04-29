#ifndef MAKEPDU_H
#define MAKEPDU_H

#include <stdint.h>
#include "cclient.h"


uint8_t* makeInitialPDU();
MessagePacket_t constructMessagePacket(char destinationHandle[100], int text_message_len, uint8_t text_message[199], int socketNum);
int constructMulticastPDU(uint8_t* multicastPDU, int socketNum, char* sender_handle,int numHandles, DestHandle_t* handles, char* message);

#endif // MAKEPDU_H
