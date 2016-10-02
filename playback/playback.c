/*
 * GPL
 * duckbox 2010
 */

/* ***************************** */
/* Includes                      */
/* ***************************** */

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <poll.h>
#include <stdint.h>

#include "playback.h"
#include "common.h"
#include "misc.h"

/* ***************************** */
/* Makros/Constants              */
/* ***************************** */
// SULGE DEBUG
//#define SAM_WITH_DEBUG

#ifdef SAM_WITH_DEBUG
#define PLAYBACK_DEBUG
#else
#define PLAYBACK_SILENT
#endif

static short debug_level = 20;

#ifdef PLAYBACK_DEBUG
#define playback_printf(level, fmt, x...) do { \
if (debug_level >= level) printf("[%s:%s] " fmt, __FILE__, __FUNCTION__, ## x); } while (0)
#else
#define playback_printf(level, fmt, x...)
#endif

#ifndef PLAYBACK_SILENT
#define playback_err(fmt, x...) do { printf("[%s:%s] " fmt, __FILE__, __FUNCTION__, ## x); } while (0)
#else
#define playback_err(fmt, x...)
#endif

#define cERR_PLAYBACK_NO_ERROR      0
#define cERR_PLAYBACK_ERROR        -1

#define cMaxSpeed_ff   128 /* fixme: revise */
#define cMaxSpeed_fr   -320 /* fixme: revise */

/* ***************************** */
/* Varaibles                     */
/* ***************************** */

/* ***************************** */
/* Prototypes                    */
/* ***************************** */
extern void set_pause_timeout(uint8_t pause);
static int32_t PlaybackTerminate(Context_t  *context);

/* ***************************** */
/* MISC Functions                */
/* ***************************** */
int8_t PlaybackDieNow(int8_t val)
{
    static int8_t dieNow = 0;
    if(val)
    {
        dieNow = 1;
    }
    return dieNow;
}

/* ***************************** */
/* Functions                     */
/* ***************************** */

static int PlaybackStop(Context_t  *context);

static int PlaybackOpen(Context_t  *context, PlayFiles_t *pFiles)
{
    if (context->playback->isPlaying)
    {
        PlaybackStop(context);
    }
    
    char *uri = pFiles->szFirstFile;
    
    playback_printf(10, "URI=%s\n", uri);

    if (context->playback->isPlaying)
    { // shouldn't happen
        playback_err("playback already running\n");
        return cERR_PLAYBACK_ERROR;
    }

    char * extension = NULL;

    context->playback->uri = strdup(uri);

    context->playback->isFile = 0;
    context->playback->isHttp = 0;

    if (!strncmp("file://", uri, 7) || !strncmp("myts://", uri, 7)) 
    {
        context->playback->isFile = 1;
        if (!strncmp("myts://", uri, 7)) 
        {
            memcpy(context->playback->uri, "file", 4);
            context->playback->noprobe = 1;
        } 
        else
        {
            context->playback->noprobe = 0;
        }

        extension = getExtension(context->playback->uri+7);
        if(!extension)
        {
            playback_err("Wrong extension (%s)\n", context->playback->uri+7);
            return cERR_PLAYBACK_ERROR;
        }
    } 
    else if (strstr(uri, "://")) 
    {
        context->playback->isHttp = 1;
        extension = "mp3";
        if (!strncmp("mms://", uri, 6)) 
        {
            // mms is in reality called rtsp, and ffmpeg expects this
            char * tUri = (char*)malloc(strlen(uri) + 2);
            strncpy(tUri+1, uri, strlen(uri)+1);
            strncpy(tUri, "rtsp", 4);
            free(context->playback->uri);
            context->playback->uri = tUri;
        }
    }
    else
    {
        playback_err("Unknown stream (%s)\n", uri);
        return cERR_PLAYBACK_ERROR;
    }

    pFiles->szFirstFile = context->playback->uri;
    if ((context->container->Command(context, CONTAINER_ADD, extension) < 0)
        ||  (!context->container->selectedContainer)
        ||  (context->container->selectedContainer->Command(context, CONTAINER_INIT, pFiles) < 0))
    {
        playback_err("CONTAINER_ADD failed\n");
        return cERR_PLAYBACK_ERROR;
    }

    playback_printf(10, "exiting with value 0\n");

    return cERR_PLAYBACK_NO_ERROR;
}

static int PlaybackClose(Context_t  *context) 
{
    int ret = cERR_PLAYBACK_NO_ERROR;

    playback_printf(10, "\n");

    if (context->container->Command(context, CONTAINER_DEL, NULL) < 0)
    {
        playback_err("container delete failed\n");
    }

    context->manager->audio->Command(context, MANAGER_DEL, NULL);
    context->manager->video->Command(context, MANAGER_DEL, NULL);

    context->playback->isPaused     = 0;
    context->playback->isPlaying    = 0;
    context->playback->isForwarding = 0;
    context->playback->BackWard     = 0;
    context->playback->SlowMotion   = 0;
    context->playback->Speed        = 0;
    if(context->playback->uri) 
    {
        free(context->playback->uri);
        context->playback->uri = NULL;
    }

    playback_printf(10, "exiting with value %d\n", ret);

    return ret;
}

static int PlaybackPlay(Context_t  *context) 
{
    pthread_attr_t attr;
    int ret = cERR_PLAYBACK_NO_ERROR;

    playback_printf(10, "\n");

    if (!context->playback->isPlaying) 
    {
        context->playback->AVSync = 1;
        context->output->Command(context, OUTPUT_AVSYNC, NULL);

        context->playback->isCreationPhase = 1;	// allows the created thread to go into wait mode
        ret = context->output->Command(context, OUTPUT_PLAY, NULL);

        if (ret != 0) 
        {
            playback_err("OUTPUT_PLAY failed!\n");
            playback_err("clearing isCreationPhase!\n");
            context->playback->isCreationPhase = 0;	// allow thread to go into next state
            context->playback->isPlaying       = 0;
            context->playback->isPaused        = 0;
            context->playback->isForwarding    = 0;
            context->playback->BackWard        = 0;
            context->playback->SlowMotion      = 0;
            context->playback->Speed           = 0;
            context->container->selectedContainer->Command(context, CONTAINER_STOP, NULL);
        }
        else 
        {
            context->playback->isPlaying    = 1;
            context->playback->isPaused     = 0;
            context->playback->isForwarding = 0;
            context->playback->BackWard     = 0;
            context->playback->SlowMotion   = 0;
            context->playback->Speed        = 1;

            playback_printf(10, "clearing isCreationPhase!\n");

            context->playback->isCreationPhase = 0;	// allow thread to go into next state

            ret = context->container->selectedContainer->Command(context, CONTAINER_PLAY, NULL);
            if (ret != 0) {
                playback_err("CONTAINER_PLAY failed!\n");
            }

        }
    }
    else
    {
        playback_err("playback already running\n");
        ret = cERR_PLAYBACK_ERROR;
    }

    playback_printf(10, "exiting with value %d\n", ret);

    return ret;
}

static int PlaybackPause(Context_t  *context) 
{
    int ret = cERR_PLAYBACK_NO_ERROR;

    playback_printf(10, "\n");

    if (context->playback->isPlaying && !context->playback->isPaused) 
    {
        set_pause_timeout(1);
        
        context->output->Command(context, OUTPUT_PAUSE, NULL);

        context->playback->isPaused     = 1;
        //context->playback->isPlaying  = 1;
        context->playback->isForwarding = 0;
        context->playback->BackWard     = 0;
        context->playback->SlowMotion   = 0;
        context->playback->Speed        = 1;
    }
    else
    {
        playback_err("playback not playing or already in pause mode\n");
        ret = cERR_PLAYBACK_ERROR;
    }

    playback_printf(10, "exiting with value %d\n", ret);
    return ret;
}

static int32_t PlaybackContinue(Context_t  *context) 
{
    int32_t ret = cERR_PLAYBACK_NO_ERROR;

    playback_printf(10, "\n");

    if (context->playback->isPlaying &&
       (context->playback->isPaused || context->playback->isForwarding || 
        context->playback->BackWard || context->playback->SlowMotion)) 
    {

        set_pause_timeout(0);

        context->output->Command(context, OUTPUT_CONTINUE, NULL);

        context->playback->isPaused     = 0;
        //context->playback->isPlaying  = 1;
        context->playback->isForwarding = 0;
        context->playback->BackWard     = 0;
        context->playback->SlowMotion   = 0;
        context->playback->Speed        = 1;
    }
    else
    {
        playback_err("continue not possible\n");
        ret = cERR_PLAYBACK_ERROR;
    }

    playback_printf(10, "exiting with value %d\n", ret);
    return ret;
}

static int32_t PlaybackStop(Context_t  *context) 
{
    int32_t ret = cERR_PLAYBACK_NO_ERROR;

    playback_printf(10, "\n");
    
    PlaybackDieNow(1);

    if (context && context->playback && context->playback->isPlaying) 
    {

        context->playback->isPaused     = 0;
        context->playback->isPlaying    = 0;
        context->playback->isForwarding = 0;
        context->playback->BackWard     = 0;
        context->playback->SlowMotion   = 0;
        context->playback->Speed        = 0;

        context->output->Command(context, OUTPUT_STOP, NULL);
        context->container->selectedContainer->Command(context, CONTAINER_STOP, NULL);

    }
    else
    {
        playback_err("stop not possible\n");
        ret = cERR_PLAYBACK_ERROR;
    }

    playback_printf(10, "exiting with value %d\n", ret);
    return ret;
}

static int32_t PlaybackTerminate(Context_t  *context) 
{
    int32_t ret = cERR_PLAYBACK_NO_ERROR;

    playback_printf(20, "\n");
    
    PlaybackDieNow(1);

    if ( context && context->playback && context->playback->isPlaying ) 
    {
        //First Flush and than delete container, else e2 cant read length of file anymore

        if (context->output->Command(context, OUTPUT_FLUSH, NULL) < 0)
        {
            playback_err("failed to flush output.\n");
        }

        ret = context->container->selectedContainer->Command(context, CONTAINER_STOP, NULL);

        context->playback->isPaused     = 0;
        context->playback->isPlaying    = 0;
        context->playback->isForwarding = 0;
        context->playback->BackWard     = 0;
        context->playback->SlowMotion   = 0;
        context->playback->Speed        = 0;

    } 
    else
    {
        playback_err("%p %p %d\n", context, context->playback, context->playback->isPlaying);

        /* fixme: konfetti: we should return an error here but this seems to be a condition which
         * can happen and is not a real error, which leads to a dead neutrino. should investigate
         * here later.
         */
    }

    playback_printf(20, "exiting with value %d\n", ret);
    return ret;
}

static int32_t PlaybackSeek(Context_t  *context, int64_t *pos, uint8_t absolute) 
{
    int32_t ret = cERR_PLAYBACK_NO_ERROR;

    playback_printf(10, "pos: %lldd\n", *pos);

    if (context->playback->isPlaying && !context->playback->isForwarding && !context->playback->BackWard && !context->playback->SlowMotion && !context->playback->isPaused) 
    {
        context->playback->isSeeking = 1;
        context->output->Command(context, OUTPUT_CLEAR, NULL);
        if (absolute)
        {
            context->container->selectedContainer->Command(context, CONTAINER_SEEK_ABS, pos);
        }
        else
        {
            context->container->selectedContainer->Command(context, CONTAINER_SEEK, pos);
        }
        context->playback->isSeeking = 0;
    } 
    else
    {
        playback_err("not possible\n");
        ret = cERR_PLAYBACK_ERROR;
    }

    playback_printf(10, "exiting with value %d\n", ret);

    return ret;
}

static int32_t PlaybackPts(Context_t  *context, int64_t *pts)
{
    int32_t ret = cERR_PLAYBACK_NO_ERROR;

    playback_printf(20, "\n");

    *pts = 0;

    if (context->playback->isPlaying)
    {
        ret = context->output->Command(context, OUTPUT_PTS, pts);
    } 
    else
    {
        playback_err("not possible\n");
        ret = cERR_PLAYBACK_ERROR;
    }

    playback_printf(20, "exiting with value %d\n", ret);

    return ret;
}

static int32_t PlaybackGetFrameCount(Context_t  *context, int64_t *frameCount) 
{
    int ret = cERR_PLAYBACK_NO_ERROR;

    playback_printf(20, "\n");

    *frameCount = 0;

    if (context->playback->isPlaying) 
    {
        ret = context->output->Command(context, OUTPUT_GET_FRAME_COUNT, frameCount);
    } else
    {
        playback_err("not possible\n");
        ret = cERR_PLAYBACK_ERROR;
    }

    playback_printf(20, "exiting with value %d\n", ret);

    return ret;
}

static int32_t PlaybackLength(Context_t  *context, int64_t *length) 
{
    int32_t ret = cERR_PLAYBACK_NO_ERROR;

    playback_printf(20, "\n");

    *length = -1;

    if (context->playback->isPlaying) 
    {
        if (context->container && context->container->selectedContainer)
        {
            context->container->selectedContainer->Command(context, CONTAINER_LENGTH, length);
        }
    } 
    else
    {
        playback_err("not possible\n");
        ret = cERR_PLAYBACK_ERROR;
    }

    playback_printf(20, "exiting with value %d\n", ret);
    return ret;
}

static int32_t PlaybackSwitchAudio(Context_t  *context, int32_t *track)
{
    int32_t ret = cERR_PLAYBACK_NO_ERROR;
    int32_t curtrackid = 0;
    int32_t nextrackid = 0;

    playback_printf(10, "\n");

    if (context && context->playback && context->playback->isPlaying)
    {
        if (context->manager && context->manager->audio) 
        {
            context->manager->audio->Command(context, MANAGER_GET, &curtrackid);
            context->manager->audio->Command(context, MANAGER_SET, track);
            context->manager->audio->Command(context, MANAGER_GET, &nextrackid);
        }
        else
        {
            playback_err("switch audio not possible\n");
            ret = cERR_PLAYBACK_ERROR;
        }

        if(nextrackid != curtrackid)
        {

            //PlaybackPause(context);
            if (context->output && context->output->audio)
            {
                context->output->audio->Command(context, OUTPUT_SWITCH, (void*)"audio");
            }

            if (context->container && context->container->selectedContainer)
            {
                context->container->selectedContainer->Command(context, CONTAINER_SWITCH_AUDIO, &nextrackid);
            }
            //PlaybackContinue(context);
        }
    } 
    else
    {
        playback_err("switch audio not possible\n");
        ret = cERR_PLAYBACK_ERROR;
    }

    playback_printf(10, "exiting with value %d\n", ret);
    return ret;
}

static int32_t PlaybackSwitchSubtitle(Context_t  *context, int32_t *track) 
{
    int32_t ret = cERR_PLAYBACK_NO_ERROR;
    int32_t curtrackid = -1;
    int32_t nextrackid = -1;

    playback_printf(10, "Track: %d\n", *track);

    if (context && context->playback && context->playback->isPlaying )
    {
        if (context->manager && context->manager->subtitle) 
        {
            context->manager->subtitle->Command(context, MANAGER_GET, &curtrackid);
            context->manager->subtitle->Command(context, MANAGER_SET, track);
            context->manager->subtitle->Command(context, MANAGER_GET, &nextrackid);
          
            if (curtrackid != nextrackid && nextrackid > -1)
            {
                if (context->output && context->output->subtitle)
                {
                    context->output->subtitle->Command(context, OUTPUT_SWITCH, (void*)"subtitle");
                }

                if (context->container && context->container->selectedContainer)
                {
                    context->container->selectedContainer->Command(context, CONTAINER_SWITCH_SUBTITLE, &nextrackid);
                }
            }
        } 
        else
        {
            ret = cERR_PLAYBACK_ERROR;
            playback_err("no subtitle\n");
        }
    } 
    else
    {
        playback_err("not possible\n");
        ret = cERR_PLAYBACK_ERROR;
    }

    playback_printf(10, "exiting with value %d\n", ret);

    return ret;
}

static int32_t PlaybackInfo(Context_t  *context, char **infoString) 
{
    int32_t ret = cERR_PLAYBACK_NO_ERROR;

    playback_printf(10, "\n");

    /* konfetti comment: 
     * removed if clause here (playback running) because its 
     * not necessary for all container. e.g. in case of ffmpeg 
     * container playback must not play to get the info.
     */
    if (context->container && context->container->selectedContainer)
    {
        context->container->selectedContainer->Command(context, CONTAINER_INFO, infoString);
    }

    playback_printf(10, "exiting with value %d\n", ret);

    return ret;
}

static int32_t Command(void* _context, PlaybackCmd_t command, void *argument) 
{
    Context_t* context = (Context_t*) _context; /* to satisfy compiler */
    int32_t ret = cERR_PLAYBACK_NO_ERROR;

    playback_printf(20, "Command %d\n", command);


    switch(command) 
    {
        case PLAYBACK_OPEN: 
        {
            ret = PlaybackOpen(context, (PlayFiles_t*)argument);
            break;
        }
        case PLAYBACK_CLOSE: 
        {
            ret = PlaybackClose(context);
            break;
        }
        case PLAYBACK_PLAY: 
        {
            ret = PlaybackPlay(context);
            break;
        }
        case PLAYBACK_STOP: 
        {
            ret = PlaybackStop(context);
            break;
        }
        case PLAYBACK_PAUSE: 
        {
            ret = PlaybackPause(context);
            break;
        }
        case PLAYBACK_CONTINUE: 
        {
            ret = PlaybackContinue(context);
            break;
        }
        case PLAYBACK_TERM: 
        {
            ret = PlaybackTerminate(context);
            break;
        }
        case PLAYBACK_SEEK: 
        {
            ret = PlaybackSeek(context, (int64_t*)argument, 0);
            break;
        }
        case PLAYBACK_SEEK_ABS: 
        {
            ret = PlaybackSeek(context, (int64_t*)argument, -1);
            break;
        }
        case PLAYBACK_PTS: 
        {
            ret = PlaybackPts(context, (int64_t*)argument);
            break;
        }
        case PLAYBACK_LENGTH: 
        {
            ret = PlaybackLength(context, (int64_t*)argument);
            break;
        }
        case PLAYBACK_SWITCH_AUDIO: 
        {
            ret = PlaybackSwitchAudio(context, (int*)argument);
            break;
        }
        case PLAYBACK_SWITCH_SUBTITLE: 
        {
            ret = PlaybackSwitchSubtitle(context, (int*)argument);
            break;
        }
        case PLAYBACK_INFO: 
        {
            ret = PlaybackInfo(context, (char**)argument);
            break;
        }
        case PLAYBACK_GET_FRAME_COUNT: 
        { 
            ret = PlaybackGetFrameCount(context, (uint64_t*)argument);
            break;
        }
        default:
            playback_err("PlaybackCmd %d not supported!\n", command);
            ret = cERR_PLAYBACK_ERROR;
            break;
    }

    playback_printf(20, "exiting with value %d\n", ret);

    return ret;
}

/*
 * This is very unreadable and must be changed
 */
PlaybackHandler_t PlaybackHandler = {
    "Playback", //name
    -1,         //fd
    0,          //isFile
    0,          //isHttp
    0,          //isPlaying
    0,          //isPaused
    0,          //isForwarding
    0,          //isSeeking
    0,          //isCreationPhase
    0,          //BackWard
    0,          //SlowMotion
    0,          //Speed
    0,          //AVSync
    0,          //isVideo
    0,          //isAudio
    0,          //isSubtitle
    0,          //abortRequested
    &Command,   //Command
    "",         //uri
    0,          //size
    0,          //noprobe
    0,          //isLoopMode
    0,          //isTSLiveMode
};
