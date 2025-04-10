#include <stdio.h>
#include <stdlib.h>
#include <pcap.h>
#include "checksum.h"
#include <sys/types.h>
#include <string.h>        // For memcpy()
#include <stdint.h>        // For uint8_t, uint16_t, etc.
#include <netinet/in.h>    // For ntohs()
#include <arpa/inet.h>     // For inet_ntoa()
#include <netinet/ether.h> // For ether_ntoa()

typedef unsigned char u_char;
#define IP_STR_LEN 16
#define MAC_STR_LEN 18

// Function prototypes
const char* get_service_name(uint16_t port);
void udp(const uint8_t *packet);
void arp(const uint8_t *packet);
void convert_to_mac(const uint8_t mac_bytes[6], char *mac_str, size_t max_len);
void print_etherType(const char *etherType);
const char *get_packet_type(uint16_t etherType);
void convert_to_ip(const uint8_t *bytes, char *output, size_t length);
void ethernet(const uint8_t *packet_in);
void ip(const uint8_t *packet);
void my_packet_handler(u_char *args, const struct pcap_pkthdr *header, const uint8_t *packet);
void icmp(const uint8_t *packet);
void tcp(const uint8_t *packet, uint16_t ip_total_len, uint16_t ip_header_length,
         const uint8_t *ip_src, const uint8_t *ip_dest);
// Convert a 6-byte MAC address to a colon-separated string using the ether library.
void convert_to_mac(const uint8_t mac_bytes[6], char *mac_str, size_t max_len)
{
    struct ether_addr eth;
    // Copy the 6-byte MAC address into the structure's octet field.
    memcpy(eth.ether_addr_octet, mac_bytes, 6);

    // Use ether_ntoa to convert the struct ether_addr to a string.
    // Note: The result is stored in a static buffer.
    char *temp = ether_ntoa(&eth);

    // Copy the string to the user-provided buffer, ensuring not to overflow it.
    strncpy(mac_str, temp, max_len);
    // Ensure null termination.
    mac_str[max_len - 1] = '\0';
}

void print_etherType(const char *etherType)
{
    printf("\t\tType: %s\n", etherType);
}

const char* get_service_name(uint16_t port) {
    switch (port) {
        case 80:  return "HTTP";
        case 23:  return "TELNET";
        case 21:  return "FTP";
        case 110: return "POP3";
        case 25:  return "SMTP";
        case 53:  return "DNS";
        default:  return NULL; // Unknown service
    }
}


const char *get_packet_type(uint16_t etherType)
{
    switch (etherType)
    {
    case 0x0800: // IPv4
        return "IP";
    case 0x0806: // ARP
        return "ARP";
    default:
        return "Unknown";
    }
}

// Convert 4 bytes into a dotted-decimal IP string using inet_ntoa.
void convert_to_ip(const uint8_t *bytes, char *output, size_t length)
{
    struct in_addr addr;
    memcpy(&addr.s_addr, bytes, sizeof(addr.s_addr));

    char *temp = inet_ntoa(addr);
    strncpy(output, temp, length);
    output[length - 1] = '\0';
}

// Process and print the Ethernet header.
void ethernet(const uint8_t *packet_in)
{

    printf("\tEthernet Header\n");

    uint8_t dest_mac[6];
    uint8_t src_mac[6];
    uint16_t etherType;

    memcpy(dest_mac, packet_in, 6);
    memcpy(src_mac, packet_in + 6, 6);
    memcpy(&etherType, packet_in + 12, 2);
    etherType = ntohs(etherType);

    // Print Destination MAC.
    printf("\t\tDest MAC: ");
    char mac_str[MAC_STR_LEN];
    convert_to_mac(dest_mac, mac_str, sizeof(mac_str));
    printf("%s\n", mac_str);

    // Print Source MAC.
    printf("\t\tSource MAC: ");
    convert_to_mac(src_mac, mac_str, sizeof(mac_str));
    printf("%s\n", mac_str);

    // Print Ethernet type.
    const char *type = get_packet_type(etherType);
    print_etherType(type);
}

uint32_t packet_counter = 0;

