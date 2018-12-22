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

/* ***************************** */
/* Types                         */
/* ***************************** */

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

static uint8_t updateCodecData(uint8_t *data, int32_t size)
{
    static uint8_t *oldData = NULL;
    static int32_t oldSize = 0;
    
    uint8_t update = 0;
    if (data != NULL && size > 0)
    {
        if (size != oldSize)
        {
            update = 1;
        }
        else
        {
            uint32_t i = 0;
            for (i = 0; i < size; i++)
            {
                if (data[i] != oldData[i])
                {
                    update = 1;
                    break;
                }
            }
        }
    }

    if (update)
    {
        if (oldData != NULL)
        {
            free(oldData);
        }
        oldData = malloc(size);
        memcpy(oldData, data, size);
        oldSize = size;
    }

    return update;
}

static int writeData(void* _call)
{
    WriterAVCallData_t* call = (WriterAVCallData_t*) _call;

    unsigned char  PesHeader[PES_MAX_HEADER_SIZE];
    unsigned char  FakeHeaders[64]; // 64bytes should be enough to make the fake headers
    unsigned int   FakeHeaderLength;
    unsigned char  Version             = 5;
    unsigned int   FakeStartCode       = (Version << 8) | PES_VERSION_FAKE_START_CODE;
    unsigned int   usecPerFrame = 41708; /* Hellmaster1024: default value */
    BitPacker_t ld = {FakeHeaders, 0, 32};

    divx_printf(10, "\n");

    if (call == NULL)
    {
        divx_err("call data is NULL...\n");
        return 0;
    }

    if ((call->data == NULL) || (call->len <= 0))
    {
        divx_err("parsing NULL Data. ignoring...\n");
        return 0;
    }

    if (call->fd < 0)
    {
        divx_err("file pointer < 0. ignoring ...\n");
        return 0;
    }

    divx_printf(10, "AudioPts %lld\n", call->Pts);

    usecPerFrame = 1000000000 / call->FrameRate;
    divx_printf(10, "Microsecends per frame = %d\n", usecPerFrame);

    memset(FakeHeaders, 0, sizeof(FakeHeaders));

    /* Create info record for frame parser */
    /* divx4 & 5
       VOS
       PutBits(&ld, 0x0, 8);
       PutBits(&ld, 0x0, 8);
     */
    PutBits(&ld, 0x1b0, 32);      // startcode
    PutBits(&ld, 0, 8);           // profile = reserved
    PutBits(&ld, 0x1b2, 32);      // startcode (user data)
    PutBits(&ld, 0x53545443, 32); // STTC - an embedded ST timecode from an avi file
    PutBits(&ld, usecPerFrame , 32);
    // microseconds per frame
    FlushBits(&ld);

    FakeHeaderLength    = (ld.Ptr - (FakeHeaders));

    struct iovec iov[4];
    int ic = 0;
    iov[ic].iov_base = PesHeader;
    iov[ic++].iov_len = InsertPesHeader (PesHeader, call->len, MPEG_VIDEO_PES_START_CODE, call->Pts, FakeStartCode);
    iov[ic].iov_base = FakeHeaders;
    iov[ic++].iov_len = FakeHeaderLength;
    
    if (initialHeader) 
    {
        iov[ic].iov_base = call->private_data;
        iov[ic++].iov_len = call->private_size;
        initialHeader = 0;
    }
    
    iov[ic].iov_base = call->data;
    iov[ic++].iov_len = call->len;

    int len = call->WriteV(call->fd, iov, ic);

    divx_printf(10, "xvid_Write < len=%d\n", len);

    return len;
}

/* ***************************** */
/* Writer  Definition            */
/* ***************************** */

static WriterCaps_t mpeg4p2_caps = {
    "mpeg4p2",
    eVideo,
    "V_MPEG4",
    VIDEO_ENCODING_MPEG4P2,
    -1,
    -1
};

struct Writer_s WriterVideoMPEG4 = {
    &reset,
    &writeData,
    NULL,
    &mpeg4p2_caps
};


struct Writer_s WriterVideoMSCOMP = {
    &reset,
    &writeData,
    NULL,
    &mpeg4p2_caps
};

static WriterCaps_t fourcc_caps = {
    "fourcc",
    eVideo,
    "V_MS/VFW/FOURCC",
    VIDEO_ENCODING_MPEG4P2,
    -1,
    -1
};

struct Writer_s WriterVideoFOURCC = {
    &reset,
    &writeData,
    NULL,
    &fourcc_caps
};

static WriterCaps_t divx_caps = {
    "divx",
    eVideo,
    "V_MKV/XVID",
    VIDEO_ENCODING_MPEG4P2,
    -1,
    -1
};

struct Writer_s WriterVideoDIVX = {
    &reset,
    &writeData,
    NULL,
    &divx_caps
};
