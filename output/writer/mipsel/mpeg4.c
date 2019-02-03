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

static int writeData(void* _call)
{
    WriterAVCallData_t* call = (WriterAVCallData_t*) _call;

    unsigned char  PesHeader[PES_MAX_HEADER_SIZE];

    mpeg4_printf(10, "\n");

    if (call == NULL)
    {
        mpeg4_err("call data is NULL...\n");
        return 0;
    }

    if ((call->data == NULL) || (call->len <= 0))
    {
        mpeg4_err("parsing NULL Data. ignoring...\n");
        return 0;
    }

    if (call->fd < 0)
    {
        mpeg4_err("file pointer < 0. ignoring ...\n");
        return 0;
    }

    mpeg4_printf(10, "VideoPts %lld\n", call->Pts);


    unsigned int PacketLength = call->len;
    if (initialHeader && call->private_size && call->private_data != NULL)
    {
        PacketLength += call->private_size;
    }

    struct iovec iov[3];
    int ic = 0;
    iov[ic].iov_base = PesHeader;
    iov[ic++].iov_len = InsertPesHeader (PesHeader, PacketLength, MPEG_VIDEO_PES_START_CODE, call->Pts, 0);

    if (initialHeader && call->private_size && call->private_data != NULL)
    {
        initialHeader = 0;
        iov[ic].iov_base = call->private_data;
        iov[ic++].iov_len = call->private_size;
    }
    iov[ic].iov_base = call->data;
    iov[ic++].iov_len = call->len;

    int len = call->WriteV(call->fd, iov, ic);

    mpeg4_printf(10, "xvid_Write < len=%d\n", len);

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
    STREAMTYPE_MPEG4_Part2,
    -1
};

struct Writer_s WriterVideoMPEG4 = {
    &reset,
    &writeData,
    NULL,
    &mpeg4p2_caps
};

static WriterCaps_t caps_h263 = {
    "h263",
    eVideo,
    "V_H263",
    VIDEO_ENCODING_H263,
    STREAMTYPE_H263,
    -1
};

struct Writer_s WriterVideoH263 = {
    &reset,
    &writeData,
    NULL,
    &caps_h263
};