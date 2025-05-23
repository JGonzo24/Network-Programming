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
#include "windowing.h"

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
void processServer(int socketNum, double err_rate);
void processClient(int socketNum, uint8_t *buffer, int dataLen, struct sockaddr_in6 client, int16_t buffSize, int8_t windowSize);
STATE fileOpen(int socketNum, struct sockaddr_in6 client, uint8_t *buffer, int32_t dataLen, char **data_file);
STATE waitOnAck(int socketNum, struct sockaddr_in6 client);
STATE sendData(int socketNum, struct sockaddr_in6 client, uint8_t *buffer, int dataLen, char *data_file, int16_t buffSize, int8_t windowSize);

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

    processServer(socketNum, err_rate);

    close(socketNum);

    return 0;
}

void processServer(int socketNum, double err_rate)
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
        printPDU(buffer, dataLen);
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
                // Get the window and buffer size
                int16_t buffSize = 0;
                int8_t winSize = 0;

                // Extract the buffer size and window size from the buffer
                memcpy(&winSize, buffer + 7, sizeof(winSize));
                memcpy(&buffSize, buffer + 8, sizeof(buffSize));

                buffSize = ntohs(buffSize);
                // Print the buffer size and window size

                printf("Buffer size: %d\n", buffSize);
                printf("Window size: %d\n", winSize);

                sendErr_init(err_rate, DROP_ON, FLIP_ON, DEBUG_ON, RSEED_ON);

                printf("Child fork() - child pid: %d\n", getpid());

                // Here we are going to close the main server socket, then open a new socket to process
                close(socketNum);
                removeFromPollSet(socketNum);

                // open a new socket:
                int childSocket = udpServerSetup(0);
                addToPollSet(childSocket);
                processClient(childSocket, buffer, dataLen, client, buffSize, winSize);

                close(childSocket);
                exit(0);
            }
        }

        printf("Received message from client with ");
        printIPInfo(&client);
        printf(" Len: %d '%s'\n", dataLen, buffer);
    }
}

void processClient(int socketNum, uint8_t *buffer, int dataLen, struct sockaddr_in6 client, int16_t buffSize, int8_t windowSize)
{
    char *data_file = NULL;

    STATE state = START;
    while (state != DONE)
    {
        switch (state)
        {
        case START:
            state = FILE_OPEN;
            break;

        case FILE_OPEN:
            state = fileOpen(socketNum, client, buffer, dataLen, &data_file);
            break;

        case WAIT_ON_ACK:
            state = waitOnAck(socketNum, client);
            break;

        case SEND_DATA:
            printf("GOT TO SEND DATA!");
            state = sendData(socketNum, client, buffer, dataLen, data_file, buffSize, windowSize);
            break;

        default:
            state = DONE;
            break;
        }
    }
}

