#ifndef __WINDOWING_H__
#define __WINDOWING_H__

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdbool.h>
#include <stdint.h>


#define MAX_PACKET_SIZE 1407

typedef struct {
    uint32_t seqNum;
    uint8_t flag;  
    uint8_t data[1400];
    uint16_t payloadLen;
    bool valid;
} Packet;

 
typedef struct {
int upper;
int lower;
int current;
int windowSize;
Packet* buffer;
} SenderWindow;

// Initalize the reciever buffer

typedef struct {
    Packet* buffer;
    int size;
    int nextSeqNum;
    int count;
    int highest;
} ReceiverBuffer;


void initSenderWindow(SenderWindow* window, int windowSize);
void destroySenderWindow(SenderWindow* window);
void addPacketToWindow(SenderWindow* window, Packet* packet);
void removePacketFromWindow(SenderWindow* window, uint32_t seqNum);
void printWindow(SenderWindow* window);
void printAllPacketsInWindow(SenderWindow *window);
int windowIsEmpty(SenderWindow* window);
int windowIsOpen(SenderWindow* window);



void initReceiverBuffer(ReceiverBuffer* receiverBuffer, int windowSize);
void printReceiverBuffer(ReceiverBuffer *buffer);
void addPacketToReceiverBuffer(ReceiverBuffer *buffer, Packet *packet);
void destroyReceiverBuffer(ReceiverBuffer *buffer);







#endif