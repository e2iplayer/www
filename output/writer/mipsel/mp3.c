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

    unsigned char  PesHeader[PES_MAX_HEADER_SIZE + 22];

    mp3_printf(10, "\n");

    if (call == NULL || call->data == NULL || call->len <= 0 || call->fd < 0) {
        wma_err("Wrong input call: %p, data: %p, len: %d, fd: %d\n", call, call->data, call->len, call->fd);
        return 0;
    }

    mp3_printf(10, "AudioPts %lld\n", call->Pts);

    call->private_size = 0;
    
    uint32_t headerSize = InsertPesHeader (PesHeader, call->len + call->private_size, MPEG_AUDIO_PES_START_CODE, call->Pts, 0);
    if(call->private_size > 0)
    {
        memcpy(&PesHeader[headerSize], call->private_data, call->private_size);
        headerSize += call->private_size;
    }
    struct iovec iov[2];
    iov[0].iov_base = PesHeader;
    iov[0].iov_len = headerSize;
    iov[1].iov_base = call->data;
    iov[1].iov_len = call->len;

    int len = call->WriteV(call->fd, iov, 2);

    mp3_printf(10, "mp3_Write-< len=%d\n", len);
    return len;
}

/* ***************************** */
/* Writer  Definition            */
/* ***************************** */

static WriterCaps_t caps_mp3 = {
    "mp3",
    eAudio,
    "A_MP3",
    AUDIO_ENCODING_MP3,
    AUDIOTYPE_MP3,
    -1
};

struct Writer_s WriterAudioMP3 = {
    &reset,
    &writeData,
    NULL,
    &caps_mp3
};

static WriterCaps_t caps_mpegl3 = {
    "mpeg/l3",
    eAudio,
    "A_MPEG/L3",
    AUDIO_ENCODING_MPEG2,
    AUDIOTYPE_MP3,
    -1
};

struct Writer_s WriterAudioMPEGL3 = {
    &reset,
    &writeData,
    NULL,
    &caps_mpegl3
};