STATE sendData(int socketNum, struct sockaddr_in6 client, uint8_t *buffer, int dataLen,
               char *data_file, int16_t buffSize, int8_t windowSize)
{
    int seqNum = 0;
    // just double check packet to be sure
    printf("\nCheck the buffer just to be sure\n");
    printPDU(buffer, dataLen);

    SenderWindow window;
    initSenderWindow(&window, windowSize);

    printf("In sendData state in server. Window size: %d\n. ", windowSize);
    printf("Buffer size from cmd line: %d\n", buffSize);

    printf("WE ARE NOW GOING TO TRY TO SEND DATA FROM THE SERVER TO CLIENT\n");

    // Now to send data...........
    printf("Opening File: %s\n", data_file);

    FILE *file = fopen(data_file, "rb");
    if (!file)
    {
        perror("Failed to open file in sendData state in server");
        return DONE;
    }
    else
    {
        printf("Was able to open the file to try and read from the server now!\n");
        bool eofSeen = false;

        while (!windowIsFull(&window) && !eofSeen)
        {
            // Read a chunk of data from the file
            uint8_t dataBuffer[buffSize];
            size_t bytesRead = fread(dataBuffer, 1, buffSize, file);

            if (bytesRead < 0)
            {
                perror("Error reading from file");
                fclose(file);
                return DONE;
            }
            else if (bytesRead == 0)
            {
                eofSeen = true;
                printf("End of file reached.\n");

                // send EOF flag
                uint8_t eofBuffer[ACKBUF];
                uint32_t seqNum = 0; // Replace with your protocol's sequence number if needed
                int eofLen = createPDU(eofBuffer, seqNum, EOF_FLAG, NULL, 0, windowSize, buffSize);
                safeSendto(socketNum, eofBuffer, eofLen, 0, (struct sockaddr *)&client, sizeof(client));

                printf("Sent EOF flag to client.\n");
                fclose(file);
                break;
            }

            // Create a PDU with the data
            uint8_t pduBuffer[MAXBUF];
            int pduLen = createPDU(pduBuffer, seqNum, DATA, dataBuffer, bytesRead, windowSize, buffSize);

            Packet packet;
            packet.seqNum = seqNum;
            packet.flag = DATA;
            memcpy(packet.data, dataBuffer, bytesRead);

            // Add the packet to the window
            addPacketToWindow(&window, &packet);

            // Send the PDU to the client
            printf("Sending PDU with sequence number %d and flag %d\n", seqNum, DATA);
            printPDU(pduBuffer, pduLen);

            int bytesSent = safeSendto(socketNum, pduBuffer, pduLen, 0, (struct sockaddr *)&client, sizeof(client));
            seqNum++;

            if (bytesSent < 0)
            {
                perror("Error sending data");
                fclose(file);
                return DONE;
            }

            printf("Sent %d bytes to client.\n", bytesSent);

            // Poll, wait for RR or SREJ
            // If there is something there to read immediately, then we can read it
            while (pollCall(0) > 0)
            {
                // Proccess RR or SREJ
                uint8_t ackBuffer[ACKBUF];
                socklen_t addrLen = sizeof(client);
                int received = safeRecvfrom(socketNum, ackBuffer, ACKBUF, 0, (struct sockaddr *)&client, (int *)&addrLen);
                if (received < 0)
                {
                    perror("Error receiving ack");
                    fclose(file);
                    return DONE;
                }

                // Extract the flag from the ack
                uint8_t flag;
                memcpy(&flag, ackBuffer + 6, sizeof(flag));
                printf("Received ack with flag %d\n", flag);

                // Check if the flag is RR or SREJ
                if (flag == RR)
                {
                    printf("Received RR, moving window forward.\n");
                    // Move the window forward
                    window.lower = (window.lower + 1) % window.windowSize;
                    window.current = (window.current + 1) % window.windowSize;
                }
                else if (flag == SREJ)
                {
                    printf("Received SREJ, resending packet.\n");
                    // Resend the packet
                    int resendIndex = (window.lower + 1) % window.windowSize;
                    Packet *resendPacket = &window.buffer[resendIndex];
                    int resendLen = createPDU(pduBuffer, resendPacket->seqNum, DATA, resendPacket->data, bytesRead, windowSize, buffSize);
                    safeSendto(socketNum, pduBuffer, resendLen, 0, (struct sockaddr *)&client, sizeof(client));
                }
                else
                {
                    printf("Received unexpected flag %d, ignoring.\n", flag);
                }
            }

            // while the window is closed, if poll doesn't time out, then process the rr/srej
            // else, send lowest packet in the window
            while (windowIsFull(&window))
            {
                int timeout = 1000;
                int ready;

                ready = pollCall(timeout);

                if (ready == -1)
                {
                    printf("Timed out waiting for RR/SREJ, resending lowest packet in window.\n");
                    // Resend the lowest packet in the window
                    int resendIndex = window.lower;
                    Packet *resendPacket = &window.buffer[resendIndex];
                    int resendLen = createPDU(pduBuffer, resendPacket->seqNum, DATA, resendPacket->data, bytesRead, windowSize, buffSize);
                    safeSendto(socketNum, pduBuffer, resendLen, 0, (struct sockaddr *)&client, sizeof(client));
                }
                else
                {
                    // We have a socket that is ready to read now, process the rr/srej
                    uint8_t ackBuffer[ACKBUF];
                    socklen_t addrLen = sizeof(client);
                    int received = safeRecvfrom(socketNum, ackBuffer, ACKBUF, 0, (struct sockaddr *)&client, (int *)&addrLen);
                    if (received < 0)
                    {
                        perror("Error receiving ack");
                        fclose(file);
                        return DONE;
                    }
                    // Extract the flag from the ack
                    uint8_t flag;
                    memcpy(&flag, ackBuffer + 6, sizeof(flag));
                    printf("Received ack with flag %d\n", flag);
                    // Check if the flag is RR or SREJ
                    if (flag == RR)
                    {
                        printf("Received RR, moving window forward.\n");
                        // Move the window forward
                        window.lower = (window.lower + 1) % window.windowSize;
                        window.current = (window.current + 1) % window.windowSize;
                    }
                    else if (flag == SREJ)
                    {
                        printf("Received SREJ, resending packet.\n");
                        // Resend the packet
                        int resendIndex = (window.lower + 1) % window.windowSize;
                        Packet *resendPacket = &window.buffer[resendIndex];
                        int resendLen = createPDU(pduBuffer, resendPacket->seqNum, DATA, resendPacket->data, bytesRead, windowSize, buffSize);
                        safeSendto(socketNum, pduBuffer, resendLen, 0, (struct sockaddr *)&client, sizeof(client));
                    }
                    else
                    {
                        printf("Received unexpected flag %d, ignoring.\n", flag);
                    }
                }
            }
        }
    }
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

/**
 * @brief
 * @details This opens up the file on the server side to ensure that the file that is requested is
 * in the server's directory
 *
 * @param socketNum
 * @param client
 * @param buffer
 * @param dataLen
 * @param data_file
 * @param buffSize
 * @return STATE
 */
STATE fileOpen(int socketNum, struct sockaddr_in6 client, uint8_t *buffer, int32_t dataLen, char **data_file)
{
    // Check if the buffer is long enough to contain the filename
    if (dataLen < 13)
    {
        printf("Received buffer too short for filename, transitioning to DONE.\n");
        return DONE;
    }

    // Extract the filename from the buffer
    // Assuming the filename starts at index 7 and is null-terminated
    // The filename length is given by 7 for header 2 for )
    {
        char fname[MAX_FILE_LEN];
        int filenameLen = dataLen - 10;

        memcpy(fname, buffer + 10, filenameLen);
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
            *data_file = malloc(strlen(fname) + 1);
            if (*data_file == NULL)
            {
                perror("MALLOC FAILED IN fileOPEN state");
                return DONE;
            }
            strcpy(*data_file, fname);

            return WAIT_ON_ACK;
        }
        return DONE;
    }
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
