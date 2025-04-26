#ifndef SHARED_H
#define SHARED_H

#include <stdint.h>
#include "sendreceive.h"
#include "handle_table.h"

#define MAX_HANDLE_LEN 100
#define MAX_MSG_SIZE 199
#define MAX_DEST_HANDLES 10
#define MAXBUF 1400


int sendListPDU(int socketNum);
int makeListPDU(uint8_t* listPDU, int socketNum);



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


CommandType parseCommand(char *buffer);


#endif // COMMON_H