/*
 * LinuxDVB Output handling.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

/* ***************************** */
/* Includes                      */
/* ***************************** */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <linux/dvb/video.h>
#include <linux/dvb/audio.h>
#include <memory.h>
#include <asm/types.h>
#include <pthread.h>
#include <errno.h>
#include <poll.h>

#include "bcm_ioctls.h"

#include "common.h"
#include "output.h"
#include "writer.h"
#include "misc.h"
#include "pes.h"

/* ***************************** */
/* Makros/Constants              */
/* ***************************** */

//#define LINUXDVB_DEBUG
#define LINUXDVB_SILENT

static uint16_t debug_level = 0;

static const char FILENAME[] = __FILE__;

#ifdef LINUXDVB_DEBUG
#define linuxdvb_printf(level, fmt, x...) do { \
if (debug_level >= level) printf("[%s:%s] " fmt, __FILE__, __FUNCTION__, ## x ); } while (0)
#else
#define linuxdvb_printf(x...)
#endif

#ifndef LINUXDVB_SILENT
#define linuxdvb_err(fmt, x...) do { printf("[%s:%s] " fmt, __FILE__, __FUNCTION__, ## x); } while (0)
#else
#define linuxdvb_err(x...)
#endif

#define cERR_LINUXDVB_NO_ERROR      0
#define cERR_LINUXDVB_ERROR        -1

static const char VIDEODEV[] = "/dev/dvb/adapter0/video0";
static const char AUDIODEV[] = "/dev/dvb/adapter0/audio0";

static int videofd 	= -1;
static int audiofd 	= -1;

struct DVBApiVideoInfo_s
{
    int aspect_ratio;
    int progressive;
    int frame_rate;
    int width, height;
};
static struct DVBApiVideoInfo_s videoInfo = {-1,-1,-1,-1,-1};

unsigned long long int sCURRENT_PTS = 0;
bool isBufferedOutput = false;

pthread_mutex_t LinuxDVBmutex;

/* ***************************** */
/* Prototypes                    */
/* ***************************** */
int32_t LinuxDvbBuffOpen(Context_t *context, char *type, int outfd);
int32_t LinuxDvbBuffClose(Context_t *context);
int32_t LinuxDvbBuffFlush(Context_t *context);
int32_t LinuxDvbBuffResume(Context_t *context);

ssize_t BufferingWriteV(int fd, const struct iovec *iov, int ic);
int32_t LinuxDvbBuffSetSize(const uint32_t bufferSize);
uint32_t LinuxDvbBuffGetSize();

int LinuxDvbStop(Context_t  *context, char * type);

/* ***************************** */
/* MISC Functions                */
/* ***************************** */

void getLinuxDVBMutex(const char *filename __attribute__((unused)), const char *function __attribute__((unused)), int line __attribute__((unused)))
{
    pthread_mutex_lock(&LinuxDVBmutex);
}

void releaseLinuxDVBMutex(const char *filename __attribute__((unused)), const char *function __attribute__((unused)), int line __attribute__((unused))) {
    pthread_mutex_unlock(&LinuxDVBmutex);
}

static int LinuxDvbMapBypassMode(int bypass)
{
    if (0x30 == bypass && IsDreambox())
    {
        return 0x0f;
    }
    return bypass;
}

int LinuxDvbOpen(Context_t  *context __attribute__((unused)), char *type)
{
    uint8_t video = !strcmp("video", type);
    uint8_t audio = !strcmp("audio", type);

    linuxdvb_printf(10, "v%d a%d\n", video, audio);

    if (video && videofd < 0) 
    {
        videofd = open(VIDEODEV, O_RDWR | O_NONBLOCK);

        if (videofd < 0)
        {
            linuxdvb_err("failed to open %s - errno %d, %s\n", VIDEODEV, errno, strerror(errno));
            linuxdvb_err("%s\n", );
            return cERR_LINUXDVB_ERROR;
        }
 
        if (ioctl( videofd, VIDEO_CLEAR_BUFFER) == -1)
        {
            linuxdvb_err("VIDEO_CLEAR_BUFFER: ERROR %d, %s\n", errno, strerror(errno));
        }

        if (ioctl( videofd, VIDEO_SELECT_SOURCE, (void*)VIDEO_SOURCE_MEMORY) == -1)
        {
            linuxdvb_err("VIDEO_SELECT_SOURCE: ERROR %d, %s\n", errno, strerror(errno));
        }
        
        if (ioctl(videofd, VIDEO_FREEZE) == -1)
        {
            linuxdvb_err("VIDEO_FREEZE: ERROR %d, %s\n", errno, strerror(errno));
        }

        if (isBufferedOutput)
            LinuxDvbBuffOpen(context, type, videofd);
    }
    if (audio && audiofd < 0) 
    {
        audiofd = open(AUDIODEV, O_RDWR | O_NONBLOCK);

        if (audiofd < 0)
        {
            linuxdvb_err("failed to open %s - errno %d, %s\n", AUDIODEV, errno, strerror(errno));
            return cERR_LINUXDVB_ERROR;
        }

        if (ioctl( audiofd, AUDIO_CLEAR_BUFFER) == -1)
        {
            linuxdvb_err("AUDIO_CLEAR_BUFFER: ERROR %d, %s\n", errno, strerror(errno));
        }

        if (ioctl( audiofd, AUDIO_SELECT_SOURCE, AUDIO_SOURCE_MEMORY) == -1)
        {
            linuxdvb_err("AUDIO_SELECT_SOURCE: ERROR %d, %s\n", errno, strerror(errno));
        }
        
        if (ioctl( audiofd, AUDIO_PAUSE) == -1)
        {
            linuxdvb_err("AUDIO_PAUSE: ERROR %d, %s\n", errno, strerror(errno));
        }
        
        if (isBufferedOutput)
            LinuxDvbBuffOpen(context, type, audiofd);
    }

    return cERR_LINUXDVB_NO_ERROR;
}

int LinuxDvbClose(Context_t  *context, char *type)
{
    uint8_t video = !strcmp("video", type);
    uint8_t audio = !strcmp("audio", type);

    linuxdvb_printf(10, "v%d a%d\n", video, audio);

    /* closing stand alone is not allowed, so prevent
     * user from closing and don't call stop. stop will
     * set default values for us (speed and so on).
     */
    LinuxDvbStop(context, type);

    getLinuxDVBMutex(FILENAME, __FUNCTION__,__LINE__);
    
    if (isBufferedOutput)
        LinuxDvbBuffClose(context);

    if (video && videofd != -1)
    {
        close(videofd);
        videofd = -1;
    }
    if (audio && audiofd != -1) 
    {
        close(audiofd);
        audiofd = -1;
    }

    releaseLinuxDVBMutex(FILENAME, __FUNCTION__, __LINE__);
    return cERR_LINUXDVB_NO_ERROR;
}

int LinuxDvbPlay(Context_t  *context, char *type) {
    int32_t ret = cERR_LINUXDVB_NO_ERROR;
    Writer_t *writer;

    uint8_t video = !strcmp("video", type);
    uint8_t audio = !strcmp("audio", type);

    linuxdvb_printf(10, "v%d a%d\n", video, audio);

    if (video && videofd != -1) {
        char *Encoding = NULL;
        context->manager->video->Command(context, MANAGER_GETENCODING, &Encoding);

        linuxdvb_printf(10, "V %s\n", Encoding);

        writer = getWriter(Encoding);
        if (writer == NULL)
        {
            linuxdvb_err("cannot found writer for encoding %s using default\n", Encoding);
            ret = cERR_LINUXDVB_ERROR;
        }
        else
        {
            linuxdvb_printf(20, "found writer %s for encoding %s\n", writer->caps->name, Encoding);
            if (ioctl( videofd, VIDEO_SET_STREAMTYPE, (void*) writer->caps->dvbStreamType) == -1)
            {
                linuxdvb_err("VIDEO_SET_STREAMTYPE: ERROR %d, %s\n", errno, strerror(errno));
                ret = cERR_LINUXDVB_ERROR;
            }
        }
        free(Encoding);
        
        if (0 != ioctl(videofd, VIDEO_PLAY))
        {
            linuxdvb_err("VIDEO_PLAY: ERROR %d, %s\n", errno, strerror(errno));
            ret = cERR_LINUXDVB_ERROR;
        }
        
        if (ioctl(videofd, VIDEO_CONTINUE) == -1)
        {
            linuxdvb_err("VIDEO_CONTINUE: ERROR %d, %s\n", errno, strerror(errno));
        }
        
        if (ioctl( videofd, VIDEO_CLEAR_BUFFER) == -1)
        {
            linuxdvb_err("VIDEO_CLEAR_BUFFER: ERROR %d, %s\n", errno, strerror(errno));
        }
    }
    if (audio && audiofd != -1) {
        char * Encoding = NULL;
        context->manager->audio->Command(context, MANAGER_GETENCODING, &Encoding);

        linuxdvb_printf(20, "0 A %s\n", Encoding);

        writer = getWriter(Encoding);
        
        if (writer == NULL)
        {
            linuxdvb_err("cannot found writer for encoding %s using default\n", Encoding);
            ret = cERR_LINUXDVB_ERROR;
        } 
        else
        {
            linuxdvb_printf(20, "found writer %s for encoding %s\n", writer->caps->name, Encoding);
            if (ioctl( audiofd, AUDIO_SET_BYPASS_MODE, (void*) LinuxDvbMapBypassMode(writer->caps->dvbStreamType)) < 0)
            {
                linuxdvb_err("AUDIO_SET_BYPASS_MODE: ERROR %d, %s\n", errno, strerror(errno));
                ret = cERR_LINUXDVB_ERROR;
            }
        }

        if (ioctl(audiofd, AUDIO_PLAY) < 0)
        {
            linuxdvb_err("AUDIO_PLAY: ERROR %d, %s\n", errno, strerror(errno));
            ret = cERR_LINUXDVB_ERROR;
        }
        
        if (ioctl(audiofd, AUDIO_CONTINUE) < 0)
        {
            linuxdvb_err("AUDIO_CONTINUE: ERROR %d, %s\n", errno, strerror(errno));
            ret = cERR_LINUXDVB_ERROR;
        }
        free(Encoding);
    }

    //return ret;
    return 0;
}

int LinuxDvbStop(Context_t  *context __attribute__((unused)), char * type) 
{
    int ret = cERR_LINUXDVB_NO_ERROR;
    unsigned char video = !strcmp("video", type);
    unsigned char audio = !strcmp("audio", type);

    linuxdvb_printf(10, "v%d a%d\n", video, audio);

    getLinuxDVBMutex(FILENAME, __FUNCTION__,__LINE__);

    if (video && videofd != -1) 
    {
        if (ioctl(videofd, VIDEO_CLEAR_BUFFER) == -1)
        {
            linuxdvb_err("VIDEO_CLEAR_BUFFER: ERROR %d, %s\n", errno, strerror(errno));
        }
        
        if (ioctl(videofd, VIDEO_STOP) == -1)
        {
            linuxdvb_err("VIDEO_STOP: ERROR %d, %s\n", errno, strerror(errno));
            ret = cERR_LINUXDVB_ERROR;
        }

        ioctl(videofd, VIDEO_SLOWMOTION, 0);
        ioctl(videofd, VIDEO_FAST_FORWARD, 0);
        ioctl(videofd, VIDEO_SELECT_SOURCE, VIDEO_SOURCE_DEMUX);
    }
    if (audio && audiofd != -1) {
        if (ioctl(audiofd, AUDIO_CLEAR_BUFFER) == -1)
        {
            linuxdvb_err("AUDIO_CLEAR_BUFFER: ERROR %d, %s\n", errno, strerror(errno));
        }

        if (ioctl(audiofd, AUDIO_STOP) == -1)
        {
            linuxdvb_err("AUDIO_STOP: ERROR %d, %s\n", errno, strerror(errno));
            ret = cERR_LINUXDVB_ERROR;
        }
        ioctl(audiofd, AUDIO_SELECT_SOURCE, AUDIO_SOURCE_DEMUX);
    }

    releaseLinuxDVBMutex(FILENAME, __FUNCTION__,__LINE__);

    return ret;
}

int LinuxDvbPause(Context_t  *context __attribute__((unused)), char *type) {
    int32_t ret = cERR_LINUXDVB_NO_ERROR;
    uint8_t video = !strcmp("video", type);
    uint8_t audio = !strcmp("audio", type);

    linuxdvb_printf(10, "v%d a%d\n", video, audio);

    getLinuxDVBMutex(FILENAME, __FUNCTION__,__LINE__);

    if (video && videofd != -1) 
    {
        if (ioctl(videofd, VIDEO_FREEZE, NULL) == -1)
        {
            linuxdvb_err("VIDEO_FREEZE: ERROR %d, %s\n", errno, strerror(errno));
            ret = cERR_LINUXDVB_ERROR;
        }
    }
    
    if (audio && audiofd != -1)
    {
        if (ioctl(audiofd, AUDIO_PAUSE, NULL) == -1)
        {
            linuxdvb_err("AUDIO_PAUSE: ERROR %d, %s\n", errno, strerror(errno));
            ret = cERR_LINUXDVB_ERROR;
        }
    }

    releaseLinuxDVBMutex(FILENAME, __FUNCTION__,__LINE__);

    return ret;
}

int LinuxDvbContinue(Context_t  *context __attribute__((unused)), char * type) {
    int32_t ret = cERR_LINUXDVB_NO_ERROR;
    uint8_t video = !strcmp("video", type);
    uint8_t audio = !strcmp("audio", type);

    linuxdvb_printf(10, "v%d a%d\n", video, audio);

    if (video && videofd != -1)
    {
        if (ioctl(videofd, VIDEO_CONTINUE, NULL) == -1)
        {
            linuxdvb_err("VIDEO_CONTINUE: ERROR %d, %s\n", errno, strerror(errno));
            ret = cERR_LINUXDVB_ERROR;
        }
    }
    
    if (audio && audiofd != -1)
    {
        if (ioctl(audiofd, AUDIO_CONTINUE, NULL) == -1)
        {
            linuxdvb_err("AUDIO_CONTINUE: ERROR %d, %s\n", errno, strerror(errno));
            ret = cERR_LINUXDVB_ERROR;
        }
    }
    
    if (isBufferedOutput)
        LinuxDvbBuffResume(context);

    linuxdvb_printf(10, "exiting\n");
    
    return ret;
}

int LinuxDvbAudioMute(Context_t  *context __attribute__((unused)), char *flag) {
    int ret = cERR_LINUXDVB_NO_ERROR;

    linuxdvb_printf(10, "\n");

    if (audiofd != -1) {
        if(*flag == '1')
        {
            if (ioctl(audiofd, AUDIO_STOP, NULL) == -1)
            {
                linuxdvb_err("AUDIO_STOP: ERROR %d, %s\n", errno, strerror(errno));
                ret = cERR_LINUXDVB_ERROR;
            }
        }
        else
        {
            if (ioctl(audiofd, AUDIO_PLAY) == -1)
            {
                linuxdvb_err("AUDIO_PLAY: ERROR %d, %s\n", errno, strerror(errno));
                ret = cERR_LINUXDVB_ERROR;
            }
        }
    }

    linuxdvb_printf(10, "exiting\n");

    return ret;
}

int LinuxDvbFlush(Context_t  *context __attribute__((unused)), char * type) 
{
    return cERR_LINUXDVB_NO_ERROR;
}

int LinuxDvbSlowMotion(Context_t  *context, char * type) {
    int32_t ret = cERR_LINUXDVB_NO_ERROR;

    uint8_t video = !strcmp("video", type);
    uint8_t audio = !strcmp("audio", type);

    linuxdvb_printf(10, "v%d a%d\n", video, audio);

    if ( (video && videofd != -1) || (audio && audiofd != -1) ) {
        getLinuxDVBMutex(FILENAME, __FUNCTION__,__LINE__);

        if (video && videofd != -1) {
            if (ioctl(videofd, VIDEO_SLOWMOTION, context->playback->SlowMotion) == -1)
            {
                linuxdvb_err("VIDEO_SLOWMOTION: ERROR %d, %s\n", errno, strerror(errno));
                ret = cERR_LINUXDVB_ERROR;
            }
        }

        releaseLinuxDVBMutex(FILENAME, __FUNCTION__,__LINE__);
    }

    linuxdvb_printf(10, "exiting with value %d\n", ret);

    return ret;
}

int LinuxDvbAVSync(Context_t  *context, char *type __attribute__((unused))) {
    int32_t ret = cERR_LINUXDVB_NO_ERROR;
    /* konfetti: this one is dedicated to audiofd so we
     * are ignoring what is given by type! I think we should
     * remove this param. Therefor we should add a variable
     * setOn or something like that instead, this would remove
     * using a variable inside the structure.
     */
    if (audiofd != -1) {
        getLinuxDVBMutex(FILENAME, __FUNCTION__,__LINE__);

        if (ioctl(audiofd, AUDIO_SET_AV_SYNC, 0) == -1) //context->playback->AVSync) == -1)
        {
            linuxdvb_err("AUDIO_SET_AV_SYNC: ERROR %d, %s\n", errno, strerror(errno));
            ret = cERR_LINUXDVB_ERROR;
        }

        releaseLinuxDVBMutex(FILENAME, __FUNCTION__,__LINE__);
    }

    return ret;
}

int LinuxDvbClear(Context_t  *context __attribute__((unused)), char *type) 
{
    int32_t ret = cERR_LINUXDVB_NO_ERROR;
    uint8_t video = !strcmp("video", type);
    uint8_t audio = !strcmp("audio", type);

    linuxdvb_printf(10, "LinuxDvbClear v%d a%d\n", video, audio);

    if ( (video && videofd != -1) || (audio && audiofd != -1) ) 
    {
        getLinuxDVBMutex(FILENAME, __FUNCTION__,__LINE__);

        if (video && videofd != -1) 
        {
            if (ioctl(videofd, VIDEO_CLEAR_BUFFER) == -1)
            {
                linuxdvb_err("VIDEO_CLEAR_BUFFER: ERROR %d, %s\n", errno, strerror(errno));
                ret = cERR_LINUXDVB_ERROR;
            }
        }
        else if (audio && audiofd != -1) 
        {
            if (ioctl(audiofd, AUDIO_CLEAR_BUFFER) == -1)
            {
                linuxdvb_err("AUDIO_CLEAR_BUFFER: ERROR %d, %s\n", errno, strerror(errno));
                ret = cERR_LINUXDVB_ERROR;
            }
        }

        releaseLinuxDVBMutex(FILENAME, __FUNCTION__,__LINE__);
    }

    linuxdvb_printf(10, "exiting\n");

    return ret;
}

int LinuxDvbPts(Context_t  *context __attribute__((unused)), unsigned long long int* pts) {
    int32_t ret = cERR_LINUXDVB_ERROR;
    
    linuxdvb_printf(50, "\n");

    // GET_PTS is immutable call, so it can be done in parallel to other requests
    if (videofd > -1 && !ioctl(videofd, VIDEO_GET_PTS, (void*)&sCURRENT_PTS))
    {
        ret = cERR_LINUXDVB_NO_ERROR;
    }
    else
    {
        linuxdvb_err("VIDEO_GET_PTS: ERROR %d, %s\n", errno, strerror(errno));
    }

    if (ret != cERR_LINUXDVB_NO_ERROR) 
    {
        if (audiofd > -1 && !ioctl(audiofd, AUDIO_GET_PTS, (void*)&sCURRENT_PTS))
        {
            ret = cERR_LINUXDVB_NO_ERROR;
        }
        else
        {
            linuxdvb_err("AUDIO_GET_PTS: ERROR %d, %s\n", errno, strerror(errno));
        }
    }

    if (ret != cERR_LINUXDVB_NO_ERROR)
    {
        sCURRENT_PTS = 0;
    }

    *((unsigned long long int *)pts)=(unsigned long long int)sCURRENT_PTS;
    return ret;
}

int LinuxDvbGetFrameCount(Context_t  *context __attribute__((unused)), unsigned long long int* frameCount)
{
    return cERR_LINUXDVB_NO_ERROR;
}

int LinuxDvbSwitch(Context_t  *context, char *type) 
{
    uint8_t audio = !strcmp("audio", type);
    uint8_t video = !strcmp("video", type);
    Writer_t *writer;

    linuxdvb_printf(10, "v%d a%d\n", video, audio);

    if ( (video && videofd != -1) || (audio && audiofd != -1) ) {
        getLinuxDVBMutex(FILENAME, __FUNCTION__,__LINE__);

        if (audio && audiofd != -1) {
            char * Encoding = NULL;
            if (context && context->manager && context->manager->audio) {
                context->manager->audio->Command(context, MANAGER_GETENCODING, &Encoding);

                linuxdvb_printf(10, "A %s\n", Encoding);

                writer = getWriter(Encoding);

                if (ioctl(audiofd, AUDIO_STOP ,NULL) == -1)
                {
                    linuxdvb_err("AUDIO_STOP: ERROR %d, %s\n", errno, strerror(errno));
                }

                if (ioctl(audiofd, AUDIO_CLEAR_BUFFER) == -1)
                {
                    linuxdvb_err("AUDIO_CLEAR_BUFFER: ERROR %d, %s\n", errno, strerror(errno));
                }

                if (writer == NULL)
                {
                    linuxdvb_err("cannot found writer for encoding %s using default\n", Encoding);
                } 
                else
                {
                    linuxdvb_printf(10, "found writer %s for encoding %s\n", writer->caps->name, Encoding);
                    if (ioctl( audiofd, AUDIO_SET_BYPASS_MODE, (void*) LinuxDvbMapBypassMode(writer->caps->dvbStreamType)) == -1)
                    {
                        linuxdvb_err("AUDIO_SET_BYPASS_MODE: ERROR %d, %s\n", errno, strerror(errno));
                    }
                }

                if (ioctl(audiofd, AUDIO_PLAY) == -1)
                {
                    linuxdvb_err("AUDIO_PLAY: ERROR %d, %s\n", errno, strerror(errno));
                }
                free(Encoding);
            }
            else
                linuxdvb_printf(20, "no context for Audio\n");
        }

        if (video && videofd != -1) {
            char * Encoding = NULL;
            if (context && context->manager && context->manager->video) {
                context->manager->video->Command(context, MANAGER_GETENCODING, &Encoding);

                if (ioctl(videofd, VIDEO_STOP ,NULL) == -1)
                {
                    linuxdvb_err("VIDEO_STOP: ERROR %d, %s\n", errno, strerror(errno));
                }

                if (ioctl(videofd, VIDEO_CLEAR_BUFFER) == -1)
                {
                    linuxdvb_err("VIDEO_CLEAR_BUFFER: ERROR %d, %s\n", errno, strerror(errno));
                }

                linuxdvb_printf(10, "V %s\n", Encoding);

                writer = getWriter(Encoding);

                if (writer == NULL)
                {
                    linuxdvb_err("cannot found writer for encoding %s using default\n", Encoding);
                } 
                else
                {
                    linuxdvb_printf(10, "found writer %s for encoding %s\n", writer->caps->name, Encoding);
                    if (ioctl( videofd, VIDEO_SET_STREAMTYPE, (void*) writer->caps->dvbStreamType) == -1)
                    {
                        linuxdvb_err("VIDEO_SET_STREAMTYPE: ERROR %d, %s\n", errno, strerror(errno));
                    }
                }

                if (ioctl(videofd, VIDEO_PLAY) == -1)
                {
                    /* konfetti: fixme: think on this, I think we should
                     * return an error here and stop the playback mode
                     */
                    linuxdvb_err("VIDEO_PLAY:ERROR %d, %s\n", errno, strerror(errno));
                }
                free(Encoding);
            }
            else
                linuxdvb_printf(20, "no context for Video\n");
        }

        releaseLinuxDVBMutex(FILENAME, __FUNCTION__,__LINE__);

    }

    linuxdvb_printf(10, "exiting\n");

    return cERR_LINUXDVB_NO_ERROR;
}

static int Write(void  *_context, void *_out)
{
    Context_t          *context  = (Context_t  *) _context;
    AudioVideoOut_t    *out      = (AudioVideoOut_t*) _out;
    int32_t            ret       = cERR_LINUXDVB_NO_ERROR;
    int32_t            res       = 0;
    uint8_t            video     = 0;
    uint8_t            audio     = 0;
    Writer_t           *writer   = NULL;
    WriterAVCallData_t call;

    if (out == NULL)
    {
       linuxdvb_err("null pointer passed\n");
       return cERR_LINUXDVB_ERROR;
    }
    
    video = !strcmp("video", out->type);
    audio = !strcmp("audio", out->type);
  
    linuxdvb_printf(20, "DataLength=%u PrivateLength=%u Pts=%llu FrameRate=%f\n", 
                                                    out->len, out->extralen, out->pts, out->frameRate);
    linuxdvb_printf(20, "v%d a%d\n", video, audio);

    if (video) 
    {
        char *Encoding = NULL;
        context->manager->video->Command(context, MANAGER_GETENCODING, &Encoding);

        linuxdvb_printf(20, "Encoding = %s\n", Encoding);

        writer = getWriter(Encoding);

        if (writer == NULL)
        {
            linuxdvb_printf(20, "searching default writer ... %s\n", Encoding);
            writer = getDefaultVideoWriter();
        }

        if (writer == NULL)
        {
            linuxdvb_err("unknown video codec and no default writer %s\n",Encoding);
            ret = cERR_LINUXDVB_ERROR;
        } 
        else
        {
            struct pollfd pfd[1];
            pfd[0].fd = videofd;
            pfd[0].events = POLLPRI;
            int pollret = poll(pfd, 1, 0);
            if (pollret > 0 && pfd[0].revents & POLLPRI)
            {
                struct video_event evt;
                if (ioctl(videofd, VIDEO_GET_EVENT, &evt) == -1)
                {
                    linuxdvb_err("ioctl failed with errno %d\n", errno);
                    linuxdvb_err("VIDEO_GET_EVENT: %s\n", strerror(errno));
                }
                else
                {
                    if (evt.type == VIDEO_EVENT_SIZE_CHANGED)
                    {
                        linuxdvb_printf(10, "VIDEO_EVENT_SIZE_CHANGED\n", evt.type);
                        linuxdvb_printf(10, "width  : %d\n", evt.u.size.w);
                        linuxdvb_printf(10, "height : %d\n", evt.u.size.h);
                        linuxdvb_printf(10, "aspect : %d\n", evt.u.size.aspect_ratio);
                        videoInfo.width = evt.u.size.w;
                        videoInfo.height = evt.u.size.h;
                        videoInfo.aspect_ratio = evt.u.size.aspect_ratio;
                    }
                    else if (evt.type == VIDEO_EVENT_FRAME_RATE_CHANGED)
                    {
                        linuxdvb_printf(10, "VIDEO_EVENT_FRAME_RATE_CHANGED\n", evt.type);
                        linuxdvb_printf(10, "framerate : %d\n", evt.u.frame_rate);
                        videoInfo.frame_rate = evt.u.frame_rate;
                    }
                    else if (evt.type == 16 /*VIDEO_EVENT_PROGRESSIVE_CHANGED*/)
                    {
                        linuxdvb_printf(10, "VIDEO_EVENT_PROGRESSIVE_CHANGED\n", evt.type);
                        linuxdvb_printf(10, "progressive : %d\n", evt.u.frame_rate);
                        videoInfo.progressive = evt.u.frame_rate;
                        context->manager->video->Command(context, MANAGER_UPDATED_TRACK_INFO, NULL);
                    }
                    else
                    {
                        linuxdvb_err("unhandled DVBAPI Video Event %d\n", evt.type);
                    }
                }
            }

            call.fd           = videofd;
            call.data         = out->data;
            call.len          = out->len;
            call.Pts          = out->pts;
            call.Dts          = out->dts;
            call.private_data = out->extradata;
            call.private_size = out->extralen;
            call.FrameRate    = out->frameRate;
            call.FrameScale   = out->timeScale;
            call.Width        = out->width;
            call.Height       = out->height;
            call.InfoFlags    = out->infoFlags;
            call.Version      = 0;
            call.WriteV       = isBufferedOutput ? BufferingWriteV : writev_with_retry;

            if (writer->writeData)
            {
                res = writer->writeData(&call);
            }

            if (res < 0)
            {
                linuxdvb_err("failed to write data %d - %d\n", res, errno);
                linuxdvb_err("%s\n", strerror(errno));
                ret = cERR_LINUXDVB_ERROR;
            }
        }

        free(Encoding);
    } 
    else if (audio)
    {
        char *Encoding = NULL;
        context->manager->audio->Command(context, MANAGER_GETENCODING, &Encoding);

        linuxdvb_printf(20, "%s::%s Encoding = %s\n", FILENAME, __FUNCTION__, Encoding);

        writer = getWriter(Encoding);

        if (writer == NULL)
        {
            linuxdvb_printf(20, "searching default writer ... %s\n", Encoding);
            writer = getDefaultAudioWriter();
        }

        if (writer == NULL)
        {
            linuxdvb_err("unknown audio codec %s and no default writer\n",Encoding);
            ret = cERR_LINUXDVB_ERROR;
        } 
        else
        {
            call.fd             = audiofd;
            call.data           = out->data;
            call.len            = out->len;
            call.Pts            = out->pts;
            call.Dts            = out->dts;
            call.private_data   = out->extradata;
            call.private_size   = out->extralen;
            call.FrameRate      = out->frameRate;
            call.FrameScale     = out->timeScale;
            call.InfoFlags      = out->infoFlags;
            call.Version        = 0;
            call.WriteV         = isBufferedOutput ? BufferingWriteV : writev_with_retry;

            if (writer->writeData)
            {
                res = writer->writeData(&call);
            }

            if (res < 0)
            {
                linuxdvb_err("failed to write data %d - %d\n", res, errno);
                linuxdvb_err("%s\n", strerror(errno));
                ret = cERR_LINUXDVB_ERROR;
            }
        }

        free(Encoding);
    }

    return ret;
}

static int reset(Context_t  *context)
{
    int ret = cERR_LINUXDVB_NO_ERROR;
    Writer_t*   writer;
    char * Encoding = NULL;

    context->manager->video->Command(context, MANAGER_GETENCODING, &Encoding);

    writer = getWriter(Encoding);

    if (writer == NULL)
    {
        linuxdvb_err("unknown video codec %s\n",Encoding);
        ret = cERR_LINUXDVB_ERROR;
    } else
    {
        writer->reset();
    }

    free(Encoding);

    context->manager->audio->Command(context, MANAGER_GETENCODING, &Encoding);

    writer = getWriter(Encoding);

    if (writer == NULL)
    {
        linuxdvb_err("unknown video codec %s\n",Encoding);
        ret = cERR_LINUXDVB_ERROR;
    } else
    {
        writer->reset();
    }

    free(Encoding);

    if (isBufferedOutput)
        LinuxDvbBuffFlush(context);
    
    return ret;
}

static int Command(void  *_context, OutputCmd_t command, void * argument) {
    Context_t* context = (Context_t*) _context;
    int ret = cERR_LINUXDVB_NO_ERROR;
    
    linuxdvb_printf(50, "Command %d\n", command);

    switch(command) {
    case OUTPUT_OPEN: {
        ret = LinuxDvbOpen(context, (char*)argument);
        break;
    }
    case OUTPUT_CLOSE: {
        ret = LinuxDvbClose(context, (char*)argument);
        reset(context);
        sCURRENT_PTS = 0;
        break;
    }
    case OUTPUT_PLAY: {	// 4
        sCURRENT_PTS = 0;
        ret = LinuxDvbPlay(context, (char*)argument);
        break;
    }
    case OUTPUT_STOP: {
        reset(context);
        ret = LinuxDvbStop(context, (char*)argument);
        sCURRENT_PTS = 0;
        break;
    }
    case OUTPUT_FLUSH: {
        ret = LinuxDvbFlush(context, (char*)argument);
        reset(context);
        sCURRENT_PTS = 0;
        break;
    }
    case OUTPUT_PAUSE: {
        ret = LinuxDvbPause(context, (char*)argument);
        break;
    }
    case OUTPUT_CONTINUE: {
        ret = LinuxDvbContinue(context, (char*)argument);
        break;
    }
    case OUTPUT_AVSYNC: {
        ret = LinuxDvbAVSync(context, (char*)argument);
        break;
    }
    case OUTPUT_CLEAR: {
        ret = LinuxDvbClear(context, (char*)argument);
        reset(context);
        sCURRENT_PTS = 0;
        break;
    }
    case OUTPUT_PTS: {
        unsigned long long int pts = 0;
        ret = LinuxDvbPts(context, &pts);
        *((unsigned long long int*)argument) = (unsigned long long int)pts;
        break;
    }
    case OUTPUT_SWITCH: {
        ret = LinuxDvbSwitch(context, (char*)argument);
        break;
    }
    case OUTPUT_SLOWMOTION: {
        return LinuxDvbSlowMotion(context, (char*)argument);
        break;
    }
    case OUTPUT_AUDIOMUTE: {
        return LinuxDvbAudioMute(context, (char*)argument);
        break;
    }
    case OUTPUT_GET_FRAME_COUNT: {
        unsigned long long int frameCount = 0;
        ret = LinuxDvbGetFrameCount(context, &frameCount);
        *((unsigned long long int*)argument) = (unsigned long long int)frameCount;
        break;
    }
    case OUTPUT_GET_PROGRESSIVE: {
        ret = cERR_LINUXDVB_NO_ERROR;
        *((int*)argument) = videoInfo.progressive;
        break;
    }
    case OUTPUT_SET_BUFFER_SIZE: {
        ret = cERR_LINUXDVB_ERROR;
        if (!isBufferedOutput)
        {
            uint32_t bufferSize = *((uint32_t*)argument);
            ret = cERR_LINUXDVB_NO_ERROR;
            if (bufferSize > 0)
            {
                LinuxDvbBuffSetSize(bufferSize);
                isBufferedOutput = true;
            }
        }
        break;
    }
    case OUTPUT_GET_BUFFER_SIZE: {
        ret = cERR_LINUXDVB_NO_ERROR;
        *((uint32_t*)argument) = LinuxDvbBuffGetSize();
        break;
    }
    default:
        linuxdvb_err("ContainerCmd %d not supported!\n", command);
        ret = cERR_LINUXDVB_ERROR;
        break;
    }

    linuxdvb_printf(50, "exiting with value %d\n", ret);

    return ret;
}

static char *LinuxDvbCapabilities[] = { "audio", "video", NULL };

struct Output_s LinuxDvbOutput = {
    "LinuxDvb",
    &Command,
    &Write,
    LinuxDvbCapabilities
};
