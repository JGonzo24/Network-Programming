
#include <stdio.h>
#include <stdlib.h>
#include <pcap.h>
#include "checksum.h"
#include <sys/types.h>
#include <string.h>       // For memcpy()
#include <stdint.h>       // For uint8_t, uint16_t, etc.
#include <netinet/in.h>   // For ntohs()
#include <arpa/inet.h>    // For inet_ntop()

 
typedef unsigned char u_char;
#define IP_STR_LEN 16
void arp(const uint8_t *packet);
//-------------------------------------------------------------
// Utility function: print MAC addresses.
// Prints each byte as hex. (The sample output omits a leading zero for values < 0x10.)
void print_mac(const uint8_t *mac) {
    for (int i = 0; i < 6; i++) {
        // Print in lowercase hexadecimal without a leading zero
        // (use %x instead of %02x to match sample output).
        printf("%x", mac[i]);
        if (i < 5)
            printf(":");
    }
    printf("\n");
}

void print_etherType(const char* etherType)
{
    printf("\t\tType: %s", etherType);
}

const char* get_packet_type(uint16_t etherType)
{
    switch (etherType) {
        case 0x0800:  // IPv4
            return "IPv4";
        case 0x0806:  // ARP
            return "ARP";
        case 0x86DD:  // IPv6
            return "IPv6";
        default:
            return "Unknown";
    }
}

char* convert_to_ip(const uint8_t *bytes, char* output, size_t length)
{
    snprintf(output, length, "%u.%u.%u.%u", bytes[0], bytes[1],bytes[2],bytes[3]);
    return output;
}

void ethernet(const uint8_t *packet_in)
{
    printf("\tEthernet Header\n");
    uint8_t dest_mac[6];
    uint8_t src_mac[6];
    uint16_t etherType;

    memcpy(dest_mac, packet_in, 6);
    memcpy(src_mac, packet_in + 6, 6);
    memcpy(&etherType, packet_in + 12, 2);
    // change the endianness from network to host
    etherType = ntohs(etherType);
    // Now that we have destination and source address, lets print them!
    printf("\t\tDest MAC: ");
    print_mac(dest_mac);

    printf("\t\tSource MAC: ");
    print_mac(src_mac);
    
    const char* type = get_packet_type(etherType);
    print_etherType(type);

}

//-------------------------------------------------------------
uint32_t packet_counter = 0;
void my_packet_handler(u_char *args, const struct pcap_pkthdr *header, const uint8_t *packet)
{
    // first check if the ethernet packet is even an ethernet packet
    if (header->caplen < 14)
    {
        fprintf(stderr,"Packet too short for an Ethernet header.\n");
        return;
    }
    packet_counter++;
    printf("Packet Number: %i  Packet Len: %i\n\n", packet_counter, header->caplen);

    ethernet(packet);

    uint16_t etherType;
    // Need to ensure we have the right endianess here
    memcpy(&etherType, packet + 12, 2);
    etherType = ntohs(etherType);

    if(etherType == 0x0806)
    {
        arp(packet+14);
    }
    printf("\n");
}



void icmp()
{

}

void arp(const uint8_t *packet)
{
    // If we determine the ethernet packet is a ARP type, then we need to get the OP code, sender MAC, Sender IP, 
    // Target MAC, and Target IP
    // First things first 
    uint16_t opcode; // 2 bytes
    uint8_t source_mac[6]; // 6 bytes
    uint8_t source_protocol_addr[4]; // 4 bytes
    uint8_t dest_mac[6]; // 6 bytes
    uint8_t dest_protocol_addr[4]; // 4 bytes

    // Read them in from the packet
    memcpy(&opcode, packet + 6, 2);
    memcpy(source_mac, packet + 8, 6);
    memcpy(source_protocol_addr, packet + 14, 4);
    memcpy(dest_mac, packet + 18, 6);
    memcpy(dest_protocol_addr, packet + 24, 4);

    // for the op code, since it was read directly from network it needs be changed
    opcode = ntohs(opcode);

    printf("\n\tARP header\n");
    printf("\t\tOpcode: ");
    if (opcode == 1)
        printf("Request\n");
    else if (opcode == 2)
        printf("Reply\n");
    else
        printf("Unknown (%u)\n", opcode);

    printf("\t\tSender MAC: ");
    print_mac(source_mac);

    char source_ip[IP_STR_LEN];
    convert_to_ip(source_protocol_addr, source_ip, IP_STR_LEN);
    printf("\t\tSender IP: %s\n", source_ip);

    printf("\t\tTarget MAC: ");
    print_mac(dest_mac);

    char dest_ip[IP_STR_LEN];
    convert_to_ip(dest_protocol_addr, dest_ip, IP_STR_LEN);
    printf("\t\tTarget IP: %s\n", dest_ip);
}

void ip()
{

}

void tcp()
{
    
}

int main()
{
    // if(argc != 2)
    // {
    //     fprintf(stderr, "Input into cmd line: %s <trace file>\n", argv[0]);
    //     exit(EXIT_FAILURE);
    // }
    // Open File with PCAP library
    char errbuf[PCAP_ERRBUF_SIZE];
    pcap_t *pcap_handle = pcap_open_offline("ArpTest.pcap", errbuf);

    if (!pcap_handle)
    {
        fprintf(stderr, "Error during opening trace file: %s\n",errbuf);
        exit(EXIT_FAILURE);
    }

    if (pcap_loop(pcap_handle, 0, my_packet_handler, NULL) < 0) 
    {
        fprintf(stderr, "Error processing packets: %s\n", pcap_geterr(pcap_handle));
        pcap_close(pcap_handle);
        exit(EXIT_FAILURE);
    }
    pcap_close(pcap_handle);
    return 0;
}
