#ifndef misc_123
#define misc_123

#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

/* some useful things needed by many files ... */

/* ***************************** */
/* Types                         */
/* ***************************** */

typedef struct BitPacker_s
{
    unsigned char*      Ptr;                                    /* write pointer */
    unsigned int        BitBuffer;                              /* bitreader shifter */
    int                 Remaining;                              /* number of remaining in the shifter */
} BitPacker_t;

/* ***************************** */
/* Makros/Constants              */
/* ***************************** */

#define INVALID_PTS_VALUE                       0x200000000ull

/* ***************************** */
/* Prototypes                    */
/* ***************************** */

void PutBits(BitPacker_t * ld, unsigned int code, unsigned int length);
void FlushBits(BitPacker_t * ld);

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
  unsigned int i = 0;
  int pos = 0;

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

static inline int IsDreambox()
{
    struct stat buffer;   
    return (stat("/proc/stb/tpm/0/serial", &buffer) == 0);
}

#endif
