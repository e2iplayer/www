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

#include <libavcodec/avcodec.h>

#include "stm_ioctls.h"
#include "bcm_ioctls.h"

#include "common.h"
#include "output.h"
#include "debug.h"
#include "misc.h"
#include "pes.h"
#include "writer.h"
#include "pcm.h"

/* ***************************** */
/* Makros/Constants              */
/* ***************************** */

/* ***************************** */
/* Types                         */
/* ***************************** */

/* ***************************** */
/* Varaibles                     */
/* ***************************** */
static uint8_t initialHeader = 1;
static uint8_t codec_data[18];
static uint64_t fixed_buffertimestamp;
static uint64_t fixed_bufferduration;
static uint32_t fixed_buffersize;
static uint8_t *fixed_buffer;
static uint32_t fixed_bufferfilled;

/* ***************************** */
/* Prototypes                    */
/* ***************************** */

/* ***************************** */
/* MISC Functions                */
/* ***************************** */

static int32_t reset()
{
    initialHeader = 1;
    return 0;
}

static int32_t writeData(void *_call)
{
    WriterAVCallData_t* call = (WriterAVCallData_t*) _call;

    pcm_printf(10, "\n");

    if (!call)
    {
        pcm_err("call data is NULL...\n");
        return 0;
    }

    pcm_printf(10, "AudioPts %lld\n", call->Pts);

    if (!call->data || (call->len <= 0))
    {
        pcm_err("parsing NULL Data. ignoring...\n");
        return 0;
    }

    if (call->fd < 0)
    {
        pcm_err("file pointer < 0. ignoring ...\n");
        return 0;
    }

    static uint8_t  PesHeader[PES_MAX_HEADER_SIZE + 22];
    pcmPrivateData_t *pcmPrivateData  = (pcmPrivateData_t*)call->private_data;
    uint8_t *buffer = call->data;
    uint32_t   size = call->len;
    
    if( pcmPrivateData->bResampling || NULL == fixed_buffer )
    {
        int32_t width = 0;
        int32_t depth = 0;
        int32_t rate = (uint64_t)pcmPrivateData->sample_rate;
        int32_t channels = (uint8_t) pcmPrivateData->channels;
        int32_t block_align = 0;
        int32_t byterate = 0;

        uint32_t codecID   = (uint32_t)pcmPrivateData->codec_id;
        
        uint8_t dataPrecision = 0;
        uint8_t LE = 0;
        switch (codecID)
        {
            case AV_CODEC_ID_PCM_S8:
            case AV_CODEC_ID_PCM_U8:
                width = depth = 8;
                break;
            case AV_CODEC_ID_PCM_S16LE:
            case AV_CODEC_ID_PCM_U16LE:
                LE = 1;
            case AV_CODEC_ID_PCM_S16BE:
            case AV_CODEC_ID_PCM_U16BE:
                width = depth = 16;
                break;
            case AV_CODEC_ID_PCM_S24LE:
            case AV_CODEC_ID_PCM_U24LE:
                LE = 1;
            case AV_CODEC_ID_PCM_S24BE:
            case AV_CODEC_ID_PCM_U24BE:
                width = depth = 24;
                break;
            case AV_CODEC_ID_PCM_S32LE:
            case AV_CODEC_ID_PCM_U32LE:
                LE = 1;
            case AV_CODEC_ID_PCM_S32BE:
            case AV_CODEC_ID_PCM_U32BE:
                width = depth = 32;
                break;
            default:
                break;
        }
        
        uint8_t *data = codec_data;
        uint16_t format = LE ? 0x0001 : 0x0100;
        
        byterate = channels * rate * width / 8;
        block_align = channels * width / 8;
        memset(data, 0, sizeof(codec_data));
        /* format tag */
        *(data++) = format & 0xff;
        *(data++) = (format >> 8) & 0xff;
        /* channels */
        *(data++) = channels & 0xff;
        *(data++) = (channels >> 8) & 0xff;
        /* sample rate */
        *(data++) = rate & 0xff;
        *(data++) = (rate >> 8) & 0xff;
        *(data++) = (rate >> 16) & 0xff;
        *(data++) = (rate >> 24) & 0xff;
        /* byte rate */
        *(data++) = byterate & 0xff;
        *(data++) = (byterate >> 8) & 0xff;
        *(data++) = (byterate >> 16) & 0xff;
        *(data++) = (byterate >> 24) & 0xff;
        /* block align */
        *(data++) = block_align & 0xff;
        *(data++) = (block_align >> 8) & 0xff;
        /* word size */
        *(data++) = depth & 0xff;
        *(data++) = (depth >> 8) & 0xff;
        
        uint32_t nfixed_buffersize = rate * 30 / 1000;
        nfixed_buffersize *= channels * depth / 8;
        fixed_buffertimestamp = call->Pts;
        fixed_bufferduration = 90000 * nfixed_buffersize /  byterate;
        
        if(fixed_buffersize != nfixed_buffersize || NULL == fixed_buffer)
        {
            fixed_buffersize = nfixed_buffersize;
            if(NULL != fixed_buffer)
            {
                free(fixed_buffer);
            }
            fixed_buffer = malloc(fixed_buffersize);
        }
        fixed_bufferfilled = 0;
        pcm_printf(40, "PCM fixed_buffersize [%u] [%s]\n", fixed_buffersize, LE ? "LE":"BE");
    }
    
    while (size > 0)
    {        
        uint32_t cpSize = (fixed_buffersize - fixed_bufferfilled);
        if(cpSize > size)
        {
            memcpy(fixed_buffer + fixed_bufferfilled, buffer, size);
            fixed_bufferfilled += size;
            return size;
        }
        
        memcpy(fixed_buffer + fixed_bufferfilled, buffer, cpSize);
        fixed_bufferfilled = 0;
        buffer += cpSize;
        size -= cpSize;
        
        uint32_t addHeaderSize = 0;
        if( STB_DREAMBOX == GetSTBType() )
        {
            addHeaderSize = 4;
        }
        uint32_t headerSize = InsertPesHeader(PesHeader, fixed_buffersize + 4 + addHeaderSize + sizeof(codec_data), MPEG_AUDIO_PES_START_CODE, fixed_buffertimestamp, 0);
        if( STB_DREAMBOX == GetSTBType() )
        {
            PesHeader[headerSize++] = 0x42; // B
            PesHeader[headerSize++] = 0x43; // C
            PesHeader[headerSize++] = 0x4D; // M
            PesHeader[headerSize++] = 0x41; // A
        }

        PesHeader[headerSize++] = (fixed_buffersize >> 24) & 0xff;
        PesHeader[headerSize++] = (fixed_buffersize >> 16) & 0xff;
        PesHeader[headerSize++] = (fixed_buffersize >> 8)  & 0xff;
        PesHeader[headerSize++] = fixed_buffersize & 0xff;
        
        memcpy(PesHeader + headerSize, codec_data, sizeof(codec_data));
        headerSize += sizeof(codec_data);

        PesHeader[6] |= 1;

        struct iovec iov[2];
        iov[0].iov_base = PesHeader;
        iov[0].iov_len  = headerSize;
        iov[1].iov_base = fixed_buffer;
        iov[1].iov_len  = fixed_buffersize;
        call->WriteV(call->fd, iov, 2);
        fixed_buffertimestamp += fixed_bufferduration;
    }
    
    return size;
}

/* ***************************** */
/* Writer  Definition            */
/* ***************************** */

static WriterCaps_t caps_pcm = 
{
    "pcm",
    eAudio,
    "A_PCM",
    -1,
    0x30,
    -1
};

struct Writer_s WriterAudioPCM = 
{
    &reset,
    &writeData,
    NULL,
    &caps_pcm
};

static WriterCaps_t caps_ipcm = 
{
    "ipcm",
    eAudio,
    "A_IPCM",
    -1,
    0x30,
    -1
};

struct Writer_s WriterAudioIPCM = 
{
    &reset,
    &writeData, /* writeDataIPCM */
    NULL,
    &caps_ipcm
};
