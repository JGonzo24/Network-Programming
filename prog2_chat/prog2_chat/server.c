/******************************************************************************
 * myServer.c
 *
 * Written by Prof. Smith, updated Jan 2023
 * Use at your own risk.
 *
 *****************************************************************************/

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
#include <poll.h> // Include poll.h for pollfd structure

#include "networks.h"
#include "safeUtil.h"
#include "sendreceive.h"
#include "pollLib.h"
#include "handle_table.h"
#include "shared.h"
#include "makePDU.h"

#define DEBUG_FLAG 1
#define POLL_SET_SIZE 10 // Define the size of the poll set

void printPacket(const uint8_t *packet, size_t length);
void recvFromClient(int clientSocket);
int checkArgs_s(int argc, char *argv[]);
void serverControl(int serverSocket);
int processClient(int socketNum);
int addNewSocket(int socketNum);
void forwardMessage(int socketNum, uint8_t *buffer, int messageLen);
void handleFlags(int socketNum, uint8_t flag, uint8_t *buffer, int messageLen);
int handleBroadcastMessage_s(int socketNum, const char *buffer);
int validateMessage(uint8_t *buffer, int messageLen, int sender_socketNum);
int validateMulticastMessage(uint8_t *buffer , int socketNum, int messageLen);
int sendClientResponse(int socketNum, uint8_t flag, uint8_t handle_len, char *handle);
int handleListHandles_s(int socketNum, char *buffer);



#define MAX_HANDLE_LEN 100
// Define MAX_HANDLE_LEN with an appropriate value

void printPacket(const uint8_t *packet, size_t length)
{
    printf("Packet (%zu bytes):\n", length);
    for (size_t i = 0; i < length; i++)
    {
        printf("%02x ", packet[i]); // Print each byte in hexadecimal format
        if ((i + 1) % 16 == 0)
        {
            printf("\n"); // Add a newline every 16 bytes for readability
        }
    }
    printf("\n");
}

int main(int argc, char *argv[])
{
    int mainServerSocket = 0; // Socket descriptor for the server socket
    int portNumber = 0;

    // Parse command line arguments
    portNumber = checkArgs_s(argc, argv);

    // Create the server socket
    mainServerSocket = tcpServerSetup(portNumber);
    initHandleTable(); // Initialize the handle table

    // Set up the poll set
    setupPollSet();

    // Start the server control loop to handle client connections
    serverControl(mainServerSocket);

    // Close the server socket when done
    close(mainServerSocket);

    return 0;
}

// Function to receive data from a client and echo it back
void recvFromClient(int clientSocket)
{
    uint8_t dataBuffer[MAXBUF];
    int messageLen = 0;

    // Receive data from the client_socket
    if ((messageLen = recvPDU(clientSocket, dataBuffer, MAXBUF)) < 0)
    {
        perror("recv call failed");
        exit(-1);
    }
    if (messageLen > 0)
    {
        // Send the message back to the client (echoing)
        messageLen = safeSend(clientSocket, dataBuffer, messageLen, 0);
    }
    else
    {
        printf("Socket %d: Connection closed by other side\n", clientSocket);
    }
}


int handleBroadcastMessage_s(int socketNum, const char *buffer)
{
    // Handle the broadcast message command
    printf("Handle broadcast message command.\n");
    printf("------------------- broadcast message -------------------\n");
    // Construct broadcast packet
    printPacket((uint8_t *)buffer, strlen(buffer));
    Handle_t *handle = getHandleTable();
    int handleCount = getHandleCount();
    for (int i = 0; i < handleCount; i++)
    {
        if (handle[i].socketNum != socketNum)
        {
            // Send the broadcast message to all clients except the sender
            int sent = sendPDU(handle[i].socketNum, (uint8_t *)buffer, strlen(buffer));
            if (sent < 0)
            {
                printf("Error sending broadcast message to socket %d\n", handle[i].socketNum);
            }
        }
    }
    printf("--------------------------------------------------------\n");
    
    return 0;
}

