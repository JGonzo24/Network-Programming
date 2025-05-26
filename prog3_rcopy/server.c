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
STATE sendData(int socketNum, struct sockaddr_in6 client, char *data_file, int16_t buffSize, int8_t windowSize);
void done(int socketNum, struct sockaddr_in6 client, char *data_file, int16_t buffSize, int8_t windowSize);
void processRRSREJ(int socketNum, struct sockaddr_in6 *client, SenderWindow *W, int8_t windowSize, int16_t buffSize, uint8_t *pdu);
int main(int argc, char *argv[])
{
    int socketNum = 0;
    int portNumber = 0;

    portNumber = checkArgs(argc, argv);

    double err_rate = atof(argv[1]);
    sendErr_init(err_rate, DROP_ON, FLIP_ON, DEBUG_ON, RSEED_ON);

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
        dataLen = safeRecvfrom(socketNum, buffer, MAXBUF, 0, (struct sockaddr *)&client, &clientAddrLen);

        printf("Received bytes in the main server socket:\n");
        // printPDU(buffer, dataLen);
        //  check the checksum
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

void done(int socketNum, struct sockaddr_in6 client, char *data_file, int16_t buffSize, int8_t windowSize)
{

    if (data_file)
    {
        fclose(fopen(data_file, "rb"));
        printf("Closed data file: %s\n", data_file);
    }
    removeFromPollSet(socketNum);
    close(socketNum);

    printf("LETS LEAVE THE DONE STATE!\n");
    exit(-1);
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
            state = sendData(socketNum, client, data_file, buffSize, windowSize);
            break;

        default:
            state = DONE;
            printf("GOT TO DONE STATE from default\n\n");
            break;
        }
    }
    printf("Exiting processClient, cleaning up...\n");
    done(socketNum, client, data_file, buffSize, windowSize);
}

void processRRSREJ(int socketNum, struct sockaddr_in6 *client, SenderWindow *W, int8_t windowSize, int16_t buffSize, uint8_t *pdu)
{
    uint8_t rrSREJBuffer[11];
    int addrLen = sizeof(*client);
    int received = safeRecvfrom(socketNum, rrSREJBuffer, sizeof(rrSREJBuffer), 0, (struct sockaddr *)client, &addrLen);
    printf("Received RR/SREJ PDU of length %d\n", received);
    printBytes(rrSREJBuffer, received);
    if (received < 0)
    {
        perror("Error receiving RR/SREJ");
        return;
    }

    if (!verifyChecksum(rrSREJBuffer, received))
    {
        printf("Checksum verification failed for RR/SREJ PDU, ignoring.\n");
        return;
    }

    uint32_t seqNum;
    memcpy(&seqNum, rrSREJBuffer + 7, sizeof(seqNum));
    seqNum = ntohl(seqNum);

    uint8_t flag;
    memcpy(&flag, rrSREJBuffer + 6, sizeof(flag));
    if (flag == RR)
    {
        printf("Received RR for sequence number %u, sliding window.\n", seqNum);
        // Slide the window using the received RR sequence number
        W->lower = seqNum;
        W->upper = W->lower + windowSize;
    }
    else if (flag == SREJ)
    {
        int index = seqNum % windowSize;
        printf("Received SREJ for sequence number %u, resending packet.\n", seqNum);
        // Resend the packet from the sender window
        Packet *pckt = &W->buffer[index];
        int resendPduLen = createPDU(pdu, pckt->seqNum, pckt->flag, pckt->data, pckt->payloadLen, windowSize, buffSize);
        int sent = safeSendto(socketNum, pdu, resendPduLen, 0, (struct sockaddr *)client, sizeof(*client));
        if (sent < 0)
        {
            perror("Error resending data PDU");
            return;
        }
        printf("Resent data PDU with sequence number %u and length %d\n", pckt->seqNum, resendPduLen);
    }
}

