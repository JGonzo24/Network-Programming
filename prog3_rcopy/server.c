/**
 * @defgroup udp_server UDP Server Module
 * @brief A simple IPv6/IPv4 UDP server example.
 * @{
 */

/**
 * @file    server.c
 * @author  Joshua Gonzalez
 * @brief   Simple UDP server that listens for messages and echoes back byte counts.
 * @version 0.1
 * @date    2025-05-10
 *
 * @details
 * This server sets up a UDP socket (IPv6-capable), receives messages until
 * it sees a single dot ('.') as the first character, and for each message
 * prints the client address + payload, then echoes back the number of bytes received.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <stdbool.h>

#include "gethostbyname.h"
#include "networks.h"
#include "safeUtil.h"
#include "helperFunctions.h"
#include "cpe464.h"
#include "pollLib.h"
#include <signal.h>
#include "srej.h"

typedef enum State STATE;
enum State
{
    START,
    FILE_OPEN,
    DONE,
    WAIT_ON_ACK,
    SEND_DATA,
};

/**
 * @brief   Validates command-line arguments for port number.
 * @param   argc  Argument count from main().
 * @param   argv  Argument vector from main().
 * @return  The port number to use (0 if none provided).
 *
 * @note    On error (too many args), prints usage to stderr and exits.
 */
int checkArgs(int argc, char *argv[]);

/**
 * @brief   Entry point for the UDP server application.
 * @param   argc  Number of command-line arguments.
 * @param   argv  Array of command-line argument strings.
 * @return  0 on clean exit.
 *
 * @details
 * - Parses optional port via `checkArgs()`.
 * - Calls `udpServerSetup()` to bind a socket.
 * - Hands off to `processClient()` for recv/send loop.
 * - Closes the socket and returns.
 */

static void handleZombies(int signo);
void processServer(int socketNum);
void processClient(int socketNum, uint8_t *buffer, int dataLen, struct sockaddr_in6 client);
STATE fileOpen(int socketNum, struct sockaddr_in6 client, uint8_t *buffer, int32_t dataLen, int32_t *data_file, int32_t *buffSize);
STATE waitOnAck(int socketNum, struct sockaddr_in6 client);

int main(int argc, char *argv[])
{
    int socketNum = 0;
    int portNumber = 0;

    portNumber = checkArgs(argc, argv);

    double err_rate = atof(argv[1]);
    sendErr_init(err_rate, DROP_ON, FLIP_ON, DEBUG_ON, RSEED_OFF);

    socketNum = udpServerSetup(portNumber);

    setupPollSet();
    addToPollSet(socketNum);

    processServer(socketNum);

    close(socketNum);

    return 0;
}

void processServer(int socketNum)
{
    pid_t pid = 0;
    uint8_t buffer[MAXBUF];
    struct sockaddr_in6 client;
    int clientAddrLen = sizeof(client);
    int dataLen;

    signal(SIGCHLD, handleZombies);

    while (1)
    {
        dataLen = safeRecvfrom(socketNum, buffer, MAXBUF, 0, (struct sockaddr *)&client, (int *)&clientAddrLen);

        printf("Received bytes in the main server socket:\n");
        printBytes(buffer, dataLen);

        // check the checksum
        bool checkSum;
        checkSum = verifyChecksum(buffer, dataLen);

        if (checkSum == false)
        {
            continue;
        }
        if (checkSum == true)
        {
            if ((pid = fork()) < 0)
            {
                perror("Forking in server gone wrong");
                exit(-1);
            }
            else if (pid == 0)
            {
                // we are now in a child process, so start the processing of a client
                printf("Child fork() - child pid: %d\n", getpid());

                // Here we are going to close the main server socket, then open a new socket to process 
                close(socketNum);
                removeFromPollSet(socketNum);

                // open a new socket:
                int childSocket = udpServerSetup(0);
                addToPollSet(childSocket);
                processClient(childSocket, buffer, dataLen, client);

                close(childSocket);
                exit(0);
            }
        }

        printf("Received message from client with ");
        printIPInfo(&client);
        printf(" Len: %d '%s'\n", dataLen, buffer);
    }
}

void processClient(int socketNum, uint8_t *buffer, int dataLen, struct sockaddr_in6 client)
{
    int32_t data_file = 0;
    int32_t buff_size = 0;
    // Here we can have our state machine for the server
    STATE state = START;
    while (state != DONE)
    {
        switch (state)
        {
        case START:
            state = FILE_OPEN;
            break;

        case FILE_OPEN:
            state = fileOpen(socketNum, client, buffer, dataLen, &data_file, &buff_size);
            break;

        case WAIT_ON_ACK:
            state = waitOnAck(socketNum, client);
            break;

        case SEND_DATA:
            printf("GOT TO SEND DATA!");
            state = sendData(socketNum, client, buffer, dataLen, &data_file, &buff_size);
            break;

        default:
            state = DONE;
            break;
        }
    }
}


