
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdint.h>

#include "shared.h"
#include "handle_table.h"

int makeListPDU(uint8_t* listPDU, int socketNum)
{
    // Creae a list PDU
    /*
    Format: flag = 10
    */
    uint8_t flag = 0x0A; // Command type for %l
    
    // Put the PDU into the buffer
    listPDU[0] = flag;
    return sendPDU(socketNum, listPDU, 1);
}

int sendListPDU(int socketNum)
{
    uint8_t listPDU[MAXBUF];
    int offset = 0;
    // send back to the client a packet with the a flag == 12 for each handle
    uint8_t flag = 0x0B; // Command type for sending back from the server
    listPDU[0] = flag;
    offset++;
    // Get the handles from the handle table
    int handleCount = showHandles();
    printf("Number of handles: %d\n", handleCount);
    printf("----------------------------------");

    uint32_t networkOrderCount = htonl(handleCount);
    memcpy(listPDU + 1, &networkOrderCount, sizeof(uint32_t)); // Copy the handle count to the PDU
    offset += sizeof(uint32_t);

    // sendPDU to the client 
    int sent = sendPDU(socketNum, listPDU, offset);
    if (sent < 0)
    {
        printf("Error sending list PDU to socket %d\n", socketNum);
        return -1;
    }
    else
    {
        printf("List PDU sent to socket %d\n", socketNum);
    }

    // then send the handles in format of for each handle 
    /*
    - Flag
    - handle length
    - handle
    */
    // Get the handles from the handle table

    /*
    Use the struct Handle_t to get the handles
    add the flag in the first byte
    */
   Handle_t *handleTable = getHandleTable();
   int count = getHandleCount();

    if (handleTable == NULL)
    {
        printf("Error: handle table is NULL\n");
        return -1;
    }

    for (int i = 0; i < count; i++)
    {
        uint8_t handlePDU[MAXBUF];
        handlePDU[0] = 0x0C; // Command type for sending back from the server
        DestHandle_t handleInfo;
        handleInfo.dest_handle_len = strlen(handleTable[i].handle);
        memcpy(handleInfo.handle_name, handleTable[i].handle, handleInfo.dest_handle_len);
        handleInfo.handle_name[handleInfo.dest_handle_len] = '\0'; // Null-terminate the handle
        
        // Copy the handle length and handle to the PDU
        handlePDU[1] = handleInfo.dest_handle_len; // Set the handle length
        memcpy(handlePDU + 2, handleInfo.handle_name, handleInfo.dest_handle_len); // Copy the handle
        handlePDU[2 + handleInfo.dest_handle_len] = '\0'; // Null-terminate the handle
        int handlePDU_len = 2 + handleInfo.dest_handle_len; // Length of the PDU

        printPacket(handlePDU, handlePDU_len); // Print the PDU in hex format
        // Send the handle PDU to the client
        int sent = sendPDU(socketNum, handlePDU, handlePDU_len);
    }
    showHandles();
    return 0;
}