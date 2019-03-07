/*
 * RAM write buffering utilities
 * samsamsam 2018
 *
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
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <memory.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <time.h>

#include "common.h"
#include "debug.h"
#include "misc.h"
#include "writer.h"

/* ***************************** */
/* Types                         */
/* ***************************** */
typedef enum OutputType_e{
    OUTPUT_UNK,
    OUTPUT_AUDIO,
    OUTPUT_VIDEO,
} OutputType_t;

typedef struct BufferingNode_s {
    uint32_t dataSize;
    OutputType_t dataType;
    void *stamp;
    struct BufferingNode_s *next;
} BufferingNode_t;

/* ***************************** */
/* Makros/Constants              */
/* ***************************** */
#define cERR_LINUX_DVB_BUFFERING_NO_ERROR      0
#define cERR_LINUX_DVB_BUFFERING_ERROR        -1

/* ***************************** */
/* Varaibles                     */
/* ***************************** */
static pthread_t bufferingThread;
static pthread_mutex_t bufferingMtx;
static pthread_cond_t  bufferingExitCond;
static pthread_cond_t  bufferingDataConsumedCond;
static pthread_cond_t  bufferingWriteFinishedCond;
static pthread_cond_t  bufferingdDataAddedCond;
static bool hasBufferingThreadStarted = false;
static BufferingNode_t *bufferingQueueHead = NULL;
static BufferingNode_t *bufferingQueueTail = NULL;

static uint32_t maxBufferingDataSize = 0;
static uint32_t bufferingDataSize = 0;

static int videofd = -1;
static int audiofd = -1;
static int g_pfd[2] = {-1, -1};

static pthread_mutex_t *g_pDVBMtx = NULL;

static bool g_bDuringWrite = false;
static bool g_bSignalWriteFinish = false;

static void *g_pWriteStamp = NULL;

/* ***************************** */
/* Prototypes                    */
/* ***************************** */

/* ***************************** */
/* MISC Functions                */
/* ***************************** */
static void WriteWakeUp()
{
    int ret = write(g_pfd[1], "x", 1);
    if (ret != 1) {
        buff_printf(20, "WriteWakeUp write return %d\n", ret);
    }
}

/* **************************** */
/* Worker Thread                */
/* **************************** */
static void LinuxDvbBuffThread(Context_t *context) 
{
    int flags = 0;
    static BufferingNode_t *nodePtr = NULL;
    buff_printf(20, "ENTER\n");

    if (pipe(g_pfd) == -1)
        buff_err("critical error\n");
    
    /* Make read and write ends of pipe nonblocking */
    if ((flags = fcntl(g_pfd[0], F_GETFL)) == -1)
        buff_err("critical error\n");
    
    /* Make read end nonblocking */
    flags |= O_NONBLOCK;
    if (fcntl(g_pfd[0], F_SETFL, flags) == -1)
        buff_err("critical error\n");
    
    if ((flags = fcntl(g_pfd[1], F_GETFL)) == -1)
        buff_err("critical error\n");
    
    /* Make write end nonblocking */
    flags |= O_NONBLOCK;
    if (fcntl(g_pfd[1], F_SETFL, flags) == -1)
        buff_err("critical error\n");

    PlaybackDieNowRegisterCallback(WriteWakeUp);
    
    while (0 == PlaybackDieNow(0))
    {
        pthread_mutex_lock(&bufferingMtx);
        g_bDuringWrite = false;
        if (g_bSignalWriteFinish)
        {
            pthread_cond_signal(&bufferingWriteFinishedCond);
            g_bSignalWriteFinish = false;
        }

        if (nodePtr)
        {
            free(nodePtr);
            nodePtr = NULL;
            /* signal that we free some space in queue */
            pthread_cond_signal(&bufferingDataConsumedCond);
        }
        
        if (!bufferingQueueHead)
        {
            assert(bufferingQueueTail == NULL);
            
            /* Queue is empty we need to wait for data to be added */
            pthread_cond_wait(&bufferingdDataAddedCond, &bufferingMtx);
            pthread_mutex_unlock(&bufferingMtx);
            continue; /* To check PlaybackDieNow(0) */
        }
        else
        {
            nodePtr = bufferingQueueHead;
            bufferingQueueHead = bufferingQueueHead->next;
            if (bufferingQueueHead == NULL)
            {
                bufferingQueueTail = NULL;
            }
            
            if (bufferingDataSize >= (nodePtr->dataSize + sizeof(BufferingNode_t)))
            {
                bufferingDataSize -= (nodePtr->dataSize + sizeof(BufferingNode_t));
            }
            else
            {
                assert(bufferingDataSize == 0);
                bufferingDataSize = 0;
            }
        }

        /* We will write data without mutex
         * this have some disadvantage because we can 
         * write some portion of data after LinuxDvbBuffFlush,
         * for example after seek.
         */
        if (nodePtr && !context->playback->isSeeking && context->playback->stamp == nodePtr->stamp)
        {
            /* Write data to valid output */
            uint8_t *dataPtr = (uint8_t *)nodePtr + sizeof(BufferingNode_t);
            int fd = nodePtr->dataType == OUTPUT_VIDEO ? videofd : audiofd;
            g_bDuringWrite = true;
            pthread_mutex_unlock(&bufferingMtx);
            if (0 != WriteWithRetry(context, g_pfd[0], fd, g_pDVBMtx, dataPtr, nodePtr->dataSize))
            {
                buff_err("Something is WRONG\n");
            }
        }
        else
        {
            pthread_mutex_unlock(&bufferingMtx);
        }
    }
    
    pthread_mutex_lock(&bufferingMtx);
    pthread_cond_signal(&bufferingExitCond);
    pthread_mutex_unlock(&bufferingMtx);
    
    buff_printf(20, "EXIT\n");
    hasBufferingThreadStarted = false;
    
    close(g_pfd[0]);
    close(g_pfd[1]);
    g_pfd[0] = -1;
    g_pfd[1] = -1;
}

