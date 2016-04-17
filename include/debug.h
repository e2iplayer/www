#ifndef debug_123
#define debug_123

#include <stdio.h>
#include <errno.h>

static inline void Hexdump(unsigned char *Data, int length)
{

    int k;
    for (k = 0; k < length; k++)
    {
        printf("%02x ", Data[k]);
        if (((k+1)&31)==0)
            printf("\n");
    }
    printf("\n");

}

#endif
