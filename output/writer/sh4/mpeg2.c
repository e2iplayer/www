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

/* ***************************** */
/* Prototypes                    */
/* ***************************** */

/* ***************************** */
/* MISC Functions                */
/* ***************************** */

static int reset()
{
    return 0;
}

static int writeData(void* _call)
{
    WriterAVCallData_t* call = (WriterAVCallData_t*) _call;

    unsigned char               PesHeader[PES_MAX_HEADER_SIZE];
    int len = 0;
    unsigned int Position = 0;

    mpeg2_printf(10, "\n");

    if (call == NULL)
    {
        mpeg2_err("call data is NULL...\n");
        return 0;
    }

    mpeg2_printf(10, "VideoPts %lld\n", call->Pts);

    if ((call->data == NULL) || (call->len <= 0))
    {
        mpeg2_err("parsing NULL Data. ignoring...\n");
        return 0;
    }

    if (call->fd < 0)
    {
        mpeg2_err("file pointer < 0. ignoring ...\n");
        return 0;
    }

    while(Position < call->len) 
    {
        int32_t PacketLength = (call->len - Position) <= MAX_PES_PACKET_SIZE ?
                           (call->len - Position) : MAX_PES_PACKET_SIZE;

        int32_t Remaining = call->len - Position - PacketLength;

        mpeg2_printf(20, "PacketLength=%d, Remaining=%d, Position=%d\n", PacketLength, Remaining, Position);

        struct iovec iov[2];
        iov[0].iov_base = PesHeader;
        iov[0].iov_len = InsertPesHeader (PesHeader, PacketLength, 0xe0, call->Pts, 0);
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

    mpeg2_printf(10, "< len %d\n", len);
    return len;
}

/* ***************************** */
/* Writer  Definition            */
/* ***************************** */
static WriterCaps_t caps = {
    "mpeg2",
    eVideo,
    "V_MPEG2",
    VIDEO_ENCODING_AUTO,
    -1,
    -1,
};

struct Writer_s WriterVideoMPEG2 = {
    &reset,
    &writeData,
    NULL,
    &caps
};

static WriterCaps_t h264_caps = {
    "mpges_h264",
    eVideo,
    "V_MPEG2/H264",
    VIDEO_ENCODING_H264,
    -1,
    -1
};

struct Writer_s WriterVideoMPEGH264 = {
    &reset,
    &writeData,
    NULL,
    &h264_caps
};
