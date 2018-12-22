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

    if (initialHeader)
    {

        unsigned char  PesHeader[PES_MAX_HEADER_SIZE];

        if ((call->private_size <= 0) || (call->private_data == NULL))
        {
            wma_err("private NULL.\n");
            return -1;
        }
        
        struct iovec iov[2];
        iov[0].iov_base = PesHeader;
        iov[0].iov_len = InsertPesHeader (PesHeader, call->private_size, MPEG_AUDIO_PES_START_CODE, 0, 0);
        iov[1].iov_base = call->private_data;
        iov[1].iov_len = call->private_size;
        len = call->WriteV(call->fd, iov, 2);
        initialHeader = 0;
    }

    if (len > -1 && call->len > 0 && call->data)
    {
        unsigned char  PesHeader[PES_MAX_HEADER_SIZE];
        struct iovec iov[2];
        iov[0].iov_base = PesHeader;
        iov[0].iov_len = InsertPesHeader (PesHeader, call->len, MPEG_AUDIO_PES_START_CODE, call->Pts, 0);
        iov[1].iov_base = call->data;
        iov[1].iov_len = call->len;

        ssize_t l = call->WriteV(call->fd, iov, 2);
        len = (l > -1) ? len + l : l;
    }

    wma_printf(10, "wma < %d\n", len);

    return len;
}

/* ***************************** */
/* Writer Definition            */
/* ***************************** */

static WriterCaps_t capsWMAPRO = {
    "wma/pro",
    eAudio,
    "A_WMA/PRO",
    AUDIO_ENCODING_WMA,
    -1,
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
    -1,
    -1
};

struct Writer_s WriterAudioWMA = {
    &reset,
    &writeData,
    NULL,
    &capsWMA
};
