#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>


#include "windowing.h"



// Function prototypes






// Function definitions

/*
- build a buffer of data, this is going to be of size WINDOW_SIZE, which is an input to this function
- this must be a flat array 
- circular buffer
*/

void initSenderWindow(SenderWindow* window, int windowSize) {;
    window->lower = 0;
    window->current = 0;
    window->upper = window->lower + windowSize;
    window->windowSize = windowSize;
    window->buffer = (Packet*)malloc(windowSize * sizeof(Packet));

    if (window->buffer == NULL) {
        perror("Failed to allocate memory for sender window buffer");
        exit(EXIT_FAILURE);
    }
}

void destroySenderWindow(SenderWindow* window) {
    free(window->buffer);
    window->buffer = NULL;
    window->upper = 0;
    window->lower = 0;
    window->current = 0;
    window->windowSize = 0;
}

void addPacketToWindow(SenderWindow* window, Packet* packet)
{
    int index = packet->seqNum % window->windowSize;
    window->buffer[index] = *packet;
}

int windowIsFull(SenderWindow* window)
{
    if (window->current >= window->upper)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

void printWindow(SenderWindow* window)
{
    printf("Sender Window:\n");
    printf("Lower: %d, Current: %d, Upper: %d\n", window->lower, window->current, window->upper);
    for (int i = 0; i < window->windowSize; i++)
    {
        printf("Packet %d: SeqNum: %u, Flag: %u\n", i, window->buffer[i].seqNum, window->buffer[i].flag);
    }
}



// Init the receiver buffer, this doesn't need to be a circular buffer
void initReceiverBuffer(ReceiverBuffer *buffer, int bufferSize)
{
    buffer->buffer = (Packet*)calloc(bufferSize, sizeof(Packet));
    if (buffer->buffer == NULL)
    {
        perror("Failed to allocate memory for receiver buffer");
        exit(EXIT_FAILURE);
    }

    buffer->size = bufferSize;
    buffer->count = 0;
    buffer->nextSeqNum = 0;
}


void destroyReceiverBuffer(ReceiverBuffer *buffer)
{
    free(buffer->buffer);
    buffer->buffer = NULL;
    buffer->size = 0;
    buffer->count = 0;
    buffer->nextSeqNum = 0;
}

void addPacketToReceiverBuffer(ReceiverBuffer *buffer, Packet *packet)
{
    int index = packet->seqNum % buffer->size;
    buffer->buffer[index] = *packet;
}

void printReceiverBuffer(ReceiverBuffer *buffer)
{
    printf("Receiver Buffer:\n");
    printf("Next SeqNum: %d, Count: %d\n", buffer->nextSeqNum, buffer->count);
    for (int i = 0; i < buffer->size; i++)
    {
        printf("Packet %d: SeqNum: %u, Flag: %u\n", i, buffer->buffer[i].seqNum, buffer->buffer[i].flag);
    }
}