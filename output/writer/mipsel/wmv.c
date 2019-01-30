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
#define WMV_FRAME_START_CODE 0x0d

/* ***************************** */
/* Types                         */
/* ***************************** */

static const  uint8_t Vc1FrameStartCode[]     = {0, 0, 1, WMV_FRAME_START_CODE};

/* ***************************** */
/* Varaibles                     */
/* ***************************** */
static int initialHeader = 1;
static video_codec_data_t videocodecdata = {0, 0};

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

    wmv_printf(10, "VideoPts %lld\n", call->Pts);
    wmv_printf(10, "Got Private Size %d\n", call->private_size);
    
    unsigned char PesHeader[PES_MAX_HEADER_SIZE + sizeof(Vc1FrameStartCode)];
    int32_t ic = 0;
    struct iovec iov[5];
    unsigned int PacketLength = 0;
    
    iov[ic++].iov_base = PesHeader;
    if (initialHeader) 
    {
        initialHeader = 0;
        if(videocodecdata.data)
        {
            free(videocodecdata.data);
            videocodecdata.data = NULL;
        }

        unsigned int codec_size = call->private_size;
        if (codec_size > 4) codec_size = 4;
 
        videocodecdata.length = 33;
        uint8_t *data = videocodecdata.data = malloc(videocodecdata.length);
        memset(videocodecdata.data, 0, videocodecdata.length);
        data += 18;
        /* width */
        *(data++) = (call->Width >> 8) & 0xff;
        *(data++) = call->Width & 0xff;
        /* height */
        *(data++) = (call->Height >> 8) & 0xff;
        *(data++) = call->Height & 0xff;
        if (call->private_data && codec_size) memcpy(data, call->private_data, codec_size);
        if(STB_DREAMBOX == GetSTBType() || 0 != ioctl(call->fd, VIDEO_SET_CODEC_DATA, &videocodecdata))
        {
            iov[ic].iov_base  = videocodecdata.data;
            iov[ic++].iov_len = videocodecdata.length;
            PacketLength     += videocodecdata.length;
        }
    }

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

    /* some mipsel receiver(s) like et4x00 needs to have Copy(0)/Original(1) flag set to Original */
    PesHeader[6] |= 1;
    
    if(needFrameStartCode)
    {
        memcpy(PesHeader + iov[0].iov_len, Vc1FrameStartCode, sizeof(Vc1FrameStartCode) );
        iov[0].iov_len += sizeof(Vc1FrameStartCode);
    }
    
    if(videocodecdata.data)
    {
        free(videocodecdata.data);
        videocodecdata.data = NULL;
    }
    
    return call->WriteV(call->fd, iov, ic);
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
