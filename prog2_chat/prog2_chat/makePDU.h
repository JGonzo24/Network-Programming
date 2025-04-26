#ifndef MAKEPDU_H
#define MAKEPDU_H

#include <stdint.h>
#include "cclient.h"


uint8_t* makeInitialPDU();
MessagePacket_t constructMessagePacket(char destinationHandle[100], int text_message_len, uint8_t text_message[199], int socketNum);
uint8_t* constructMulticastPacket(char* buffer, int socketNum);

#endif // MAKEPDU_H
