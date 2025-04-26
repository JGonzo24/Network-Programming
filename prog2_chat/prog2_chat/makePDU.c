

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


uint8_t* makeInitialPDU()
{
    int sender_handle_len = strlen(sender_handle);
    uint16_t total_len = 1 + 1 + sender_handle_len; // 1 byte for flag, 1 byte for handle length, and the length of the handle
    uint8_t static pdu[MAXBUF]; // Static array to hold the PDU
    uint8_t flag = 1;

    // memcpy the flag into the PDU
    memcpy(pdu, &flag, 1);

    // memcpy the sender handle length into the PDU
    memcpy(pdu + 1, &sender_handle_len, 1);

    // memcpy the sender handle into the PDU
    memcpy(pdu + 2, sender_handle, sender_handle_len);
    printf("Printed Initial PDU: ");
    for (int i = 0; i < total_len; i++)
    {
        printf("%02x ", pdu[i]);
    }
    printf("\n");

    return pdu;
}


uint8_t* constructMulticastPacket(char* buffer, int socketNum)
{
	static MulticastPacket_t packetInfo;
    static uint8_t multicastPDU[MAXBUF];
    memset(&packetInfo, 0, sizeof(packetInfo));

	printf("About to read the command!");

	readMulticastCommand(buffer, &packetInfo.numDestHandles, packetInfo.destHandles, packetInfo.text_message);
    // construct the packet to be sent
    int offset = 0;
    // First byte is the flag
    packetInfo.flag = 0x06;


    packetInfo.senderHandleLen = strlen(sender_handle);
    
    memcpy(packetInfo.senderHandle, sender_handle, packetInfo.senderHandleLen);

    // now copy them all into the PDU
    multicastPDU[offset++] = packetInfo.flag;
    multicastPDU[offset++] = packetInfo.senderHandleLen;
    memcpy(multicastPDU + offset, packetInfo.senderHandle, packetInfo.senderHandleLen);
    offset += packetInfo.senderHandleLen;
    multicastPDU[offset++] = packetInfo.numDestHandles;
    for (int i = 0; i < packetInfo.numDestHandles; i++)
    {
        multicastPDU[offset++] = packetInfo.destHandles[i].dest_handle_len;
        memcpy(multicastPDU + offset, packetInfo.destHandles[i].handle_name, packetInfo.destHandles[i].dest_handle_len);
        offset += packetInfo.destHandles[i].dest_handle_len;
    }
    // Copy the text message into the PDU
    int text_message_len = strlen(packetInfo.text_message); 
    memcpy(multicastPDU + offset, packetInfo.text_message, text_message_len);
    offset += text_message_len;
    // manually add the null terminator
    multicastPDU[offset] = '\0';
    offset += 1;
    
    printPacket(multicastPDU, offset);

    sendPDU(socketNum, multicastPDU, offset);
	return multicastPDU;
} 


MessagePacket_t constructMessagePacket(char destinationHandle[100], int text_message_len, uint8_t text_message[199], int socketNum)
{
    // // First check if we need to resize the message packet
    // if (text_message_len > MAXBUF)
    // {
    //     printf("Error: Message length exceeds maximum buffer size.\n");
        
    // }
    // printf("------------------------------------------\n");
    // printf("Text message length: %d\n", text_message_len);
    // printf("------------------------------------------\n");

    // printf("Constructing message packet...\n");
    // printf("Destination Handle: %s\n", destinationHandle);
	// int destinationHandleLen = strlen((char*)destinationHandle);
	// int senderHandleLen = strlen(sender_handle);

	// // First packet is the flag
	// uint8_t static message_packet[MAXBUF];
	// uint8_t flag = 0x05; // Command type for %m
	// memcpy(message_packet, &flag, 1);

	// // 1 byte for the sender handle length
    // memcpy(message_packet + 1, &senderHandleLen, 1);

	// // Copy the sender handle into the packet
	// memcpy(message_packet + 2, sender_handle, senderHandleLen);
	// // 1 byte for the destination handle flag (always 1
	// message_packet[2 + senderHandleLen] = 0x01;
	// // 1 byte for the destination handle length
	// message_packet[3 + senderHandleLen] = destinationHandleLen;
	// // Copy the destination handle into the packet
	// memcpy(message_packet + 4 + senderHandleLen, destinationHandle, destinationHandleLen);

    // printf("Sender Handle Length: %d\n", senderHandleLen);
    // printf("Destination Handle Length: %d\n", destinationHandleLen);

	// // Text message starts here
	// memcpy(message_packet + 4 + senderHandleLen + destinationHandleLen, text_message, text_message_len);

    // printPacket(message_packet, 4 + senderHandleLen + destinationHandleLen + text_message_len);

    // printf("----------------------------------------\n");
    // printPacket(message_packet, 4 + senderHandleLen + destinationHandleLen + text_message_len);
    // printf("Message packet constructed successfully.\n");
    // printf("-----------------------------------------\n");
    // return message_packet;
    
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

    printf("Message packet constructed successfully.\n");
    return packetInfo;
}

