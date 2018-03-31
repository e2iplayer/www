/*
 * linuxdvb output/writer handling.
 *
 * konfetti 2010 based on linuxdvb.c code from libeplayer2
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
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/uio.h>
#include <linux/dvb/video.h>
#include <linux/dvb/audio.h>
#include <memory.h>
#include <asm/types.h>
#include <pthread.h>
#include <errno.h>

#include "stm_ioctls.h"
#include "bcm_ioctls.h"

#include "common.h"
#include "output.h"
#include "debug.h"
#include "misc.h"
#include "pes.h"
#include "writer.h"

/* ***************************** */
/* Makros/Constants              */
/* ***************************** */

#ifdef SAM_WITH_DEBUG
#define WMA_DEBUG
#else
#define WMA_SILENT
#endif

#ifdef WMA_DEBUG

static short debug_level = 0;

#define wma_printf(level, fmt, x...) do { \
if (debug_level >= level) printf("[%s:%s] " fmt, __FILE__, __FUNCTION__, ## x); } while (0)
#else
#define wma_printf(level, fmt, x...)
#endif

#ifndef WMA_SILENT
#define wma_err(fmt, x...) do { printf("[%s:%s] " fmt, __FILE__, __FUNCTION__, ## x); } while (0)
#else
#define wma_err(fmt, x...)
#endif

/* ***************************** */
/* Types                         */
/* ***************************** */

/* ***************************** */
/* Varaibles                     */
/* ***************************** */

static int initialHeader = 1;
static uint8_t *PesHeader = NULL;
static uint32_t MaxPesHeader = 0;

/* ***************************** */
/* Prototypes                    */
/* ***************************** */

/* ***************************** */
/* MISC Functions                */
/* ***************************** */

static int reset()
{
    initialHeader = 1;
    return 0;
}

static int writeData(void* _call)
{
    WriterAVCallData_t* call = (WriterAVCallData_t*) _call;

    int len = 0;

    wma_printf(10, "\n");

    if (call == NULL)
    {
        wma_err("call data is NULL...\n");
        return 0;
    }

    wma_printf(10, "AudioPts %lld\n", call->Pts);

    if ((call->data == NULL) || (call->len <= 0))
    {
        wma_err("parsing NULL Data. ignoring...\n");
        return 0;
    }

    if (call->fd < 0)
    {
        wma_err("file pointer < 0. ignoring ...\n");
        return 0;
    }

    uint32_t packetLength = 4 + call->private_size + call->len;
    
    if( IsDreambox() )
    {
        packetLength += 4;
    }
    
    if((packetLength + PES_MAX_HEADER_SIZE)  > MaxPesHeader)
    {
        if(PesHeader)
        {
            free(PesHeader);
        }
        MaxPesHeader = packetLength + PES_MAX_HEADER_SIZE;
        PesHeader = malloc(MaxPesHeader);
    }

    uint32_t headerSize = InsertPesHeader(PesHeader, packetLength, MPEG_AUDIO_PES_START_CODE, call->Pts, 0);
    if( IsDreambox() )
    {
        PesHeader[headerSize++] = 0x42; // B
        PesHeader[headerSize++] = 0x43; // C
        PesHeader[headerSize++] = 0x4D; // M
        PesHeader[headerSize++] = 0x41; // A
    }

    size_t payload_len = call->len;
    PesHeader[headerSize++] = (payload_len >> 24) & 0xff;
    PesHeader[headerSize++] = (payload_len >> 16) & 0xff;
    PesHeader[headerSize++] = (payload_len >> 8)  & 0xff;
    PesHeader[headerSize++] = payload_len & 0xff;
        
    memcpy(PesHeader + headerSize, call->private_data, call->private_size);
    headerSize += call->private_size;
    
    PesHeader[6] |= 1;

    struct iovec iov[2];
    iov[0].iov_base = PesHeader;
    iov[0].iov_len  = headerSize;
    iov[1].iov_base = call->data;
    iov[1].iov_len  = call->len;

    return call->WriteV(call->fd, iov, 2);
}

/* ***************************** */
/* Writer Definition            */
/* ***************************** */

static WriterCaps_t capsWMAPRO = {
    "wma/pro",
    eAudio,
    "A_WMA/PRO",
    AUDIO_ENCODING_WMA,
    AUDIOTYPE_WMA_PRO,
    -1
};

struct Writer_s WriterAudioWMAPRO = {
    &reset,
    &writeData,
    NULL,
    &capsWMAPRO
};


static WriterCaps_t capsWMA = {
    "wma",
    eAudio,
    "A_WMA",
    AUDIO_ENCODING_WMA,
    AUDIOTYPE_WMA,
    -1
};

struct Writer_s WriterAudioWMA = {
    &reset,
    &writeData,
    NULL,
    &capsWMA
};
