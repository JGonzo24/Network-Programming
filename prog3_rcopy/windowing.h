#ifndef __WINDOWING_H__
#define __WINDOWING_H__

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdint.h>


#define MAX_PACKET_SIZE 1407

typedef struct {
    uint32_t seqNum;
    uint8_t flag;  
    uint8_t data[1400];
} Packet;

 
typedef struct {
int upper;
int lower;
int current;
int windowSize;
Packet* buffer;
} SenderWindow;


void initSenderWindow(SenderWindow* window, int windowSize);
void destroySenderWindow(SenderWindow* window);
void addPacketToWindow(SenderWindow* window, Packet* packet);
void removePacketFromWindow(SenderWindow* window, uint32_t seqNum);
void printWindow(SenderWindow* window);
int windowIsFull(SenderWindow* window);



#endif