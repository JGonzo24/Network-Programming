#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>


#include "windowing.h"

/**
 * @brief 
 * 
 * @param window 
 * @param windowSize 
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

/**
 * @brief 
 * 
 * @param window 
 */
void destroySenderWindow(SenderWindow* window) {
    free(window->buffer);
    window->buffer = NULL;
    window->upper = 0;
    window->lower = 0;
    window->current = 0;
    window->windowSize = 0;
}

/**
 * @brief 
 * 
 * @param window 
 * @param packet 
 */
void addPacketToWindow(SenderWindow* window, Packet* packet)
{
    int index = packet->seqNum % window->windowSize;
    window->buffer[index] = *packet;
}

/**
 * @brief 
 * 
 * @param window 
 * @return int 
 */
int windowIsEmpty(SenderWindow* window)
{
    if (window->current == window->upper)
    {
        return 1; // Window is empty
    }
    return 0; // Window is not empty
}

/**
 * @brief 
 * 
 * @param window 
 * @return int 
 */
int windowIsOpen(SenderWindow* window)
{
    return ((int)(window->current - window->lower) < window->windowSize);
}
  
/**
 * @brief 
 * 
 * @param window 
 */
void printWindow(SenderWindow* window)
{
    printf("Sender Window:\n");
    printf("Lower: %d, Upper: %d, Current: %d, Window Size: %d\n", window->lower, window->upper, window->current, window->windowSize);
    for (int i = 0; i < window->windowSize; i++)
    {
        printf("Packet %d: SeqNum: %u, Flag: %u, Valid: %s\n", i, window->buffer[i].seqNum, window->buffer[i].flag, window->buffer[i].valid ? "true" : "false");
    }
}

/**
 * @brief 
 * 
 * @param window 
 */
void printAllPacketsInWindow(SenderWindow *window)
{
    printf("Packets in Sender Window:\n");
    for (int i = 0; i < window->windowSize; i++)
    {
        // print all packets and all the information for that packet 
        printf("Packet %d: SeqNum: %u, Flag: %u, PayloadLen: %u, Valid: %s\n", 
               i, window->buffer[i].seqNum, window->buffer[i].flag, 
               window->buffer[i].payloadLen, window->buffer[i].valid ? "true" : "false");
    }
}


/**
 * @brief 
 * 
 * @param buffer 
 * @param bufferSize 
 */
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
    buffer->highest = 0;

    for (int i = 0; i < bufferSize; i++)
    {
        buffer->buffer[i].valid = false; // Initialize all packets as invalid
    }
}

/**
 * @brief 
 * 
 * @param buffer 
 */
void destroyReceiverBuffer(ReceiverBuffer *buffer)
{
    free(buffer->buffer);
    buffer->buffer = NULL;
    buffer->size = 0;
    buffer->count = 0;
    buffer->nextSeqNum = 0;
}

/**
 * @brief 
 * 
 * @param buffer 
 * @param packet 
 */
void addPacketToReceiverBuffer(ReceiverBuffer *buffer, Packet *packet)
{
    int index = packet->seqNum % buffer->size;
    buffer->buffer[index] = *packet;
}

/**
 * @brief 
 * 
 * @param buffer 
 */
void printReceiverBuffer(ReceiverBuffer *buffer)
{
    printf("Receiver Buffer:\n");
    printf("Next SeqNum: %d, Count: %d\n", buffer->nextSeqNum, buffer->count);
    for (int i = 0; i < buffer->size; i++)
    {
        printf("Packet %d: SeqNum: %u, Flag: %u\n", i, buffer->buffer[i].seqNum, buffer->buffer[i].flag);
    }
}