// Function to check command-line arguments and return the port number
int checkArgs_s(int argc, char *argv[])
{
    int portNumber = 0;

    if (argc > 2)
    {
        fprintf(stderr, "Usage: %s [optional port number]\n", argv[0]);
        exit(-1);
    }

    if (argc == 2)
    {
        portNumber = atoi(argv[1]);
    }

    return portNumber;
}

// Main server control function to handle new connections and client data
void serverControl(int serverSocket)
{

    addToPollSet(serverSocket);
    int returned_socket;

    while (1)
    {
        // Call poll to monitor the file descriptors in 'fds'
        returned_socket = pollCall(-1); // Block indefinitely until an event occurs

        printf("poll() returned socket: %d\n", returned_socket);

        if (returned_socket == serverSocket)
        {
            // If the returned socket is the server socket, a new client is connecting
            addNewSocket(serverSocket); // Accept new client and add to poll set
        }
        else if (returned_socket > 0)
        {
            // If the returned socket is a client socket, process its data
            printf("Processing client socket: %d\n", returned_socket);
            processClient(returned_socket);
        }
        else
        {
            printf("The returned socket was not a server or client socket!\n");
            return;
        }
    }
}

int addNewSocket(int socketNum)
{
    // Accept the new client connection
    int newSocket = accept(socketNum, NULL, NULL);

    if (newSocket <= 0)
    {
        perror("Failed to accept client");
        return -1;
    }

    // Add the new client socket to the poll set
    addToPollSet(newSocket); // This adds the socket to the global poll set

    // Get the handle len from the initial packet
    uint8_t buffer[MAXBUF];
    int received_bytes = recvPDU(newSocket, buffer, MAXBUF);

    if (received_bytes <= 0)
    {
        perror("Failed to receive handle length");
        close(newSocket);
        return -1;
    }

    uint8_t flag = buffer[0];
    uint8_t handle_len = buffer[1];

    if (handle_len + 2 > MAXBUF)
    {
        printf("Error: handle_len is too large to fit in buffer\n");
        close(newSocket);
        return -1;
    }

    // Save the handle from the buffer
    char handle[handle_len + 1]; // +1 for null terminator
    memcpy(handle, buffer + 2, handle_len);
    handle[handle_len] = '\0'; // Null terminate the string

    // Print flag, handle length, and handle
    printf("Flag: %d, Handle Length: %d, Handle: %s\n", flag, handle_len, handle);

    uint8_t response[3]; // 3 bytes: 2 for length, 1 for flag

    uint16_t length_bytes = 3;                              // 2 bytes for the PDU length and 1 byte for the flag
    uint16_t length_in_network_order = htons(length_bytes); // Convert length to network order

    // check for segfault
    if (length_in_network_order == 0)
    {
        printf("Error: length_in_network_order is zero\n");
        close(newSocket);
        return -1;
    }

    memcpy(response, &length_in_network_order, 2); // Copy PDU length (2 bytes)
    response[2] = flag; // Set the flag in the response

    if (addHandle(newSocket, handle, handle_len) < 0)
    {
        printf("Error adding handle to table\n");
        printf("Sending error response to client\n");
        response[2] = 3; // Error flag (set the flag to indicate an error)
        sendPDU(newSocket, response, sizeof(response));
    }
    else
    {
        response[2] = 2; // Success flag (set the flag to indicate success)
        printf("Handle added successfully\n");
        printf("Sending success response to client\n");
        sendPDU(newSocket, response, sizeof(response));
    }

    printf("New client connected at socket num %d\n", newSocket);
    return newSocket;
}

