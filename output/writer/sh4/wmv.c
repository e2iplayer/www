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
#include "common.h"
#include "output.h"
#include "debug.h"
#include "misc.h"
#include "pes.h"
#include "writer.h"

/* ***************************** */
/* Makros/Constants              */
/* ***************************** */

#define WMV3_PRIVATE_DATA_LENGTH        4

#define METADATA_STRUCT_A_START             12
#define METADATA_STRUCT_B_START             24
#define METADATA_STRUCT_B_FRAMERATE_START   32
#define METADATA_STRUCT_C_START             8

/* ***************************** */
/* Types                         */
/* ***************************** */

typedef struct
{
    unsigned char      privateData[WMV3_PRIVATE_DATA_LENGTH];
    unsigned int       width;
    unsigned int       height;
    unsigned int       framerate;
} awmv_t;

static const unsigned char  Metadata[]          =
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

    awmv_t private_data;
    int len = 0;

    wmv_printf(10, "\n");

    if (call == NULL) {
        wmv_err("call data is NULL...\n");
        return 0;
    }

    if ((call->data == NULL) || (call->len <= 0)) {
        wmv_err("parsing NULL Data. ignoring...\n");
        return 0;
    }

    if (call->fd < 0) {
        wmv_err("file pointer < 0. ignoring ...\n");
        return 0;
    }

    wmv_printf(10, "VideoPts %lld\n", call->Pts);
    wmv_printf(10, "Got Private Size %d\n", call->private_size);

    memcpy(private_data.privateData, call->private_data,
           call->private_size>WMV3_PRIVATE_DATA_LENGTH?WMV3_PRIVATE_DATA_LENGTH:call->private_size);

    private_data.width = call->Width;
    private_data.height = call->Height;
    private_data.framerate = call->FrameRate;

#define PES_MIN_HEADER_SIZE 9
    if (initialHeader) {
        unsigned char               PesPacket[PES_MIN_HEADER_SIZE+128];
        unsigned char*              PesPtr;
        unsigned int                MetadataLength;
        unsigned int                crazyFramerate = 0;

        wmv_printf(10, "Framerate: %u\n", private_data.framerate);
        wmv_printf(10, "biWidth: %d\n",   private_data.width);
        wmv_printf(10, "biHeight: %d\n",  private_data.height);

        crazyFramerate = ((10000000.0 / private_data.framerate) * 1000.0);
        wmv_printf(10, "crazyFramerate: %u\n", crazyFramerate);

        PesPtr          = &PesPacket[PES_MIN_HEADER_SIZE];

        memcpy (PesPtr, Metadata, sizeof(Metadata));
        PesPtr         += METADATA_STRUCT_C_START;

        memcpy (PesPtr, private_data.privateData, WMV3_PRIVATE_DATA_LENGTH);
        PesPtr             += WMV3_PRIVATE_DATA_LENGTH;

        /* Metadata Header Struct A */
        *PesPtr++           = (private_data.height >>  0) & 0xff;
        *PesPtr++           = (private_data.height >>  8) & 0xff;
        *PesPtr++           = (private_data.height >> 16) & 0xff;
        *PesPtr++           =  private_data.height >> 24;
        *PesPtr++           = (private_data.width  >>  0) & 0xff;
        *PesPtr++           = (private_data.width  >>  8) & 0xff;
        *PesPtr++           = (private_data.width  >> 16) & 0xff;
        *PesPtr++           =  private_data.width  >> 24;

        PesPtr             += 12;       /* Skip flag word and Struct B first 8 bytes */

        *PesPtr++           = (crazyFramerate >>  0) & 0xff;
        *PesPtr++           = (crazyFramerate >>  8) & 0xff;
        *PesPtr++           = (crazyFramerate >> 16) & 0xff;
        *PesPtr++           =  crazyFramerate >> 24;

        MetadataLength      = PesPtr - &PesPacket[PES_MIN_HEADER_SIZE];

        int HeaderLength        = InsertPesHeader (PesPacket, MetadataLength, VC1_VIDEO_PES_START_CODE, INVALID_PTS_VALUE, 0);

        len = write(call->fd,PesPacket, HeaderLength + MetadataLength);

        initialHeader = 0;
    }

    if(call->len > 0 && call->data) {
        unsigned int Position = 0;
        unsigned char insertSampleHeader = 1;
        while(Position < call->len) {

            int PacketLength = (call->len - Position) <= MAX_PES_PACKET_SIZE ?
                               (call->len - Position) : MAX_PES_PACKET_SIZE;

            int Remaining = call->len - Position - PacketLength;

            wmv_printf(20, "PacketLength=%d, Remaining=%d, Position=%d\n", PacketLength, Remaining, Position);

            unsigned char       PesHeader[PES_MAX_HEADER_SIZE];
            memset (PesHeader, '0', PES_MAX_HEADER_SIZE);
            int                 HeaderLength = InsertPesHeader (PesHeader, PacketLength, VC1_VIDEO_PES_START_CODE, call->Pts, 0);
            unsigned char*      PacketStart;

            if(insertSampleHeader) {
                unsigned int        PesLength;
                unsigned int        PrivateHeaderLength;

                PrivateHeaderLength     = InsertVideoPrivateDataHeader (&PesHeader[HeaderLength],
                                          call->len);
                /* Update PesLength */
                PesLength               = PesHeader[PES_LENGTH_BYTE_0] + 
                                            (PesHeader[PES_LENGTH_BYTE_1] << 8) + PrivateHeaderLength;
                PesHeader[PES_LENGTH_BYTE_0]            = PesLength & 0xff;
                PesHeader[PES_LENGTH_BYTE_1]            = (PesLength >> 8) & 0xff;
                PesHeader[PES_HEADER_DATA_LENGTH_BYTE] += PrivateHeaderLength;
                PesHeader[PES_FLAGS_BYTE]              |= PES_EXTENSION_DATA_PRESENT;

                HeaderLength                           += PrivateHeaderLength;
                insertSampleHeader = 0;
            }

            PacketStart = malloc(call->len + HeaderLength);
            memcpy (PacketStart, PesHeader, HeaderLength);
            memcpy (PacketStart + HeaderLength, call->data + Position, PacketLength);

            len = write(call->fd, PacketStart, PacketLength + HeaderLength);
            free(PacketStart);

            Position += PacketLength;
            call->Pts = INVALID_PTS_VALUE;
        }
    }

    wmv_printf(10, "< %d\n", len);
    return len;
}

/* ***************************** */
/* Writer  Definition            */
/* ***************************** */

static WriterCaps_t caps = {
    "wmv",
    eVideo,
    "V_WMV",
    VIDEO_ENCODING_WMV,
    -1,
    -1
};

struct Writer_s WriterVideoWMV = {
    &reset,
    &writeData,
    NULL,
    &caps
};
