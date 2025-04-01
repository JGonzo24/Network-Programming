#include <stdio.h>
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib") // Link the executable with the Winsock library

int main()
{
    WSADATA d;
    // WSA Startup returns 0 on success, 1 on failure 
    // Request version 2.2 into the Winsock data structure 
    if (WSAStartup(MAKEWORD(2,2), &d))
    {
        printf("Failed to initialize.\n");
        return -1;
    }
    // Print the version we actually got:

    WSACleanup();
    printf("Ok.\n");
    return 0;
}