// Function to process client data
int processClient(int socketNum)
{
    uint8_t buffer[MAXBUF];
    int messageLen = 0;
    uint8_t flag;

    // save the pdu into the buffer
    messageLen = recvPDU(socketNum, buffer, MAXBUF);
    if (messageLen < 0)
    {
        perror("recvPDU failed");
        return -1;
    }

    // Extract the flag from the buffer
    flag = buffer[0]; // First byte is the flag


    if (messageLen == 0)
    {
        printf("Socket %d: Connection closed by client\n", socketNum);

        // Remove the client from the poll set and close the socket
        removeFromPollSet(socketNum);
        close(socketNum);
        removeHandle(socketNum); // Remove the handle from the table
        return 0;
    }
    else if (messageLen < 0)
    {
        perror("Receiving the PDU() failed\n");

        // Remove the client from the poll set and close the socket
        removeFromPollSet(socketNum);
        close(socketNum);
        return -1;
    }
    else
    {
        printf("PDU Received: %d bytes\n", messageLen);
        handleFlags(socketNum, flag, buffer, messageLen);
    }
    return 0;
}

void multicastMessage(int socketNum, uint8_t *buffer, int messageLen)
{
    // Check if the message is valid
    if (messageLen < 3)
    {
        printf("Invalid multicast message: too short\n");
        return;
    }

    // Print the multicast message
    printf("Multicast message received on socket %d\n", socketNum);

    // Call the function to handle the multicast message
    int valid = validateMulticastMessage(buffer, socketNum, messageLen);
    if (valid < 0)
    {
        printf("Invalid multicast message format\n");
        return;
    }
    printf("Multicast message received on socket %d\n", socketNum);
}

void handleFlags(int socketNum, uint8_t flag, uint8_t *buffer, int messageLen)
{
    switch (flag)
    {
    case 0x04:
        // Handle the command type 0x04
        printf("Command type 0x04 received\n");
        fflush(stdout);
        handleBroadcastMessage_s(socketNum, (char*)buffer);
        break;
    case 0x05:
        // Handle the command type 0x05
        printf("Command type 0x05 received\n");
        fflush(stdout);
        forwardMessage(socketNum, buffer, messageLen);
        break;
    case 0x06:
        // Handle the command type 0x06
        printf("Command type 0x06 received\n");
        fflush(stdout);
        multicastMessage(socketNum, buffer, messageLen);
        break;
    case 0xA:
        // Handle the command type 0xA
        printf("Command type 0xA received, list handles!\n");
        fflush(stdout);
        handleListHandles_s(socketNum, (char*)buffer);
        break;

    default:
        // Handle unknown command
        printf("Unknown command detected.\n");
        break;
    }
}

int handleListHandles_s(int socketNum, char *buffer)
{
    // Handle the list handles command
    uint8_t flag = buffer[0];
    if (flag != 0xA)
    {
        printf("Invalid flag for list handles: %d\n", flag);
        return -1;
    }

    sendListPDU(socketNum); // Send the list of handles to the client
    return 0;
}

void forwardMessage(int socketNum, uint8_t *buffer, int messageLen)
{
    int valid = validateMessage(buffer, messageLen, socketNum);

    if (valid < 0)
    {
        printf("Invalid message format\n");
        return;
    }
}