int32_t LinuxDvbBuffSetSize(const uint32_t bufferSize)
{
    maxBufferingDataSize = bufferSize;
    return cERR_LINUX_DVB_BUFFERING_NO_ERROR;
}

uint32_t LinuxDvbBuffGetSize()
{
    return maxBufferingDataSize;
}

int32_t LinuxDvbBuffOpen(Context_t *context, char *type, int outfd, void *mtx)
{
    int32_t error = 0;
    int32_t ret = cERR_LINUX_DVB_BUFFERING_NO_ERROR;
    
    buff_printf(10, "\n");

    if (!hasBufferingThreadStarted) 
    {
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

        g_pDVBMtx = mtx;

        if((error = pthread_create(&bufferingThread, &attr, (void *)&LinuxDvbBuffThread, context)) != 0) 
        {
            buff_printf(10, "Creating thread, error:%d:%s\n", error, strerror(error));

            hasBufferingThreadStarted = false;
            ret = cERR_LINUX_DVB_BUFFERING_ERROR;
        }
        else 
        {
            buff_printf(10, "Created thread\n");
            hasBufferingThreadStarted = true;

            /* init synchronization prymitives */
            pthread_mutex_init(&bufferingMtx, NULL);
            
            pthread_cond_init(&bufferingExitCond, NULL);
            pthread_cond_init(&bufferingDataConsumedCond, NULL);
            pthread_cond_init(&bufferingWriteFinishedCond, NULL);
            pthread_cond_init(&bufferingdDataAddedCond, NULL);
        }
    }
    
    if (!ret)
    {
        if (!strcmp("video", type) && -1 == videofd)
        {
            videofd = outfd;
        }
        else if (!strcmp("audio", type) && -1 == audiofd)
        {
            audiofd = outfd;
        }
        else
        {
            ret = cERR_LINUX_DVB_BUFFERING_ERROR;
        }
    }

    buff_printf(10, "exiting with value %d\n", ret);
    return ret;
}

int32_t LinuxDvbBuffClose(Context_t *context)
{
    int32_t ret = 0;
    
    buff_printf(10, "\n");
    videofd = -1;
    audiofd = -1;
    
    if (hasBufferingThreadStarted) 
    {
        struct timespec max_wait = {0, 0};
        
        /* WakeUp if we are waiting in the write */ 
        WriteWakeUp();
        
        pthread_mutex_lock(&bufferingMtx);
        /* wake up if thread is waiting for data */ 
        pthread_cond_signal(&bufferingdDataAddedCond);
        
        /* wait for thread end */
#if 0
        /* This code couse symbol versioning of clock_gettime@GLIBC_2.17 */
        clock_gettime(CLOCK_REALTIME, &max_wait);
        max_wait.tv_sec += 1;
#else
        max_wait.tv_sec = time(NULL) + 2;
#endif
        pthread_cond_timedwait(&bufferingExitCond, &bufferingMtx, &max_wait);
        pthread_mutex_unlock(&bufferingMtx);

        if (!hasBufferingThreadStarted)
        {
            /* destroy synchronization prymitives?
             * for a moment, we'll exit linux process, 
             * so the system will do this for us
             */
            /*
            pthread_mutex_destroy(&bufferingMtx);
            pthread_cond_destroy(&bufferingDataConsumedCond);
            pthread_cond_destroy(&bufferingWriteFinishedCond);
            pthread_cond_destroy(&bufferingdDataAddedCond);
            */
        }
    }
    
    ret = hasBufferingThreadStarted ? cERR_LINUX_DVB_BUFFERING_ERROR : cERR_LINUX_DVB_BUFFERING_NO_ERROR;

    buff_printf(10, "exiting with value %d\n", ret);
    return ret;
}

