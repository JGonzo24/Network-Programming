#ifndef HANDLE_TABLE_H
#define HANDLE_TABLE_H

#include <stdio.h>

#include <stdint.h>
#include <stdlib.h>

// Struct for client information
typedef struct
{
    int handleLen;
    char handle[32];
    int socketNum;

} Handle_t;

void initHandleTable();
int addHandle(int socketNum, char *handle, int handleLen);
int removeHandle(int socketNum);
int getHandle(int socketNum, char *handle);
int getSocket(char *handle, int *socketNum);
int showHandles();
int resizeHandleTable(int newSize);
Handle_t *getHandleTable();
int getHandleCount();

#endif