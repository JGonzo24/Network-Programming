/*// Struct for client information
typedef struct
{
    int socketNum;
    char handle[32];
} Handle_t;

void initHandleTable();
int addHandle(int socketNum, char *handle);
int removeHandle(int socketNum);
int getHandle(int socketNum, char *handle);
int getSocket(char *handle, int *socketNum);
int showHandles();
int resizeHandleTable(int newSize);*/


#include "handle_table.h"
#include "sendreceive.h"
#include "safeUtil.h"
#include "pollLib.h"
#include "networks.h"


#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <poll.h> // Include poll.h for pollfd structure

#define INITIAL_HANDLE_TABLE_SIZE 10
static int tableSize = INITIAL_HANDLE_TABLE_SIZE; // Initial size of the handle table
static int handleCount = 0; // Number of handles currently in use
static Handle_t *handleTable = NULL; // Pointer to the handle table

// Function to initialize the handle table
void initHandleTable()
{

    // Allocate memory for the handle table
    handleTable = (Handle_t *)malloc(sizeof(Handle_t) * INITIAL_HANDLE_TABLE_SIZE);
    if (handleTable == NULL)
    {
        perror("Failed to allocate memory for handle table");
        exit(EXIT_FAILURE);
    }

    // Initialize the handle table
    int tableSize = INITIAL_HANDLE_TABLE_SIZE;
    handleCount = 0;

    for (int i = 0; i < tableSize; i++)
    {
        handleTable[i].socketNum = -1; // Initialize socket number to -1
        memset(handleTable[i].handle, 0, sizeof(handleTable[i].handle)); // Initialize handle to empty string
    }

    printf("Handle table initialized with size %d\n", tableSize);

}

// Function to add a handle to the table
int addHandle(int socketNum, char *handle)
{
    printf("Adding handle: %s with socket number: %d\n", handle, socketNum);

    // Ensure that the handle doesn't already exist in the table
    for (int i = 0; i < handleCount; i++)
    {
        if (strcmp(handleTable[i].handle, handle) == 0)
        {
            printf("Error: the handle %s already exists in the table!\n", handle);
            return -1;
        }
    }

    // Check if the table needs to be resized if we add the new handle
    printf("Handle count: %d, Table size: %d\n", handleCount, tableSize);
    if (handleCount == tableSize)
    {
        // Resize the handle table
        tableSize *= 2;
        resizeHandleTable(tableSize);  // Ensure this function allocates new memory properly
    }

    // The next thing we must do is to actually add the new handle
    // We also need to ensure the handle fits within the allocated space
    if (strlen(handle) >= sizeof(handleTable[handleCount].handle))
    {
        printf("Error: handle size exceeds maximum allowed length\n");
        return -1;
    }

    // Copy handle to the table
    handleTable[handleCount].socketNum = socketNum;
    strncpy(handleTable[handleCount].handle, handle, sizeof(handleTable[handleCount].handle) - 1);

    // Ensure the handle is null-terminated
    handleTable[handleCount].handle[sizeof(handleTable[handleCount].handle) - 1] = '\0';

    handleCount++;

    printf("Handle: %s was added with socket number: %d\n", handle, socketNum);
    return 0;
}


int resizeHandleTable(int newTableSize)
{
    Handle_t *newTable = realloc(handleTable, newTableSize * sizeof(Handle_t));
    if (!newTable)
    {
        perror("Failed to reallocate memory for resizing the table.\n");
        return -1; // Return value that shows realloc() error
    }
    handleTable = newTable;
    int oldSize = tableSize;
    tableSize = newTableSize;

    // Reinitalize the new memory
    for (int i = oldSize; i < tableSize; i++)
    {
        handleTable[i].socketNum = -1;
        memset(handleTable[i].handle, 0, sizeof(handleTable[i].handle));
    }
    printf("Handle table resized to %d entries.\n", tableSize);
    return 0;
}

int removeHandle(int socketNum)
{
    for (int i = 0; i < handleCount; i ++)
    {
        if (handleTable[i].socketNum == socketNum)
        {
            printf("Removing handle: %s with socket number: %d\n", handleTable[i].handle, socketNum);

            // To remove a handle, we would need to effectively need to shift all the other handles
            for (int j = i; j < handleCount -1; j++)
            {
                handleTable[j] = handleTable[j + 1];
             }
            
            // Clear the last entry so we can see that there is nothing there
            handleTable[handleCount-1].socketNum = -1;
            memset(handleTable[handleCount-1].handle,0, sizeof(handleTable[handleCount-1].handle));

            // decrement handle count;
            handleCount --;
            return 0;
        }
    }

    // If you didn't find the socket you were looking for:
    printf("Error: socket number %d was not found in the handle table.\n", socketNum);
    return -1;
}


// returning the socket number from the handle  
int getSocket(char *handle, int *socketNum)
{
    for (int i = 0; i < handleCount; i++)
    {
        if (strcmp(handleTable[i].handle, handle) == 0)
        {
            *socketNum = handleTable[i].socketNum;
            return 0;
        }
    }
    printf("Error: handle %s not found in the table.\n", handle);
    return -1;
}

// Add a validate function to check if the handle exists
int getHandle(int socketNum, char *handle)
{
    for (int i = 0; i < handleCount; i++)
    {
        if (handleTable[i].socketNum == socketNum)
        {
            strncpy(handle, handleTable[i].handle, sizeof(handleTable[i].handle) - 1);
            handle[sizeof(handleTable[i].handle) - 1] = '\0'; // Ensure null termination
            return 0;
        }
    }
    printf("Error: socket number %d not found in the table.\n", socketNum);
    return -1;
}
// Function to show all handles in the table
int showHandles()
{
    printf("Current handles in the table:\n");
    for (int i = 0; i < handleCount; i++)
    {
        printf("Socket: %d, Handle: %s\n", handleTable[i].socketNum, handleTable[i].handle);
    }
    return 0;
}
