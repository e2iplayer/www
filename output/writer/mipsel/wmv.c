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
/* Includes		      */
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
/* Makros/Constants	      */
/* ***************************** */

#define WMV3_PRIVATE_DATA_LENGTH 4

#define METADATA_STRUCT_A_START     12
#define METADATA_STRUCT_B_START     24
#define METADATA_STRUCT_B_FRAMERATE_START   32
#define METADATA_STRUCT_C_START     8


#define WMV_SEQUENCE_LAYER_METADATA_START_CODE  0x80
#define WMV_FRAME_START_CODE 0x0d

#define SAM_WITH_DEBUG
#ifdef SAM_WITH_DEBUG
#define WMV_DEBUG
#else
#define WMV_SILENT
#endif

#ifdef WMV_DEBUG

static short debug_level = 10;

#define wmv_printf(level, fmt, x...) do { \
if (debug_level >= level) printf("[%s:%s] " fmt, __FILE__, __FUNCTION__, ## x); } while (0)
#else
#define wmv_printf(level, fmt, x...)
#endif

#ifndef WMV_SILENT
#define wmv_err(fmt, x...) do { printf("[%s:%s] " fmt, __FILE__, __FUNCTION__, ## x); } while (0)
#else
#define wmv_err(fmt, x...)
#endif


/* ***************************** */
/* Types			 */
/* ***************************** */

static const  uint8_t Vc1FrameStartCode[]     = {0, 0, 1, WMV_FRAME_START_CODE};

static const uint8_t  Metadata[] =
{
    0x00,    0x00,   0x00,   0xc5,
    0x04,    0x00,   0x00,   0x00,
    0xc0,    0x00,   0x00,   0x00,   /* Struct C set for for advanced profile*/
    0x00,    0x00,   0x00,   0x00,   /* Struct A */
    0x00,    0x00,   0x00,   0x00,
    0x0c,    0x00,   0x00,   0x00,
    0x60,    0x00,   0x00,   0x00,   /* Struct B */
    0x00,    0x00,   0x00,   0x00,
    0x00,    0x00,   0x00,   0x00
};

/* ***************************** */
/* Varaibles                     */
/* ***************************** */
static int initialHeader = 1;

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

    wmv_printf(10, "\n");

    if (call == NULL) 
    {
        wmv_err("call data is NULL...\n");
        return 0;
    }

    if ((call->data == NULL) || (call->len <= 0)) 
    {
        wmv_err("parsing NULL Data. ignoring...\n");
        return 0;
    }

    if (call->fd < 0) 
    {
        wmv_err("file pointer < 0. ignoring ...\n");
        return 0;
    }
    /*
    if(call->private_size <= 0 || NULL == call->private_data)
    {
        wmv_err("empty private_data < 0. ignoring ...\n");
        return 0;
    }
    */
    printf("--------------------------> call->private_data[%p]\n", call->private_data);
    
    unsigned char PesHeader[PES_MAX_HEADER_SIZE + sizeof(Vc1FrameStartCode)];
    int32_t ic = 0;
    struct iovec iov[5];
    unsigned int PacketLength = 0;
    
    iov[ic++].iov_base = PesHeader;
    if (initialHeader) 
    {
        wmv_printf(10, "VideoPts %lld\n", call->Pts);
        wmv_printf(10, "Got Private Size %d\n", call->private_size);
        wmv_printf(10, "Framerate: %u\n", call->FrameRate);
        wmv_printf(10, "biWidth: %d\n",   call->Width);
        wmv_printf(10, "biHeight: %d\n",  call->Height);
    
        initialHeader = 0;
        video_codec_data_t videocodecdata = {0, 0};
#if 1
        videocodecdata.length = sizeof(Metadata);
        videocodecdata.data = malloc(videocodecdata.length);
        uint8_t *pData = videocodecdata.data;
        
        memcpy(videocodecdata.data, Metadata, sizeof(Metadata));
        
        uint32_t crazyFramerate = 0;

        wmv_printf(10, "Framerate: %u\n", call->FrameRate);
        wmv_printf(10, "biWidth: %d\n",   call->Width);
        wmv_printf(10, "biHeight: %d\n",  call->Height);

        crazyFramerate = ((10000000.0 / call->FrameRate) * 1000.0);
        wmv_printf(10, "crazyFramerate: %u\n", crazyFramerate);
        
        pData += METADATA_STRUCT_C_START;

        if(call->private_size > 0 && NULL != call->private_data)
        {
            memcpy (pData, call->private_data, WMV3_PRIVATE_DATA_LENGTH);
        }
        pData += WMV3_PRIVATE_DATA_LENGTH;

        /* Metadata Header Struct A */
        *pData++           = (call->Height >>  0) & 0xff;
        *pData++           = (call->Height >>  8) & 0xff;
        *pData++           = (call->Height >> 16) & 0xff;
        *pData++           =  call->Height >> 24;
        *pData++           = (call->Width  >>  0) & 0xff;
        *pData++           = (call->Width  >>  8) & 0xff;
        *pData++           = (call->Width  >> 16) & 0xff;
        *pData++           =  call->Width  >> 24;

        pData             += 12;       /* Skip flag word and Struct B first 8 bytes */

        *pData++           = (crazyFramerate >>  0) & 0xff;
        *pData++           = (crazyFramerate >>  8) & 0xff;
        *pData++           = (crazyFramerate >> 16) & 0xff;
        *pData++           =  crazyFramerate >> 24;
        ioctl(call->fd, VIDEO_SET_CODEC_DATA, &videocodecdata);
#else
        
        videocodecdata.length = call->private_size + 22;
        if(videocodecdata.length<33)
        {
            videocodecdata.length = 33;
        }
        
        videocodecdata.data  = malloc(videocodecdata.length);
        printf("length[%d] private_size[%d]\n", videocodecdata.length, call->private_size);
        memset(videocodecdata.data, 0, videocodecdata.length);
        
        uint8_t *pData = videocodecdata.data + 18;
        /* width */
        *(pData++) = (call->Width >> 8) & 0xff;
        *(pData++) = call->Width & 0xff;
        /* height */
        *(pData++) = (call->Height >> 8) & 0xff;
        *(pData++) = call->Height & 0xff;
        
        if(call->private_size > 0 && NULL != call->private_data)
        {
            memcpy(pData, call->private_data, call->private_size);
        }
        
        if(!ioctl(call->fd, VIDEO_SET_CODEC_DATA, &videocodecdata))
        {
            ;//printf("OKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKK\n");
        }
        else
        {
            printf("NOT OKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKK\n");
            iov[ic].iov_base  = call->private_data;
            iov[ic++].iov_len = call->private_size;
            PacketLength     += call->private_size;
        }
#endif
        
        if(videocodecdata.data)
        {
            free(videocodecdata.data);
        }
    }
    
    printf("call->private_size [%d]\n", call->private_size);
    
    uint8_t needFrameStartCode = 0;
    if( sizeof(Vc1FrameStartCode) >= call->len
        || memcmp(call->data, Vc1FrameStartCode, sizeof(Vc1FrameStartCode)) != 0 )
    {
        needFrameStartCode = 1;
        PacketLength += sizeof(Vc1FrameStartCode);
    }
    
    iov[ic].iov_base  = call->data;
    iov[ic++].iov_len = call->len;
    PacketLength     += call->len;
    
    iov[0].iov_len = InsertPesHeader(PesHeader, PacketLength, MPEG_VIDEO_PES_START_CODE, call->Pts, 0);
    if(needFrameStartCode)
    {
        memcpy(PesHeader + iov[0].iov_len, Vc1FrameStartCode, sizeof(Vc1FrameStartCode) );
        iov[0].iov_len += sizeof(Vc1FrameStartCode);
    }
    
    return writev_with_retry(call->fd, iov, ic);
}

/* ***************************** */
/* Writer  Definition            */
/* ***************************** */

static WriterCaps_t caps = {
    "wmv",
    eVideo,
    "V_WMV",
    VIDEO_ENCODING_WMV,
    STREAMTYPE_VC1_SM,
    CT_MPEG4_PART2
};

struct Writer_s WriterVideoWMV = {
    &reset,
    &writeData,
    NULL,
    &caps
};
