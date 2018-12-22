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

#include "debug.h"
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
unsigned char AC3_SYNC_HEADER[] = {0x80, 0x01, 0x00, 0x01};

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

    struct iovec iov[3];

    iov[0].iov_base = PesHeader;
    iov[0].iov_len = InsertPesHeader (PesHeader, call->len, MPEG_AUDIO_PES_START_CODE, call->Pts, 0); //+ sizeof(AC3_SYNC_HEADER)
    
    //PesHeader[6] = 0x81;
    //PesHeader[7] = 0x80;
    //PesHeader[8] = 0x09;
                
    //iov[1].iov_base = AC3_SYNC_HEADER;
    //iov[1].iov_len = sizeof(AC3_SYNC_HEADER);
    iov[1].iov_base = call->data;
    iov[1].iov_len = call->len;
    
    ac3_printf(40, "PES HEADER LEN %d\n", (int)iov[0].iov_len);

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
    AUDIOTYPE_AC3,
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
    AUDIOTYPE_AC3_PLUS,
    -1
};

struct Writer_s WriterAudioEAC3 = {
    &reset,
    &writeData,
    NULL,
    &caps_eac3
};