// PCAP packet handler function.
void my_packet_handler(u_char *args, const struct pcap_pkthdr *header, const uint8_t *packet)
{
    // Ensure we have at least an Ethernet header.
    if (header->caplen < 14)
    {
        fprintf(stderr, "Packet too short for an Ethernet header.\n");
        return;
    }
    packet_counter++;
    printf("Packet number: %u  Packet Len: %u\n\n", packet_counter, header->caplen);

    ethernet(packet);

    uint16_t etherType;
    memcpy(&etherType, packet + 12, 2);
    etherType = ntohs(etherType);

    if (etherType == 0x0806)
    {
        // ARP packet; skip Ethernet header.
        arp(packet + 14);
    }
    else if (etherType == 0x0800)
    {
        // IPv4 packet; skip Ethernet header.
        ip(packet + 14);
    }

    printf("\n");
}

// Process and print the ARP header.
void arp(const uint8_t *packet)
{
    // ARP header fields (offsets based on assumed ARP packet layout).
    uint16_t opcode;                 // 2 bytes
    uint8_t source_mac[6];           // Sender MAC: 6 bytes
    uint8_t source_protocol_addr[4]; // Sender IP: 4 bytes
    uint8_t dest_mac[6];             // Target MAC: 6 bytes
    uint8_t dest_protocol_addr[4];   // Target IP: 4 bytes

    memcpy(&opcode, packet + 6, 2);
    memcpy(source_mac, packet + 8, 6);
    memcpy(source_protocol_addr, packet + 14, 4);
    memcpy(dest_mac, packet + 18, 6);
    memcpy(dest_protocol_addr, packet + 24, 4);

    opcode = ntohs(opcode);

    printf("\n\tARP header\n");
    printf("\t\tOpcode: ");
    if (opcode == 1)
        printf("Request\n");
    else if (opcode == 2)
        printf("Reply\n");
    else
        printf("Unknown\n");

    // Convert and print sender MAC and IP.
    char sender_mac_str[MAC_STR_LEN];
    char sender_ip_str[IP_STR_LEN];
    convert_to_mac(source_mac, sender_mac_str, sizeof(sender_mac_str));
    convert_to_ip(source_protocol_addr, sender_ip_str, sizeof(sender_ip_str));
    printf("\t\tSender MAC: %s\n", sender_mac_str);
    printf("\t\tSender IP: %s\n", sender_ip_str);

    // Convert and print target MAC and IP.
    char target_mac_str[MAC_STR_LEN];
    char target_ip_str[IP_STR_LEN];
    convert_to_mac(dest_mac, target_mac_str, sizeof(target_mac_str));
    convert_to_ip(dest_protocol_addr, target_ip_str, sizeof(target_ip_str));
    printf("\t\tTarget MAC: %s\n", target_mac_str);
    printf("\t\tTarget IP: %s\n", target_ip_str);
}

// Process and print the IP header.
void ip(const uint8_t *packet)
{
    printf("\n\tIP Header\n");

    // First byte: Version and Header Length.
    uint8_t header_length = (packet[0] & 0x0F) * 4;

    // Total Length: 2 bytes at offset 2.
    uint16_t ip_pdu_len;
    memcpy(&ip_pdu_len, packet + 2, 2);
    ip_pdu_len = ntohs(ip_pdu_len);

    // TTL: 1 byte at offset 8.
    uint8_t ttl = packet[8];

    // Protocol: 1 byte at offset 9.
    uint8_t protocol = packet[9];

    // Checksum: 2 bytes at offset 10.
    uint16_t checksum;
    memcpy(&checksum, packet + 10, 2);
    checksum = ntohs(checksum);

    // Source IP: 4 bytes at offset 12.
    uint8_t sender_ip[4];
    memcpy(sender_ip, packet + 12, 4);

    // Destination IP: 4 bytes at offset 16.
    uint8_t dest_ip[4];
    memcpy(dest_ip, packet + 16, 4);

    // Convert source and destination IP to printable strings.
    char sender_ip_str[IP_STR_LEN];
    char dest_ip_str[IP_STR_LEN];
    convert_to_ip(sender_ip, sender_ip_str, sizeof(sender_ip_str));
    convert_to_ip(dest_ip, dest_ip_str, sizeof(dest_ip_str));

    // Print the extracted IP header values.
    printf("\t\tIP PDU Len: %u\n", ip_pdu_len);
    printf("\t\tHeader Len (bytes): %u\n", header_length);
    printf("\t\tTTL: %u\n", ttl);
    printf("\t\tProtocol: ");
    switch (protocol)
    {
    case 1:
        printf("ICMP\n");
        break;
    case 6:
        printf("TCP\n");
        break;
    case 17:
        printf("UDP\n");
        break;
    default:
        printf("Unknown\n");
        break;
    }
    printf("\t\tChecksum: ");

    // if checksum is correct output "Correct (checksum)", else, output "Incorrect (checksum)"
    uint16_t computed_checksum = in_cksum((unsigned short *)packet, header_length);
    if (computed_checksum == 0)
    {
        printf("Correct (0x%04x)\n", checksum);
    }
    else
    {
        printf("Incorrect (0x%04x)\n", checksum);
    }

    printf("\t\tSender IP: %s\n", sender_ip_str);
    printf("\t\tDest IP: %s\n", dest_ip_str);

    if (protocol == 1)
    {
        icmp(packet + header_length);
    }
    else if (protocol == 6)
    {
        tcp(packet + header_length, ip_pdu_len, header_length, sender_ip, dest_ip);
    }
    else if (protocol == 17)
    {
        udp(packet + header_length);
    }
    else
    {
        return; // Unknown protocol
    }
}