int validateMulticastMessage(uint8_t *buffer, int socketNum, int messageLen)
{
    printf("Validating multicast message\n");
    int offset = 0;
    // First read the buffer to get the sender handle length
    uint8_t flag = buffer[0];

    if (flag != 0x06)
    {
        printf("Invalid flag for multicast message: %d\n", flag);
        return -1;
    }
    offset++;

    uint8_t senderHandleLen = buffer[1];
    offset++;

    char senderHandle[senderHandleLen + 1];
    memcpy(senderHandle, buffer + 2, senderHandleLen);
    offset += senderHandleLen;
    // read the byte for num of destination handles
    uint8_t numDestHandles = buffer[2 + senderHandleLen];
    offset++;
    DestHandle_t destHandles[MAX_DEST_HANDLES];

    for (int i = 0; i < numDestHandles; i++)
    {
        destHandles[i].dest_handle_len = buffer[offset++];
        memcpy(destHandles[i].handle_name, buffer + offset, destHandles[i].dest_handle_len);
        destHandles[i].handle_name[destHandles[i].dest_handle_len] = '\0'; // Null-terminate the handle
        offset += destHandles[i].dest_handle_len;
    }

    printf("Destination handles:\n");
    for (int i = 0; i < numDestHandles; i++)
    {
        printf("Handle %d: %s\n", i + 1, destHandles[i].handle_name);
    }
    int sent_bytes = 0;
    // Check if the read handles are valid through the handle table
    for (int i = 0; i < numDestHandles; i++)
    {
        int dest_socketNum = 0;
        if (getSocket(destHandles[i].handle_name, &dest_socketNum) < 0)
        {
            printf("Error: destination handle %s not found in the table.\n", destHandles[i].handle_name);
            sent_bytes = sendClientResponse(socketNum, 0x07, destHandles[i].dest_handle_len, destHandles[i].handle_name); // Send error response to client
        }
        else
        {
            printf("Destination handle %s found in the table with socket number %d\n", destHandles[i].handle_name, dest_socketNum);
            sent_bytes = sendPDU(dest_socketNum, buffer, messageLen); // Send the message to the destination handle

            if (sent_bytes < 0)
            {
                printf("Error sending message to socket %d\n", socketNum);
                return -1;
            }
            else
            {
                printf("Message sent to socket %d\n", socketNum);
            }
        }
    }
    // Send success response to the client
    return 0;
}

int sendClientResponse(int socketNum, uint8_t flag, uint8_t handle_len, char *handle)
{
    // create a 1 byte buffer with the flag, sendPDU() will send the length
    uint8_t response[MAXBUF];
    response[0] = flag;

    // Set the PDU length (2 bytes for length + 1 byte for flag)
    response[1] = handle_len; // Set the handle length

    // copy in the handle
    memcpy(response + 2, handle, handle_len);

    int sent = sendPDU(socketNum, response, 1);

    if (sent < 0)
    {
        printf("Error sending response to socket %d\n", socketNum);
        return -1;
    }
    else
    {
        printf("Response sent to socket %d with flag %d\n", socketNum, flag);
    }
    return sent;
}

int validateMessage(uint8_t *buffer, int messageLen, int sender_socketNum)
{
    // Check if the message is valid
    if (messageLen < 3)
    {
        printf("Invalid message: too short\n");
        return -1;
    }

    // Extract the sender handle length and destination handle length
    uint8_t senderHandleLen = buffer[1];
    uint8_t destinationHandleLen = buffer[3 + senderHandleLen];

    // Extract the sender handle and destination handle
    char senderHandle[senderHandleLen + 1];
    char destinationHandle[destinationHandleLen + 1];

    memcpy(senderHandle, buffer + 2, senderHandleLen);
    memcpy(destinationHandle, buffer + 4 + senderHandleLen, destinationHandleLen);

    // Null-terminate the handles
    senderHandle[senderHandleLen] = '\0';
    destinationHandle[destinationHandleLen] = '\0';

    printf("Sender Handle: %s, Destination Handle: %s\n", senderHandle, destinationHandle);

    int socketNum = 0;
    // Check if the destination handle exists in the handle table
    if (getSocket(destinationHandle, &socketNum) < 0)
    {
        printf("Error: destination handle %s not found in the table.\n", destinationHandle);
        sendClientResponse(sender_socketNum, 0x07, destinationHandleLen, destinationHandle); // Send error response to client
        return -1;
    }
    printf("Destination handle %s found in the table with socket number %d\n", destinationHandle, socketNum);

    sendPDU(socketNum, buffer, messageLen); // Send the message to the destination handle
    printf("Message sent to socket %d\n", socketNum);

    // Check if the lengths are within bounds
    if (senderHandleLen + destinationHandleLen + 4 > messageLen)
    {
        printf("Invalid message: lengths exceed message length\n");
        return -1;
    }
    return 0; // Return 0 for valid message
}