STATE sendData(int socketNum,
               struct sockaddr_in6 client,
               char *data_file,
               int16_t buffSize,
               int8_t windowSize)
{
    FILE *fp = fopen(data_file, "rb");
    if (!fp)
    {
        perror("Error opening file");
        return DONE;
    }

    SenderWindow W;
    initSenderWindow(&W, windowSize);

    uint8_t dataBuf[MAXBUF], pdu[MAXBUF];
    uint32_t nextSeq = 0;
    bool eof = false;

    // ——— Phase 1: read & send until EOF ———
    while (!eof)
    {
        // fill up the window
        while (windowIsOpen(&W))
        {
            size_t bytesRead = fread(dataBuf, 1, buffSize, fp);
            if (bytesRead == 0)
            {
                if (feof(fp))
                {
                    eof = true;
                    printf("END OF FILE REACHED in sendData\n");
                }
                else
                {
                    perror("Error reading from file");
                    fclose(fp);
                    return DONE;
                }
                break;
            }

            // Debug:
            printf("Creating PDU with nextSeq=%u, bytesRead=%zu\n",
                   nextSeq, bytesRead);

            int pduLen = createPDU(pdu,
                                   nextSeq,
                                   DATA,
                                   dataBuf,
                                   bytesRead,
                                   windowSize,
                                   buffSize);

            int sent = safeSendto(socketNum, pdu, pduLen, 0,
                                  (struct sockaddr *)&client,
                                  sizeof(client));
            W.current++;

            if (sent < 0)
            {
                perror("Error sending data PDU");
                fclose(fp);
                return DONE;
            }
            printf("Sent data PDU seq=%u len=%d\n", nextSeq, pduLen);

            // stash in window
            Packet pkt = {
                .seqNum = nextSeq,
                .flag = DATA,
                .payloadLen = (int)bytesRead,
                .valid = true};
            memcpy(pkt.data, dataBuf, bytesRead);
            addPacketToWindow(&W, &pkt);

            printf("ARE WE AT EOF? %s\n", eof ? "YES" : "NO");
            printf("IS THE WINDOW STILL OPEN? %s\n",
                   windowIsOpen(&W) ? "YES" : "NO");
            printAllPacketsInWindow(&W);
            printWindow(&W);

            nextSeq++;

            // see if any RR/SREJs came in immediately
            while (pollCall(0) > 0)
            {
                printf("Processing RR/SREJ immediately after send…\n");
                processRRSREJ(socketNum,
                              &client,
                              &W,
                              windowSize,
                              buffSize,
                              pdu);
            }
        }

        // HERE THE WINDOW IS FULL
        // if we filled the window, wait here for at least one RR/SREJ
        if (!windowIsOpen(&W))
        {
            printf("Window full → waiting for RR/SREJ…\n");
            int r = pollCall(1000);
            if (r > 0)
            {
                processRRSREJ(socketNum,
                              &client,
                              &W,
                              windowSize,
                              buffSize,
                              pdu);
            }
            else
            {
                // timeout: resend the oldest un-ACKed
                Packet *old = &W.buffer[W.lower % windowSize];
                int len = createPDU(pdu,
                                    old->seqNum,
                                    old->flag,
                                    old->data,
                                    old->payloadLen,
                                    windowSize,
                                    buffSize);
                safeSendto(socketNum,
                           pdu, len, 0,
                           (struct sockaddr *)&client,
                           sizeof(client));
                printf("Timeout → resent seq=%u len=%d\n",
                       old->seqNum, len);
            }
        }
    }

    // ——— Phase 2: drain all outstanding packets before EOF ———
    {
        int retries = 0;
        while (W.lower < nextSeq)
        {
            printf("Draining window: lower=%u nextSeq=%u\n",
                   W.lower, nextSeq);
            int r = pollCall(1000);
            if (r > 0)
            {
                processRRSREJ(socketNum, &client, &W, windowSize, buffSize, pdu);
            }
            else
            {
                // resend oldest
                Packet *old = &W.buffer[W.lower % windowSize];
                int len = createPDU(pdu, old->seqNum, old->flag, old->data, old->payloadLen, windowSize, buffSize);
                safeSendto(socketNum, pdu, len, 0, (struct sockaddr *)&client, sizeof(client));
                printf("Drain timeout → resent lowest seq=%u len=%d\n",
                       old->seqNum, len);
                if (++retries >= 10)
                {
                    printf("Too many retries draining window → abort\n");
                    fclose(fp);
                    return DONE;
                }
            }
        }
    }

    // ——— Phase 3: send EOF and wait for its RR ———
    {
        uint8_t eofPdu[MAXBUF];
        int eofLen = createPDU(eofPdu, nextSeq, EOF_FLAG, NULL, 0, windowSize, buffSize);
        for (int t = 0; t < 10 && W.lower != nextSeq; t++)
        {
            safeSendto(socketNum, eofPdu, eofLen, 0, (struct sockaddr *)&client, sizeof(client));
            printf("Sent EOF seq=%u, attempt %d\n", nextSeq, t + 1);

            int r = pollCall(1000);
            if (r > 0)
            {
                processRRSREJ(socketNum, &client, &W, windowSize, buffSize, eofPdu);
            }
        }

        // cleanup
        printf("WE GOT TO THE END OF SEND DATA!\n");
        fclose(fp);
        free(data_file);
        removeFromPollSet(socketNum);
        close(socketNum);
        return DONE;
    }
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

    // check the checksum
    bool checkSum = verifyChecksum(ackBuffer, received);
    if (checkSum == false)
    {
        printf("Checksum Failed, try to resend the filename.\n");
        return FILE_OPEN;
    }

    // Extract the flag from the ack (assuming flag is the 7th byte in the ACK PDU)
    uint8_t flag;
    memcpy(&flag, ackBuffer + 6, 1);

    // Here FNAME_OK is your defined value indicating successful file open
    if (flag == FILE_OK_ACK)
    {
        printf("Received file ok ack from client, transitioning to SEND_DATA.\n");
        printf("Ready to send data to client.\n");
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
            uint32_t seqNum = 0;
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
