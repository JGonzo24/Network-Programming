

// --------------- MAKE PDU.c -----------------
/*
This code creates a PDU (Protocol Data Unit) for sending messages in a chat application.
Each PDU is different depending on the flags and the type of message being sent.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "makePDU.h"
#include "sendreceive.h"

/*
Format:
----------- 3 byte chat header ------------------
2 bytes: length of the packet
1 byte: command type (0x01 for the first PDU)
1 byte: sender handle length
x bytes: sender handle name
-------------------------------------------------
*/

// Global variable to store the sender handle

uint8_t *makeInitialPDU()
{
    int sender_handle_len = strlen(sender_handle);
    uint8_t static pdu[MAXBUF]; // Static array to hold the PDU
    uint8_t flag = 1;

    // memcpy the flag into the PDU
    memcpy(pdu, &flag, 1);

    // memcpy the sender handle length into the PDU
    memcpy(pdu + 1, &sender_handle_len, 1);

    // memcpy the sender handle into the PDU
    memcpy(pdu + 2, sender_handle, sender_handle_len);

    printf("\n");

    return pdu;
}

int constructMulticastPDU(uint8_t* multicastPDU, int socketNum, char* sender_handle,int numHandles, DestHandle_t* handles, char* message)
{
    int offset = 0;
    multicastPDU[0] = 0x06; // Command type for %c
    offset++;

    uint8_t sender_handle_len = strlen(sender_handle);
    multicastPDU[offset++] = sender_handle_len;
    memcpy(multicastPDU + offset, sender_handle, sender_handle_len);
    offset += sender_handle_len;

    multicastPDU[offset++] = numHandles; // Number of destination handles

    for (int i = 0; i < numHandles; i++)
    {
        uint8_t dest_handle_len = strlen(handles[i].handle_name);
        multicastPDU[offset++] = dest_handle_len;
        memcpy(multicastPDU + offset, handles[i].handle_name, dest_handle_len);
        offset += dest_handle_len;
    }

    // Copy the message into the PDU
    int message_len = strlen(message);
    memcpy(multicastPDU + offset, message, message_len);
    offset += message_len;

    // Now that the PDU is constructed, we can send it
    int pdu_length = offset; // Length of the PDU
    return pdu_length;
}

MessagePacket_t constructMessagePacket(char destinationHandle[100], int text_message_len, uint8_t text_message[199], int socketNum)
{

    // Create a packet info structure to hold the packet and its length
    MessagePacket_t packetInfo;
    static uint8_t message_packet[MAXBUF];
    int destinationHandleLen = strlen(destinationHandle);
    int senderHandleLen = strlen(sender_handle);

    int total_len = 1 + 1 + senderHandleLen + 1 + 1 + destinationHandleLen + text_message_len; // 1 byte for flag, 1 byte for sender handle length, 1 byte for destination handle flag, 1 byte for destination handle length, and the length of the text message

    if (total_len > MAXBUF)
    {
        printf("Error: Message length exceeds maximum buffer size.\n");
        packetInfo.packet = NULL;
        packetInfo.packet_len = -1;
        return packetInfo;
    }
    int offset = 0;
    // First packet is the flag
    uint8_t flag = 0x05; // Command type for %m
    message_packet[offset++] = flag;

    // 1 byte for the sender handle length
    message_packet[offset++] = senderHandleLen;
    // Copy the sender handle into the packet
    memcpy(message_packet + offset, sender_handle, senderHandleLen);
    offset += senderHandleLen;

    // 1 byte for the destination handle flag (always 1)
    message_packet[offset++] = 0x01;
    // 1 byte for the destination handle length
    message_packet[offset++] = destinationHandleLen;
    // Copy the destination handle into the packet
    memcpy(message_packet + offset, destinationHandle, destinationHandleLen);
    offset += destinationHandleLen;
    // Text message starts here
    memcpy(message_packet + offset, text_message, text_message_len);
    offset += text_message_len;

    // Fill the packet info structure
    packetInfo.packet = message_packet;
    packetInfo.packet_len = total_len;
    packetInfo.text_message_len = text_message_len;
    return packetInfo;
}
