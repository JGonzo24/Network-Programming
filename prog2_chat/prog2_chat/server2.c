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

#define MAXBUF 1024 // Define MAXBUF with an appropriate buffer size
#define DEBUG_FLAG 1
#define POLL_SET_SIZE 10 // Define the size of the poll set

void recvFromClient(int clientSocket);
int checkArgs(int argc, char *argv[]);
void serverControl(int serverSocket);
int processClient(int socketNum);
int addNewSocket(int socketNum);

int main(int argc, char *argv[]) {
    int mainServerSocket = 0;   // Socket descriptor for the server socket
    int portNumber = 0;

    // Parse command line arguments
    portNumber = checkArgs(argc, argv);

    // Create the server socket
    mainServerSocket = tcpServerSetup(portNumber);

    // Set up the poll set
    setupPollSet();

    // Start the server control loop to handle client connections
    serverControl(mainServerSocket);

    // Close the server socket when done
    close(mainServerSocket);

    return 0;
}

// Function to receive data from a client and echo it back
void recvFromClient(int clientSocket) {
    uint8_t dataBuffer[MAXBUF];
    int messageLen = 0;

    // Receive data from the client_socket
    if ((messageLen = recvPDU(clientSocket, dataBuffer, MAXBUF)) < 0) {
        perror("recv call failed");
        exit(-1);
    }
    if (messageLen > 0) {
        printf("Socket %d: Message received, length: %d Data: %s\n", clientSocket, messageLen, dataBuffer);

        // Send the message back to the client (echoing)
        messageLen = safeSend(clientSocket, dataBuffer, messageLen, 0);
        printf("Socket %d: msg sent: %d bytes, text: %s\n", clientSocket, messageLen, dataBuffer);
    } else {
        printf("Socket %d: Connection closed by other side\n", clientSocket);
    }
}

// Function to check command-line arguments and return the port number
int checkArgs(int argc, char *argv[]) {
    int portNumber = 0;

    if (argc > 2) {
        fprintf(stderr, "Usage: %s [optional port number]\n", argv[0]);
        exit(-1);
    }

    if (argc == 2) {
        portNumber = atoi(argv[1]);
    }

    return portNumber;
}

// Main server control function to handle new connections and client data
void serverControl(int serverSocket) {
    addToPollSet(serverSocket);
    int returned_socket;


    while (1) {
        // Call poll to monitor the file descriptors in 'fds'
        returned_socket = pollCall(-1);  // Block indefinitely until an event occurs

        printf("poll() returned socket: %d\n", returned_socket);

        if (returned_socket == serverSocket) {
            // If the returned socket is the server socket, a new client is connecting
            addNewSocket(serverSocket);  // Accept new client and add to poll set
        } else if (returned_socket > 0) {
            // If the returned socket is a client socket, process its data
            processClient(returned_socket);
        } else {
            printf("The returned socket was not a server or client socket!\n");
            return;
        }
    }
}

// Function to accept a new client connection and add it to the poll set
int addNewSocket(int socketNum) {
    // Accept the new client connection
    int newSocket = accept(socketNum, NULL, NULL);

    if (newSocket < 0) {
        perror("Failed to accept client");
        return -1;
    }

    // Add the new client socket to the poll set
    addToPollSet(newSocket);  // This adds the socket to the global poll set
    printf("New client connected at socket num %d\n", newSocket);
    return newSocket;
}

// Function to process client data
int processClient(int socketNum) {
    uint8_t buffer[MAXBUF];
    int messageLen = 0;

    messageLen = recvPDU(socketNum, buffer, MAXBUF);
    if (messageLen == 0) {
        printf("Socket %d: Connection closed by client\n", socketNum);

        // Remove the client from the poll set and close the socket
        removeFromPollSet(socketNum);
        close(socketNum);
        return 0;
    } else if (messageLen < 0) {
        perror("Receiving the PDU() failed\n");

        // Remove the client from the poll set and close the socket
        removeFromPollSet(socketNum);
        close(socketNum);
        return -1;
    } else {
        // Print the received data
        printf("Message received on socket %d, length: %d, Data: %s\n", socketNum, messageLen, buffer);
    }
    // Echo the message back to the client
    messageLen = sendPDU(socketNum, buffer, messageLen);
    printf("Socket %d: Number of bytes sent: %d bytes, message: %s\n", socketNum, messageLen, buffer);
    // Continue processing the client
    return 0;
}