int32_t LinuxDvbBuffFlush(Context_t *context)
{
    static BufferingNode_t *nodePtr = NULL;
    buff_printf(40, "ENTER bufferingQueueHead[%p]\n", bufferingQueueHead);
    
    /* signal if we are waiting for write to DVB decoders */
    WriteWakeUp();

    pthread_mutex_lock(&bufferingMtx);
    while (bufferingQueueHead)
    {
       nodePtr = bufferingQueueHead;
       bufferingQueueHead = nodePtr->next;
       bufferingDataSize -= (nodePtr->dataSize + sizeof(BufferingNode_t));
       free(nodePtr);
    }
    bufferingQueueHead = NULL;
    bufferingQueueTail = NULL;
    buff_printf(40, "bufferingDataSize [%u]\n", bufferingDataSize);
    assert(bufferingDataSize == 0);
    bufferingDataSize = 0;

    /* signal that queue is empty */
    pthread_cond_signal(&bufferingDataConsumedCond);

    while (g_bDuringWrite && !PlaybackDieNow(0)) 
    {
        g_bSignalWriteFinish = true;
        pthread_cond_wait(&bufferingWriteFinishedCond, &bufferingMtx);
    }

    pthread_mutex_unlock(&bufferingMtx);
    buff_printf(40, "EXIT\n");
    
    return 0;
}

int32_t LinuxDvbBuffResume(Context_t *context)
{
    /* signal if we are waiting for write to DVB decoders 
     * 
     */
    WriteWakeUp();
    
    return 0;
}

void LinuxDvbBuffSetStamp(void *stamp)
{
    g_pWriteStamp = stamp;
}

ssize_t BufferingWriteV(int fd, const struct iovec *iov, int ic) 
{
    OutputType_t dataType = OUTPUT_UNK;
    BufferingNode_t *nodePtr = NULL;
    uint8_t *dataPtr = NULL;
    uint32_t chunkSize = 0;
    uint32_t i = 0;
    
    buff_printf(60, "ENTER\n");
    if (fd == videofd)
    {
        buff_printf(60, "VIDEO\n");
        dataType = OUTPUT_VIDEO;
    }
    else if (fd == audiofd)
    {
        buff_printf(60, "AUDIO\n");
        dataType = OUTPUT_AUDIO;
    }
    else
    {
        buff_err("Unknown output type\n");
        return cERR_LINUX_DVB_BUFFERING_ERROR;
    }
    
    for (i=0; i<ic; ++i)
    {
        chunkSize += iov[i].iov_len;
    }
    chunkSize += sizeof(BufferingNode_t);
    
    /* Allocate memory for queue node + data */
    nodePtr = malloc(chunkSize);
    if (!nodePtr)
    {
        buff_err("OUT OF MEM\n");
        return cERR_LINUX_DVB_BUFFERING_ERROR;
    }
    
    /* Copy data to new buffer */
    dataPtr = (uint8_t *)nodePtr + sizeof(BufferingNode_t);
    for (i=0; i<ic; ++i)
    {
        memcpy(dataPtr, iov[i].iov_base, iov[i].iov_len);
        dataPtr += iov[i].iov_len;
    }

    pthread_mutex_lock(&bufferingMtx);
    while (0 == PlaybackDieNow(0))
    {
        if (bufferingDataSize + chunkSize >= maxBufferingDataSize)
        {
            /* Buffering queue is full we need wait for space*/
            pthread_cond_wait(&bufferingDataConsumedCond, &bufferingMtx);
        }
        else
        {
            /* Add chunk to buffering queue */
            if (bufferingQueueHead == NULL)
            {
                bufferingQueueHead = nodePtr;
                bufferingQueueTail = nodePtr;
            }
            else
            {
                bufferingQueueTail->next = nodePtr;
                bufferingQueueTail = nodePtr;
            }
            
            bufferingDataSize += chunkSize;
            chunkSize -= sizeof(BufferingNode_t);
            nodePtr->dataSize = chunkSize;
            nodePtr->dataType = dataType;
            nodePtr->stamp = g_pWriteStamp;
            nodePtr->next = NULL;
            
            /* signal that we added some data to queue */
            pthread_cond_signal(&bufferingdDataAddedCond);
            break;
        }
    }
    pthread_mutex_unlock(&bufferingMtx);
    buff_printf(60, "EXIT\n");
    return chunkSize;
}
