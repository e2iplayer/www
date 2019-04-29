#ifndef COMMON_H_
#define COMMON_H_

#include<stdint.h>

#include "container.h"
#include "output.h"
#include "manager.h"
#include "playback.h"
#include <pthread.h>

typedef struct PlayFiles_t 
{ 
    char *szFirstFile;
    char *szSecondFile;
    char *szFirstMoovAtomFile;
    char *szSecondMoovAtomFile;
    uint64_t iFirstFileSize;
    uint64_t iSecondFileSize;
    uint64_t iFirstMoovAtomOffset;
    uint64_t iSecondMoovAtomOffset;
} PlayFiles_t;

typedef struct Context_s 
{
    PlaybackHandler_t	*playback;
    ContainerHandler_t	*container;
    OutputHandler_t		*output;
    ManagerHandler_t	*manager;
} Context_t;

int container_ffmpeg_update_tracks(Context_t *context, char *filename, int initial);

const char* GetGraphicSubPath();
int32_t GetGraphicWindowWidth();
int32_t GetGraphicWindowHeight();

void E2iSendMsg(const char * format, ...);
void E2iStartMsg(void);
void E2iEndMsg(void);

#endif
