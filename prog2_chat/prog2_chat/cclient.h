// File: cclient.h

#ifndef CCLIENT_H
#define CCLIENT_H

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <stdint.h> // For uint8_t and other fixed-width integer types


#define MAXBUF 1400
#define DEBUG_FLAG 1
#define MAX_MSG_SIZE 199 // Maximum message size (excluding the null terminator)
#define MAX_HANDLE_LEN 100
#define MAX_DEST_HANDLES 10


typedef struct {
	uint8_t CommandType;
    char senderHandle[MAX_HANDLE_LEN];
    uint8_t senderHandleLen;
    uint8_t destinationHandleFlag;
    char destinationHandle[MAX_HANDLE_LEN];
    uint8_t destinationHandleLen;
    uint8_t text_message[MAX_MSG_SIZE];
    uint8_t text_message_len;
    uint8_t *packet;
    int packet_len;
} MessagePacket_t;


typedef struct {
    uint8_t dest_handle_len;
    char handle_name[MAX_HANDLE_LEN];
} DestHandle_t;


typedef struct {
	uint8_t flag;
    uint8_t senderHandleLen;
    char senderHandle[MAX_HANDLE_LEN];
    uint8_t numDestHandles;
    DestHandle_t destHandles[MAX_DEST_HANDLES];
    char text_message[MAX_MSG_SIZE];
} MulticastPacket_t;



typedef enum {
	CMD_BROADCAST_MESSAGE,
	CMD_MULTICAST_MESSAGE,
	CMD_LIST_HANDLES,
	CMD_INVALID, 
    CMD_SEND_MESSAGE
} CommandType;


// Global 
extern char sender_handle[MAX_HANDLE_LEN];

// Functions
void readMulticastCommand(char *buffer, uint8_t *numHandles, DestHandle_t handles[], char *message);
void printPacket(const uint8_t *packet, size_t length);
void sendToServer(int socketNum);
int readFromStdin(uint8_t *buffer);
void checkArgs(int argc, char *argv[]);
void clientControl(int socketNum);
void processMsgFromServer(int socketNum);
void sendCommand(int socketNum, char *buffer);
CommandType parseCommand(char *buffer);
int handleSendMessage(int socketNum, const char *buffer);
void readMessageCommand(const char *buffer, char destinationHandle[100], uint8_t text_message[199]);
int  sendMessageInChunks(int socketNum, char *destinationHandle, uint8_t *fullMessage, int fullMessageLength);
void handleBroadcastMessage(int socketNum, const char *buffer);
void handleListHandles(int socketNum, const char *buffer);
void handleInvalidCommand(int socketNum, const char *buffer);
int initialConnection(int socketNum, uint8_t flag);
void receiveMessage(uint8_t *buffer, int totalBytes);
void waitForServerResponse(int socketNum);
void handleMulticastMessage(int socketNum, char *buffer);



#endif // CCLIENT_H
