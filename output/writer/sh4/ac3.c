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
#define AC3_HEADER_LENGTH       7

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

    ac3_printf(10, "\n");

    unsigned char  PesHeader[PES_MAX_HEADER_SIZE];

    if (call == NULL)
    {
        ac3_err("call data is NULL...\n");
        return 0;
    }

    ac3_printf(10, "AudioPts %lld\n", call->Pts);

    if ((call->data == NULL) || (call->len <= 0))
    {
        ac3_err("parsing NULL Data. ignoring...\n");
        return 0;
    }

    if (call->fd < 0)
    {
        ac3_err("file pointer < 0. ignoring ...\n");
        return 0;
    }

    struct iovec iov[2];

    iov[0].iov_base = PesHeader;
    iov[0].iov_len = InsertPesHeader (PesHeader, call->len, PRIVATE_STREAM_1_PES_START_CODE, call->Pts, 0);
    iov[1].iov_base = call->data;
    iov[1].iov_len = call->len;

    return call->WriteV(call->fd, iov, 2);
}

/* ***************************** */
/* Writer  Definition            */
/* ***************************** */

static WriterCaps_t caps_ac3 = {
    "ac3",
    eAudio,
    "A_AC3",
    AUDIO_ENCODING_AC3,
    -1,
    -1
};

struct Writer_s WriterAudioAC3 = {
    &reset,
    &writeData,
    NULL,
    &caps_ac3
};

static WriterCaps_t caps_eac3 = {
    "ac3",
    eAudio,
    "A_EAC3",
    AUDIO_ENCODING_AC3,
    -1,
    -1
};

struct Writer_s WriterAudioEAC3 = {
    &reset,
    &writeData,
    NULL,
    &caps_eac3
};