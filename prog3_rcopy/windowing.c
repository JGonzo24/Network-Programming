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


