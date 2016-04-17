#ifndef COMMON_H_
#define COMMON_H_

#include<stdint.h>

#include "container.h"
#include "output.h"
#include "manager.h"
#include "playback.h"
#include <pthread.h>

typedef char PlayFilesTab_t[2];

typedef struct PlayFiles_t 
{ 
    char *szFirstFile;
    char *szSecondFile;
} PlayFiles_t;

typedef struct Context_s 
{
    PlaybackHandler_t	*playback;
    ContainerHandler_t	*container;
    OutputHandler_t		*output;
    ManagerHandler_t	*manager;
} Context_t;

int container_ffmpeg_update_tracks(Context_t *context, char *filename, int initial);
#endif
