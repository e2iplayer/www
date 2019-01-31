#ifndef _exteplayer3_misc_
#define _exteplayer3_misc_

#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdint.h>

/* some useful things needed by many files ... */

/* ***************************** */
/* Types                         */
/* ***************************** */

typedef struct BitPacker_s
{
    uint8_t   *Ptr;                                    /* write pointer */
    uint32_t   BitBuffer;                              /* bitreader shifter */
    int32_t    Remaining;                              /* number of remaining in the shifter */
} BitPacker_t;

typedef enum {
    STB_UNKNOWN,
    STB_DREAMBOX,
    STB_VUPLUS,
    STB_HISILICON,
    STB_OTHER=999,
} stb_type_t;

/* ***************************** */
/* Makros/Constants              */
/* ***************************** */
#define INVALID_PTS_VALUE                       0x200000000ull

/* ***************************** */
/* Prototypes                    */
/* ***************************** */
void PutBits(BitPacker_t * ld, uint32_t code, uint32_t length);
void FlushBits(BitPacker_t * ld);
int8_t PlaybackDieNow(int8_t val); 
stb_type_t GetSTBType();

/* ***************************** */
/* MISC Functions                */
/* ***************************** */

static inline char *getExtension(char * name)
{
    if (name) 
    {
        char *ext = strrchr(name, '.');
        if (ext)
        {
            return ext + 1;
        }
    }
    return NULL;
}

static inline uint32_t ReadUint32(uint8_t *buffer)
{
    uint32_t num = (uint32_t)buffer[0] << 24 |
                   (uint32_t)buffer[1] << 16 |
                   (uint32_t)buffer[2] << 8  |
                   (uint32_t)buffer[3];
    return num;
}

static inline uint16_t ReadUInt16(uint8_t *buffer)
{
    uint16_t num = (uint16_t)buffer[0] << 8 |
                   (uint16_t)buffer[1];
    return num;
}

#endif // _exteplayer3_misc_