#define PSEUDO_HDR_LEN 12

// tcp() now takes additional pointers to the 4-byte source and destination IP addresses.
void tcp(const uint8_t *packet, uint16_t ip_total_len, uint16_t ip_header_length,
         const uint8_t *ip_src, const uint8_t *ip_dest)
{
    printf("\n\tTCP Header\n");

    // Extract Data Offset using the top 4 bits of byte 12 (only one shift allowed).
    // (Byte at index 12 holds data offset in its upper 4 bits.)
    uint8_t offset = (packet[12] >> 4) & 0x0F;
    uint16_t tcp_header_length = offset * 4;

    // Source Port: 2 bytes at offset 0
    uint16_t src_port;
    memcpy(&src_port, packet + 0, 2);
    src_port = ntohs(src_port);

    // Destination Port: 2 bytes at offset 2
    uint16_t dest_port;
    memcpy(&dest_port, packet + 2, 2);
    dest_port = ntohs(dest_port);

    // Sequence Number: 4 bytes at offset 4
    uint32_t seq_number;
    memcpy(&seq_number, packet + 4, 4);
    seq_number = ntohl(seq_number);

    // Acknowledgment Number: 4 bytes at offset 8
    uint32_t ack_number;
    memcpy(&ack_number, packet + 8, 4);
    ack_number = ntohl(ack_number);

    // Now extract the full 16-bit Data Offset + Flags field from offset 12
    uint16_t tmp;
    memcpy(&tmp, packet + 12, 2);
    tmp = ntohs(tmp);
    // The lower 12 bits contain the flags.
    uint16_t flag_bits = tmp & 0x0FFF;
    int fin_flag = (flag_bits & 0x0001) ? 1 : 0;
    int syn_flag = (flag_bits & 0x0002) ? 1 : 0;
    int rst_flag = (flag_bits & 0x0004) ? 1 : 0;
    int ack_flag = (flag_bits & 0x0010) ? 1 : 0;

    // Window Size: 2 bytes at offset 14
    uint16_t window_size;
    memcpy(&window_size, packet + 14, 2);
    window_size = ntohs(window_size);

    // Checksum: 2 bytes at offset 16
    uint16_t checksum;
    memcpy(&checksum, packet + 16, 2);
    checksum = ntohs(checksum);

    // Calculate TCP segment length (header + payload) from the IP header.
    // Segment Length = total IP length - IP header length.
    int segment_length = ip_total_len - ip_header_length;

    // Print the extracted TCP header values.
    printf("\t\tSegment Length: %u\n", segment_length);
    // Get the source and destination ports correlated with the service names. If unknown print the port number
    const char *src_service = get_service_name(src_port);
    const char *dest_service = get_service_name(dest_port);
    if (src_service)
        printf("\t\tSource Port:  %s\n", src_service);
    else
        printf("\t\tSource Port:  %u\n", src_port);

    if (dest_service)
        printf("\t\tDest Port:  %s\n", dest_service);
    else
        printf("\t\tDest Port:  %u\n", dest_port);

    printf("\t\tSequence Number: %u\n", seq_number);
    printf("\t\tACK Number: %u\n", ack_number);
    printf("\t\tData Offset (bytes): %u\n", tcp_header_length);
    printf("\t\tSYN Flag: %s\n", syn_flag ? "Yes" : "No");
    printf("\t\tRST Flag: %s\n", rst_flag ? "Yes" : "No");
    printf("\t\tFIN Flag: %s\n", fin_flag ? "Yes" : "No");
    printf("\t\tACK Flag: %s\n", ack_flag ? "Yes" : "No");
    printf("\t\tWindow Size: %u\n", window_size);

    // -----------------------------------------------
    // Build the pseudo-header manually without using structs.
    // Pseudo-header layout (12 bytes):
    // Bytes 0-3: Source IP (4 bytes)
    // Bytes 4-7: Destination IP (4 bytes)
    // Byte 8: Zero (1 byte)
    // Byte 9: Protocol (1 byte, 6 for TCP)
    // Bytes 10-11: TCP Length (2 bytes) in network byte order.
    uint8_t pseudo[PSEUDO_HDR_LEN];

    // Copy source IP (4 bytes).
    memcpy(pseudo, ip_src, 4);
    // Copy destination IP (4 bytes).
    memcpy(pseudo + 4, ip_dest, 4);
    // Set byte 8 to 0.
    pseudo[8] = 0;
    // Set byte 9 to 6 for TCP.
    pseudo[9] = 6;
    // TCP segment length in network byte order.
    uint16_t net_tcp_length = htons(segment_length);
    memcpy(pseudo + 10, &net_tcp_length, 2);

    // Allocate buffer for pseudo-header concatenated with TCP segment.
    size_t buf_len = PSEUDO_HDR_LEN + segment_length;
    uint8_t *buf = malloc(buf_len);
    if (!buf)
    {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    // Copy pseudo-header.
    memcpy(buf, pseudo, PSEUDO_HDR_LEN);
    // Copy TCP segment.
    memcpy(buf + PSEUDO_HDR_LEN, packet, segment_length);

    // Compute the TCP checksum over the entire buffer.
    uint16_t computed_checksum = in_cksum((unsigned short *)buf, buf_len);
    free(buf);

    // The correct checksum is computed over the pseudo-header plus the TCP segment.
    if (computed_checksum == 0)
    {
        printf("\t\tChecksum: Correct (0x%04x)\n", checksum);
    }
    else
    {
        printf("\t\tChecksum: Incorrect (0x%04x)\n", checksum);
        printf("\t\tExpected: 0x%04x\n", computed_checksum);
    }

}

void udp(const uint8_t *packet)
{
    // Only need the source port and the destination port
    // Source Port: 2 bytes at offset 0
    uint16_t src_port;
    memcpy(&src_port, packet + 0, 2);
    src_port = ntohs(src_port);

    // Destination Port: 2 bytes at offset 2
    uint16_t dest_port;
    memcpy(&dest_port, packet + 2, 2);
    dest_port = ntohs(dest_port);

    // Now print and distguish the ports with a case statement
    printf("\n\tUDP Header\n");
    // Print Source Port.
    const char *src_service = get_service_name(src_port);
    const char *dest_service = get_service_name(dest_port);

    if (src_service)
        printf("\t\tSource Port:  %s \n", src_service);
    else

        printf("\t\tSource Port:  %u \n", src_port);

    // Print Destination Port.
    if (dest_service)
        printf("\t\tDest Port:  %s\n", dest_service);

    else
        printf("\t\tDest Port:  %u\n", dest_port);
}

void icmp(const uint8_t *packet)
{
    printf("\n\tICMP Header\n");
    // We only want the ICMP header, and then the type whether it be a request or reply
    // Type: 1 byte at offset 0
    uint8_t type = packet[0];
    if (type == 8)
    {
        printf("\t\tType: Request\n");
    }
    else if (type == 0)
    {
        printf("\t\tType: Reply\n");
    }
    else
    {
        printf("\t\tType: %u\n", type);
    }
}

int main(int argc, char *argv[])
{
    // Input the .pcap file that you want to analyze
    // Check if the user provided a filename as an argument.
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s <pcap_file>\n", argv[0]);
        exit(EXIT_FAILURE);
    }


    // Open pcap file.
    char errbuf[PCAP_ERRBUF_SIZE];
    const char *file_dir = argv[1];
    pcap_t *pcap_handle = pcap_open_offline(file_dir, errbuf);

    if (!pcap_handle)
    {
        fprintf(stderr, "Error opening trace file: %s\n", errbuf);
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
