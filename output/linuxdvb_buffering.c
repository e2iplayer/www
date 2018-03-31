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
#include <memory.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

#include "common.h"
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
    struct BufferingNode_s *next;
} BufferingNode_t;

/* ***************************** */
/* Makros/Constants              */
/* ***************************** */
#define cERR_LINUX_DVB_BUFFERING_NO_ERROR      0
#define cERR_LINUX_DVB_BUFFERING_ERROR        -1

//#define SAM_WITH_DEBUG
#ifdef SAM_WITH_DEBUG
#define LINUX_DVB_BUFFERING_DEBUG
#else
#define LINUX_DVB_BUFFERING_SILENT
#endif

#ifdef LINUX_DVB_BUFFERING_DEBUG

static const uint16_t debug_level = 40;

#define buff_printf(level, fmt, x...) do { \
if (debug_level >= level) printf("[%s:%d:%s] " fmt, __FILE__, __LINE__, __FUNCTION__, ## x); } while (0)
#else
#define buff_printf(level, fmt, x...)
#endif

#ifndef LINUX_DVB_BUFFERING_SILENT
#define buff_err(fmt, x...) do { printf("[%s:%d:%s] " fmt, __FILE__, __LINE__, __FUNCTION__, ## x); } while (0)
#else
#define buff_err(fmt, x...)
#endif

/* ***************************** */
/* Varaibles                     */
/* ***************************** */
static pthread_t bufferingThread;
static pthread_mutex_t bufferingMtx;
static pthread_cond_t  bufferingExitCond;
static pthread_cond_t  bufferingDataConsumedCond;
static pthread_cond_t  bufferingdDataAddedCond;
static bool hasBufferingThreadStarted = false;
static BufferingNode_t *bufferingQueueHead = NULL;
static BufferingNode_t *bufferingQueueTail = NULL;

static uint32_t maxBufferingDataSize = 0;
static uint32_t bufferingDataSize = 0;

static int videofd = -1;
static int audiofd = -1;

/* ***************************** */
/* Prototypes                    */
/* ***************************** */

/* ***************************** */
/* MISC Functions                */
/* ***************************** */

/* **************************** */
/* Worker Thread                */
/* **************************** */
static void LinuxDvbBuffThread(Context_t *context) 
{
    static BufferingNode_t *nodePtr = NULL;
    buff_printf(20, "ENTER\n");
    while (0 == PlaybackDieNow(0))
    {
        pthread_mutex_lock(&bufferingMtx);
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
        pthread_mutex_unlock(&bufferingMtx);
        
        /* We will write data without mutex
         * this have some disadvantage because we can 
         * write some portion of data after LinuxDvbBuffFlush,
         * for example after seek, this will be fixed later 
         */
        if (nodePtr && !context->playback->isSeeking)
        {
            /* Write data to valid output */
            uint8_t *dataPtr = (uint8_t *)nodePtr + sizeof(BufferingNode_t);
            int fd = nodePtr->dataType == OUTPUT_VIDEO ? videofd : audiofd;
            if (0 != write_with_retry(fd, dataPtr, nodePtr->dataSize))
            {
                printf("Something is WRONG\n");
            }
        }
    }
    
    pthread_mutex_lock(&bufferingMtx);
    pthread_cond_signal(&bufferingExitCond);
    pthread_mutex_unlock(&bufferingMtx);
    
    buff_printf(20, "EXIT\n");
    hasBufferingThreadStarted = false;
}

int32_t WriteSetBufferingSize(const uint32_t bufferSize)
{
    maxBufferingDataSize = bufferSize;
    return cERR_LINUX_DVB_BUFFERING_NO_ERROR;
}

int32_t LinuxDvbBuffOpen(Context_t *context, char *type, int outfd)
{
    int32_t error = 0;
    int32_t ret = cERR_LINUX_DVB_BUFFERING_NO_ERROR;
    
    buff_printf(10, "\n");

    if (!hasBufferingThreadStarted) 
    {
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

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
        pthread_mutex_lock(&bufferingMtx);
        /* wait for thread end */
        clock_gettime(CLOCK_REALTIME, &max_wait);
        max_wait.tv_sec += 1;
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
    pthread_mutex_unlock(&bufferingMtx);
    buff_printf(40, "EXIT\n");
}

ssize_t BufferingWriteV(int fd, const struct iovec *iov, size_t ic) 
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
