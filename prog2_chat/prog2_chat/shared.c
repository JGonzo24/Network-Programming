
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

int makeListPDU(uint8_t *listPDU, int socketNum)
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
        // send a PDU for each handle
        uint8_t handlePDU[MAXBUF];
        uint8_t handleFlag = 0x0C; // Command type for sending back from the server
        handlePDU[0] = handleFlag;
        int handleLen = handleTable[i].handleLen; // Get the handle length

        handlePDU[1] = handleLen; // Set the handle length
        memcpy(handlePDU + 2, handleTable[i].handle, handleLen); // Copy the handle to the PDU

        int handlePDU_len = 2 + handleLen; // Length of the PDU

        int sent = sendPDU(socketNum, handlePDU, handlePDU_len);
        if (sent < 0)
        {
            printf("Error sending handle PDU to socket %d\n", socketNum);
            return -1;
        }
        else
        {
            printf("Handle PDU sent to socket %d\n", socketNum);
        }
    }

    // lastly send a PDU with the flag == 0x0D to indicate end of list
    uint8_t endPDU[MAXBUF];
    uint8_t endFlag = 0x0D; // Command type for sending back from the server
    endPDU[0] = endFlag;
    sendPDU(socketNum, endPDU, 1);
    printf("End PDU sent to socket %d\n", socketNum);
    showHandles();
    return 0;
}

 int sendBroadcastPDU(uint8_t *broadcastPDU, int socketNum, char *message, char *sender_handle)
 {
    // construct the packet
    int offset = 0;
    // First byte is the flag
    uint8_t flag = 0x04 ; // Command type for %b
    broadcastPDU[offset++] = flag;

    // 1 byte for the sender handle length
    broadcastPDU[offset++] = strlen(sender_handle);

    // Copy the sender handle into the packet
    memcpy(broadcastPDU + offset, sender_handle, strlen(sender_handle));
    offset += strlen(sender_handle);

    // Copy the text message into the PDU
    int text_message_len = strlen(message);
    memcpy(broadcastPDU + offset, message, text_message_len);
    offset += text_message_len;
    broadcastPDU[offset] = '\0'; // Null-terminate the message
    offset += 1;
    // Send the broadcast PDU to the server
    int sent = sendPDU(socketNum, broadcastPDU, offset);
    return 0;
 }