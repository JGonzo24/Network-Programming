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

#include "gethostbyname.h"
#include "networks.h"
#include "safeUtil.h"
#include "helperFunctions.h"
#include "cpe464.h"

/**
 * @brief   Process incoming client messages on a bound UDP socket.
 * @param   socketNum  Descriptor of an already‐set‐up UDP socket.
 *
 * @details
 * Loops until the first character of `buffer` is '.', receiving into
 * an IPv6 sockaddr, printing the client’s IP and payload, then
 * sending back a text message containing the received byte count.
 */
void processClient(int socketNum);

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
int main(int argc, char *argv[])
{
    int socketNum = 0;
    int portNumber = 0;

    portNumber = checkArgs(argc, argv);

	double err_rate = atof(argv[1]);
	sendErr_init(err_rate, DROP_ON, FLIP_ON, DEBUG_ON, RSEED_OFF);

    socketNum = udpServerSetup(portNumber);

    processClient(socketNum);

    close(socketNum);

    return 0;
}



void processClient(int socketNum)
{
    int dataLen = 0;
    char buffer[MAXBUF + 1];
    struct sockaddr_in6 client;
    int clientAddrLen = sizeof(client);

    buffer[0] = '\0';
    while (buffer[0] != '.')
    {
        dataLen = safeRecvfrom(
            socketNum,
            buffer,
            MAXBUF,
            0,
            (struct sockaddr *)&client,
            &clientAddrLen
        );

        printf("Received message from client with ");
        printIPInfo(&client);
        printf(" Len: %d '%s'\n", dataLen, buffer);

        // Print the PDU details
        printPDU((uint8_t*)buffer, dataLen);

        // Echo back the received PDU to the client
        safeSendto(
            socketNum,
            buffer,
            dataLen,
            0,
            (struct sockaddr *)&client,
            clientAddrLen
        );
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

/** @} */  // end of udp_server group
