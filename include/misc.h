#ifndef misc_123
#define misc_123

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

/* the function returns the base name */
static inline char * basename(char * name)
{
  int i = 0;
  int pos = 0;

  while(name[i] != 0)
  {
    if(name[i] == '/')
      pos = i;
    i++;
  }

  if(name[pos] == '/')
    pos++;

  return name + pos;
}

/* the function returns the directry name */
static inline char * dirname(char * name)
{
  static char path[100];
  uint32_t i = 0;
  int32_t pos = 0;

  while((name[i] != 0) && (i < sizeof(path)))
  {
    if(name[i] == '/')
    {
        pos = i;
    }
    path[i] = name[i];
    i++;
  }

  path[i] = 0;
  path[pos] = 0;

  return path;
}

static inline int32_t IsDreambox()
{
    struct stat buffer;   
    return (stat("/proc/stb/tpm/0/serial", &buffer) == 0);
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

#endif
