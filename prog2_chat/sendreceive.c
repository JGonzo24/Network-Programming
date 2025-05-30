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
#include <errno.h>

#include "safeUtil.h"
#include "sendreceive.h"
#include "cclient.h"

/*
-- buffer does not include the length of the PDU
-- This function sends an application level PDU in one send()
1. Create the application level PDU
2. send() the PDU in one send() call
3. Check for errors on send() (<0)

-- Do not assume the data buffer is a C null terminated string, use memcpy()
-- Return value: number of bytes sent (not including the the 2 byte length field)
*/
int sendPDU(int clientSocket, uint8_t* dataBuffer, int lengthOfData)
{
    // First we need to add the length of the PDU to the beginning of the dataBuffer
    // The length of the PDU is 2 bytes
    uint16_t pdu_length = lengthOfData + 2; // length
    // Create a buffer to hold the PDU
    uint8_t pdu[pdu_length];
    // Set the first two bytes for the length of the PDU

    uint16_t length_bytes = htons(pdu_length);
    memcpy(pdu, &length_bytes, sizeof(length_bytes));

    // Set the flag in the PDU

    // put the rest of the data into the PDU
    memcpy(pdu + 2, dataBuffer, lengthOfData);
    
    // send the entire PDU using send()
    int numBytesSent = safeSend(clientSocket, pdu, pdu_length, 0);

    // Check if the number of bytes sent is equal to the length of the data

    if (numBytesSent == pdu_length) {
        return lengthOfData;
    } else {
        printf("Error: Sent %d bytes, but expected to send %d bytes\n", numBytesSent, lengthOfData);
        return -1;  // Error in sending
    }
}

/*
-- recv() application level PDU
-- Check the recv() for errors (return value < 0)
-- checking for closed connections (return value == 0)
-- Does the two step recv() process (using MSG_WAITALL)
-- You need the MSG_WAITALL on both recv() calls
-- return value: number of data bytes received (does not include the 2 byte length)
-- return value == 0 if the the connection was closed by the other end
-- If you do not get the first 2 bytes do not do the second call and go back for more
-- will return the bytes received in the second recv()]
-- check that the bufferSize is big enough to receive the PDU, if not print error and exit the program
-- 
*/

int recvPDU(int socketNumber, uint8_t *dataBuffer, int bufferSize) {

    uint8_t lengthBuffer[2];  // Buffer to hold the 2-byte PDU length
    int received_bytes = safeRecv(socketNumber, lengthBuffer, 2, MSG_WAITALL);

    if (received_bytes == 0) {
        printf("There is nothing to read! Connection closed by client.\n");
        return 0;
    }
    if (received_bytes < 0) {
        perror("recv failed");
        return -1;
    }
    // Convert the 2-byte length from network byte order to host byte order
    uint16_t pdu_length_read = ntohs(*(uint16_t*)lengthBuffer);
    
    //printf("PDU length read from recvPDU(): %d\n", pdu_length_read);

    // Check if the length is smaller than the buffer size
    if (pdu_length_read > bufferSize) {
        printf("Error: The buffer is not large enough to fit the PDU! Needs at least %d bytes.\n", pdu_length_read);
        return -1;  // Exit if buffer size is too small
    }    
    // 2. Receive the rest of the data (the payload)
    received_bytes = safeRecv(socketNumber, dataBuffer, pdu_length_read-2, MSG_WAITALL);
    
    // parse out the flag
    if (received_bytes == 0) {
        printf("There is nothing to read! Connection closed by client.\n");
        return 0;
    }
    if (received_bytes < 0) {
        perror("recv failed");
        return -1;
    }

    return received_bytes;
}

