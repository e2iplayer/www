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
#include <inttypes.h>
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

#include "common.h"
#include "debug.h"
#include "output.h"
#include "writer.h"
#include "misc.h"
#include "pes.h"

/* ***************************** */
/* Makros/Constants              */
/* ***************************** */

#define cERR_LINUXDVB_NO_ERROR      0
#define cERR_LINUXDVB_ERROR        -1

static const char VIDEODEV[] = "/tmp/e2i_video0";
static const char AUDIODEV[] = "/tmp/e2i_audio0";

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

static int64_t last_pts = 0;

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

#define getLinuxDVBMutex() pthread_mutex_lock(&LinuxDVBmutex)
#define releaseLinuxDVBMutex() pthread_mutex_unlock(&LinuxDVBmutex)


int LinuxDvbOpen(Context_t  *context __attribute__((unused)), char *type)
{
    uint8_t video = !strcmp("video", type);
    uint8_t audio = !strcmp("audio", type);

    linuxdvb_printf(10, "v%d a%d\n", video, audio);

    if (video && videofd < 0) 
    {
        videofd = open(VIDEODEV, O_CREAT | O_TRUNC | O_WRONLY | S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH, 0666);
    }

    if (audio && audiofd < 0) 
    {
        audiofd = open(AUDIODEV, O_CREAT | O_TRUNC | O_WRONLY | S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH, 0666);
    }

    return 0;
}

int LinuxDvbClose(Context_t  *context, char *type)
{
    return 0;
}

int LinuxDvbPlay(Context_t  *context, char *type)
{
    return 0;
}

int LinuxDvbStop(Context_t  *context __attribute__((unused)), char * type) 
{
    return 0;
}

int LinuxDvbPause(Context_t  *context __attribute__((unused)), char *type)
{
    return 0;
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

int LinuxDvbAudioMute(Context_t  *context __attribute__((unused)), char *flag)
{
    return 0;
}

int LinuxDvbFlush(Context_t  *context __attribute__((unused)), char * type) 
{
    return 0;
}

int LinuxDvbSlowMotion(Context_t  *context, char * type)
{
    return 0;
}

int LinuxDvbAVSync(Context_t  *context, char *type __attribute__((unused)))
{
    return 0;
}

int LinuxDvbClear(Context_t  *context __attribute__((unused)), char *type) 
{
    return 0;
}

int LinuxDvbPts(Context_t  *context __attribute__((unused)), unsigned long long int* pts)
{
    *((unsigned long long int *)pts)=(unsigned long long int)last_pts;
    return 0;
}

int LinuxDvbGetFrameCount(Context_t  *context __attribute__((unused)), unsigned long long int* frameCount)
{
    return cERR_LINUXDVB_NO_ERROR;
}

int LinuxDvbSwitch(Context_t  *context, char *type) 
{

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
  
    linuxdvb_printf(20, "DataLength=%u PrivateLength=%u Pts=%"PRIu64" FrameRate=%d\n", 
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
                        linuxdvb_printf(10, "VIDEO_EVENT_SIZE_CHANGED type: 0x%x\n", evt.type);
                        linuxdvb_printf(10, "width  : %d\n", evt.u.size.w);
                        linuxdvb_printf(10, "height : %d\n", evt.u.size.h);
                        linuxdvb_printf(10, "aspect : %d\n", evt.u.size.aspect_ratio);
                        videoInfo.width = evt.u.size.w;
                        videoInfo.height = evt.u.size.h;
                        videoInfo.aspect_ratio = evt.u.size.aspect_ratio;
                    }
                    else if (evt.type == VIDEO_EVENT_FRAME_RATE_CHANGED)
                    {
                        linuxdvb_printf(10, "VIDEO_EVENT_FRAME_RATE_CHANGED type: 0x%x\n", evt.type);
                        linuxdvb_printf(10, "framerate : %d\n", evt.u.frame_rate);
                        videoInfo.frame_rate = evt.u.frame_rate;
                    }
                    else if (evt.type == 16 /*VIDEO_EVENT_PROGRESSIVE_CHANGED*/)
                    {
                        linuxdvb_printf(10, "VIDEO_EVENT_PROGRESSIVE_CHANGED type: 0x%x\n", evt.type);
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

            if (out->pts != INVALID_PTS_VALUE)
            {
                if (out->pts > last_pts)
                {
                    usleep((out->pts - last_pts) / 90 * 900);
                    //usleep((out->pts - last_pts) / 90 * 500);
                }
                last_pts = out->pts;
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

        linuxdvb_printf(20, "Encoding = %s\n", Encoding);

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
    return 0;
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
