#include <stdio.h>
#include <time.h>

int main() 
{
    time_t timer;
    time(&timer);
    printf("Local time is: %s", ctime(&timer));
    return 0;
}

// This is for testing. This is coming from the desktop to the PC
