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
#define SAM_WITH_DEBUG
#ifdef SAM_WITH_DEBUG
#define AMR_DEBUG
#else
#define AMR_SILENT
#endif

#ifdef AMR_DEBUG

static short debug_level = 0;

#define amr_printf(level, fmt, x...) do { \
if (debug_level >= level) printf("[%s:%s] " fmt, __FILE__, __FUNCTION__, ## x); } while (0)
#else
#define amr_printf(level, fmt, x...)
#endif

#ifndef AMR_SILENT
#define amr_err(fmt, x...) do { printf("[%s:%s] " fmt, __FILE__, __FUNCTION__, ## x); } while (0)
#else
#define amr_err(fmt, x...)
#endif

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

    unsigned char  PesHeader[PES_MAX_HEADER_SIZE + 4 + 9];

    amr_printf(10, "\n");

    if (call == NULL)
    {
        amr_err("call data is NULL...\n");
        return 0;
    }

    amr_printf(10, "AudioPts %lld\n", call->Pts);

    if ((call->data == NULL) || (call->len <= 0))
    {
        amr_err("parsing NULL Data. ignoring...\n");
        return 0;
    }

    if (call->fd < 0)
    {
        amr_err("file pointer < 0. ignoring ...\n");
        return 0;
    }

    uint8_t hasCodecData = 1;
    if(NULL != call->private_data && call->private_size >= 17)
    {
        amr_err("wrong private_data. ignoring ...\n");
        hasCodecData = 1;
    }
    size_t payload_len = call->len;
    if(hasCodecData)
    {
        payload_len += 9;
    }
    payload_len += 4;
    
    uint32_t headerSize = InsertPesHeader(PesHeader, payload_len, MPEG_AUDIO_PES_START_CODE, call->Pts, 0);
    PesHeader[headerSize++] = (payload_len >> 24) & 0xff;
    PesHeader[headerSize++] = (payload_len >> 16) & 0xff;
    PesHeader[headerSize++] = (payload_len >> 8)  & 0xff;
    PesHeader[headerSize++] = payload_len & 0xff;
    
    if(hasCodecData)
    {
        uint8_t tmp[] = {0x45, 0x4d, 0x50, 0x20, 0x00, 0x00, 0x80, 0x00, 0x01};
        memcpy(&PesHeader[headerSize], tmp, 9);
        //memcpy(&PesHeader[headerSize], call->private_data + 8, 9);
        //memset(&PesHeader[headerSize], 0, 9);
    }
    
    struct iovec iov[2];
    iov[0].iov_base = PesHeader;
    iov[0].iov_len = headerSize;
    iov[1].iov_base = call->data;
    iov[1].iov_len = call->len;
    
    int len = call->WriteV(call->fd, iov, 2);

    amr_printf(10, "amr_Write-< len=%d\n", len);
    return len;
}

/* ***************************** */
/* Writer  Definition            */
/* ***************************** */

static WriterCaps_t caps_amr = {
    "amr",
    eAudio,
    "A_AMR",
    -1,
    AUDIOTYPE_AMR,
    -1
};

struct Writer_s WriterAudioAMR = {
    &reset,
    &writeData,
    NULL,
    &caps_amr
};


