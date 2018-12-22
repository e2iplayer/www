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
#include "common.h"
#include "output.h"
#include "debug.h"
#include "misc.h"
#include "pes.h"
#include "writer.h"

/* ***************************** */
/* Makros/Constants              */
/* ***************************** */

#define WMV3_PRIVATE_DATA_LENGTH			4

#define METADATA_STRUCT_A_START	     12
#define METADATA_STRUCT_B_START	     24
#define METADATA_STRUCT_B_FRAMERATE_START   32
#define METADATA_STRUCT_C_START	     8


#define VC1_SEQUENCE_LAYER_METADATA_START_CODE    0x80
#define VC1_FRAME_START_CODE                      0x0d

/* ***************************** */
/* Types                         */
/* ***************************** */

static const unsigned char  SequenceLayerStartCode[]  = {0x00,    0x00,   0x01,   VC1_SEQUENCE_LAYER_METADATA_START_CODE};


static const unsigned char  Metadata[]	  =
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
static unsigned char FrameHeaderSeen = 0;

/* ***************************** */
/* Prototypes                    */
/* ***************************** */

/* ***************************** */
/* MISC Functions                */
/* ***************************** */
static int reset()
{
    initialHeader = 1;
    FrameHeaderSeen = 0;
    return 0;
}

static int writeData(void* _call)
{
    WriterAVCallData_t* call = (WriterAVCallData_t*) _call;

    int len = 0;
    vc1_printf(10, "\n");

    if (call == NULL) 
    {
        vc1_err("call data is NULL...\n");
        return 0;
    }

    if ((call->data == NULL) || (call->len <= 0))
    {
        vc1_err("parsing NULL Data. ignoring...\n");
        return 0;
    }

    if (call->fd < 0)
    {
        vc1_err("file pointer < 0. ignoring ...\n");
        return 0;
    }

    vc1_printf(10, "VideoPts %lld\n", call->Pts);
    vc1_printf(10, "Got Private Size %d\n", call->private_size);

    if (initialHeader)
    {
        unsigned char   PesHeader[PES_MAX_HEADER_SIZE];
        unsigned char   PesPayload[128];
        unsigned char  *PesPtr;
        unsigned int    crazyFramerate = 0;
        struct iovec    iov[2];

        vc1_printf(10, "Framerate: %u\n", call->FrameRate);
        vc1_printf(10, "biWidth: %d\n",   call->Width);
        vc1_printf(10, "biHeight: %d\n",  call->Height);
        
        crazyFramerate = ((10000000.0 / call->FrameRate) * 1000.0);
        vc1_printf(10, "crazyFramerate: %u\n", crazyFramerate);
        
        memset(PesPayload, 0, sizeof(PesPayload));
        
        PesPtr = PesPayload;
        
        memcpy(PesPtr, SequenceLayerStartCode, sizeof(SequenceLayerStartCode));
        PesPtr += sizeof(SequenceLayerStartCode);

        memcpy(PesPtr, Metadata, sizeof(Metadata));
        PesPtr += METADATA_STRUCT_C_START;
        PesPtr += WMV3_PRIVATE_DATA_LENGTH;

        /* Metadata Header Struct A */
        *PesPtr++ = (call->Height >>  0) & 0xff;
        *PesPtr++ = (call->Height >>  8) & 0xff;
        *PesPtr++ = (call->Height >> 16) & 0xff;
        *PesPtr++ =  call->Height >> 24;
        *PesPtr++ = (call->Width  >>  0) & 0xff;
        *PesPtr++ = (call->Width  >>  8) & 0xff;
        *PesPtr++ = (call->Width  >> 16) & 0xff;
        *PesPtr++ =  call->Width  >> 24;

        PesPtr += 12; /* Skip flag word and Struct B first 8 bytes */

        *PesPtr++ = (crazyFramerate >>  0) & 0xff;
        *PesPtr++ = (crazyFramerate >>  8) & 0xff;
        *PesPtr++ = (crazyFramerate >> 16) & 0xff;
        *PesPtr++ =  crazyFramerate >> 24;

        iov[0].iov_base = PesHeader;
        iov[1].iov_base = PesPayload;
        iov[1].iov_len = PesPtr - PesPayload;
        iov[0].iov_len = InsertPesHeader (PesHeader, iov[1].iov_len, VC1_VIDEO_PES_START_CODE, INVALID_PTS_VALUE, 0);
        len = call->WriteV(call->fd, iov, 2);

        /* For VC1 the codec private data is a standard vc1 sequence header so we just copy it to the output */
        iov[0].iov_base = PesHeader;
        iov[1].iov_base = call->private_data;
        iov[1].iov_len = call->private_size;
        iov[0].iov_len = InsertPesHeader (PesHeader, iov[1].iov_len, VC1_VIDEO_PES_START_CODE, INVALID_PTS_VALUE, 0);
        len = call->WriteV(call->fd, iov, 2);

        initialHeader = 0;
    }

    if(call->len > 0 && call->data) 
    {
        uint32_t Position = 0;
        uint8_t insertSampleHeader = 1;

        while(Position < call->len) 
        {

            int32_t PacketLength = (call->len - Position) <= MAX_PES_PACKET_SIZE ?
                       (call->len - Position) : MAX_PES_PACKET_SIZE;

            int32_t Remaining = call->len - Position - PacketLength;

            vc1_printf(20, "PacketLength=%d, Remaining=%d, Position=%d\n", PacketLength, Remaining, Position);

            uint8_t PesHeader[PES_MAX_HEADER_SIZE];
            int32_t HeaderLength = InsertPesHeader (PesHeader, PacketLength, VC1_VIDEO_PES_START_CODE, call->Pts, 0);

            if(insertSampleHeader) 
            {
                const uint8_t Vc1FrameStartCode[] = {0, 0, 1, VC1_FRAME_START_CODE};

                if (!FrameHeaderSeen && (call->len > 3) && (memcmp (call->data, Vc1FrameStartCode, 4) == 0))
                {
                    FrameHeaderSeen = 1;
                }
                
                if (!FrameHeaderSeen)
                {
                    memcpy (&PesHeader[HeaderLength], Vc1FrameStartCode, sizeof(Vc1FrameStartCode));
                    HeaderLength += sizeof(Vc1FrameStartCode);
                }
                insertSampleHeader = 0;
            }

            struct iovec iov[2];
            iov[0].iov_base = PesHeader;
            iov[0].iov_len = HeaderLength;
            iov[1].iov_base = call->data + Position;
            iov[1].iov_len = PacketLength;

            ssize_t l = call->WriteV(call->fd, iov, 2);
            if (l < 0) 
            {
                len = l;
                break;
            }
            len += l;

            Position += PacketLength;
            call->Pts = INVALID_PTS_VALUE;
        }
    }

    vc1_printf(10, "< %d\n", len);
    return len;
}

/* ***************************** */
/* Writer  Definition	    */
/* ***************************** */

static WriterCaps_t caps = {
    "vc1",
    eVideo,
    "V_VC1",
    VIDEO_ENCODING_VC1,
    -1,
    -1
};

struct Writer_s WriterVideoVC1 = {
    &reset,
    &writeData,
    NULL,
    &caps
};