STATE sendData(int socketNum, struct sockaddr_in6 client, uint8_t* buffer, int dataLen, int32_t* data_file, int32_t* buff_size)
{
    printf("WE ARE NOW GOING TO TRY TO SEND DATA FROM THE SERVER TO CLIENT");
    return DONE;



}











STATE waitOnAck(int socketNum, struct sockaddr_in6 client)
{

    // Try to receive the ack (assume ACK PDU has length ACKBUF)
    uint8_t ackBuffer[ACKBUF];
    socklen_t addrLen = sizeof(client);
    int received = safeRecvfrom(socketNum, ackBuffer, ACKBUF, 0, (struct sockaddr *)&client, (int *)&addrLen);

    if (received < 0)
    {
        perror("Error receiving ack");
        return DONE;
    }

    // Extract the flag from the ack (assuming flag is the 7th byte in the ACK PDU)
    uint8_t flag;
    memcpy(&flag, ackBuffer + 6, 1);

    // Here FNAME_OK is your defined value indicating successful file open
    if (flag == FILE_OK_ACK)
    {
        printf("Received file ok ack from client, transitioning to SEND_DATA.\n");
        return SEND_DATA;
    }
    else
    {
        printf("Received ack with unexpected flag %d, transitioning to DONE.\n", flag);
        return DONE;
    }
}

STATE fileOpen(int socketNum, struct sockaddr_in6 client, uint8_t *buffer, int32_t dataLen, int32_t *data_file, int32_t *buffSize)
{
    char fname[MAX_FILE_LEN];
    int filenameLen = dataLen - 13;

    memcpy(fname, buffer + 7, filenameLen);
    fname[filenameLen] = '\0';

    printf("Extracted Filename: \"%s\"\n", fname);

    FILE *fp = fopen(fname, "rb");

    if (fp == NULL)
    {
        printf("Failed to open file %s, sending file NOT OK ack\n", fname);
        // Create a NACK PDU (using for example flag 37 for file error)
        uint8_t ackBuffer[ACKBUF];
        uint32_t seqNum = 0; // Replace with your protocol's sequence number if needed
        int ackLen = createAckPDU(ackBuffer, seqNum, FNAME_NOT_OK);
        safeSendto(socketNum, ackBuffer, ackLen, 0, (struct sockaddr *)&client, sizeof(client));
        // Remain in FILE_OPEN (or re-enter SEND_FILENAME to resend the filename)

        int timeout = 10000;
        int ready;

        ready = pollCall(timeout);

        if (ready == -1)
        {
            printf("Timed out waiting for file ok ack, transitioning to DONE.\n");
            return DONE;
        }
    }
    else
    {
        printf("File %s opened successfully.\n", fname);

        // Send a FILE OK ACK (using for example flag 36 for success)
        uint8_t ackBuffer[ACKBUF];
        uint32_t seqNum = 0; // Replace as necessary
        int ackLen = createAckPDU(ackBuffer, seqNum, FNAME_OK);
        safeSendto(socketNum, ackBuffer, ackLen, 0, (struct sockaddr *)&client, sizeof(client));
        fclose(fp);
        // Transition to the next state (for example, WAIT_ON_ACK to wait for further instructions)
        return WAIT_ON_ACK;
    }
    return DONE;
}

/**
 * @brief   Validates command-line arguments for port number.
 * @param   argc  Argument count from main().
 * @param   argv  Argument vector from main().
 * @return  The port number to use (0 if none provided).
 *
 * @note    On error (too many args), prints usage to stderr and exits.
 */

int checkArgs(int argc, char *argv[])
{
    int portNumber = 0;

    // First argument is the error rate
    double err_rate = 0.0;

    if (argc < 2 || argc > 3)
    {
        fprintf(stderr, "Usage: %s [error_rate] [optional port number]\n", argv[0]);
        exit(1);
    }

    err_rate = atof(argv[1]);
    if (err_rate < 0.0 || err_rate > 1.0)
    {
        fprintf(stderr, "Error rate must be between 0.0 and 1.0\n");
        exit(1);
    }

    if (argc == 3)
    {
        portNumber = atoi(argv[2]);
    }
    return portNumber;
}

static void handleZombies(int signo)
{
    int status;
    // Reap all available dead children without blocking
    while (waitpid(-1, &status, WNOHANG) > 0)
    {
    }
}

/** @} */ // end of udp_server group
