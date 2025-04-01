#include <stdio.h>
#include <time.h>

int main() 
{
    time_t timer;
    time(&timer);
    printf("Local time is: %s", ctime(&timer));
    printf("Hello World");
    return 0;
}

// This is for testing. This is coming from the desktop to the PC

// This for testing 3 after git cloneing from desktop to the pc 
// test test 


