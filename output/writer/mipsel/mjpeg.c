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
static bool must_send_header = false;
static uint8_t *private_data = NULL;
static uint32_t private_size = 0;


/* ***************************** */
/* Prototypes                    */
/* ***************************** */

/* ***************************** */
/* MISC Functions                */
/* ***************************** */

static int reset()
{
    must_send_header = false;
    return 0;
}

static int writeDataSimple(WriterAVCallData_t *call)
{
    uint8_t PesHeader[PES_MAX_HEADER_SIZE];
    struct iovec iov[2];

    iov[0].iov_base = PesHeader;
    iov[0].iov_len = InsertPesHeader(PesHeader, call->len, MPEG_VIDEO_PES_START_CODE, call->Pts, 0);

    iov[1].iov_base = call->data;
    iov[1].iov_len = call->len;

    return call->WriteV(call->fd, iov, 2);;
}

static int writeDataBCMV(WriterAVCallData_t *call)
{
    uint8_t PesHeader[PES_MAX_HEADER_SIZE];
    uint32_t pes_header_len = InsertPesHeader(PesHeader, call->len, MPEG_VIDEO_PES_START_CODE, call->Pts, 0);
    uint32_t len = call->len + 4 + 4 + 2;
    memcpy(PesHeader + pes_header_len, "BCMV", 4);
    pes_header_len += 4;

    PesHeader[pes_header_len++] = (len & 0xFF000000) >> 24;
    PesHeader[pes_header_len++] = (len & 0x00FF0000) >> 16;
    PesHeader[pes_header_len++] = (len & 0x0000FF00) >> 8;
    PesHeader[pes_header_len++] = (len & 0x000000FF) >> 0;
    PesHeader[pes_header_len++] = 0;
    PesHeader[pes_header_len++] = 1;

    int32_t payload_len = call->len + pes_header_len - 6;

    struct iovec iov[2];

    iov[0].iov_base = PesHeader;
    iov[0].iov_len = InsertPesHeader(PesHeader, payload_len, MPEG_VIDEO_PES_START_CODE, call->Pts, 0);

    iov[1].iov_base = call->data;
    iov[1].iov_len = call->len;

    if (payload_len > 0x8008)
        payload_len = 0x8008;

    int offs = 0;
    int bytes = payload_len - 10 - 8;
    UpdatePesHeaderPayloadSize(PesHeader, payload_len);
    // pes header
    if (pes_header_len != WriteExt(call->WriteV, call->fd, PesHeader, pes_header_len)) return -1;
    if (bytes != WriteExt(call->WriteV, call->fd, call->data, bytes)) return -1;
    
    offs += bytes;

    while (bytes < call->len)
    {
        int left = call->len - bytes;
        int wr = 0x8000;
        if (wr > left)
            wr = left;

        //PesHeader[0] = 0x00;
        //PesHeader[1] = 0x00;
        //PesHeader[2] = 0x01;
        //PesHeader[3] = 0xE0;
        PesHeader[6] = 0x81;
        PesHeader[7] = 0x00;
        PesHeader[8] = 0x00;
        pes_header_len = 9;

        UpdatePesHeaderPayloadSize(PesHeader, wr + 3);

        if (pes_header_len != WriteExt(call->WriteV, call->fd, PesHeader, pes_header_len)) return -1;
        if (wr != WriteExt(call->WriteV, call->fd, call->data + offs, wr)) return -1;

        bytes += wr;
        offs += wr;
    }

    return 1;
}

static int writeData(void *_call)
{
    mjpeg_printf(10, "\n");

    WriterAVCallData_t *call = (WriterAVCallData_t *)_call;
    if (call == NULL) {
        mjpeg_err("call data is NULL...\n");
        return 0;
    }

    mjpeg_printf(10, "VideoPts %lld\n", call->Pts);

    if (STB_HISILICON == GetSTBType()) {
        return writeDataSimple(_call);
    }

    return writeDataBCMV(call); 
}

/* ***************************** */
/* Writer  Definition            */
/* ***************************** */
static WriterCaps_t caps = {
    "mjpeg",
    eVideo,
    "V_MJPEG",
    VIDEO_ENCODING_AUTO,
    STREAMTYPE_MJPEG,
    -1
};

struct Writer_s WriterVideoMJPEG = {
    &reset,
    &writeData,
    NULL,
    &caps
};

static WriterCaps_t capsRV30 = {
    "rv30",
    eVideo,
    "V_RV30",
    VIDEO_ENCODING_AUTO,
    STREAMTYPE_RV30,
    -1
};

struct Writer_s WriterVideoRV30 = {
    &reset,
    &writeData,
    NULL,
    &capsRV30
};

static WriterCaps_t capsRV40 = {
    "rv40",
    eVideo,
    "V_RV40",
    VIDEO_ENCODING_AUTO,
    STREAMTYPE_RV40,
    -1
};

struct Writer_s WriterVideoRV40 = {
    &reset,
    &writeData,
    NULL,
    &capsRV40
};

static WriterCaps_t capsAVS2 = {
    "avs2",
    eVideo,
    "V_AVS2",
    VIDEO_ENCODING_AUTO,
    STREAMTYPE_AVS2,
    -1
};

struct Writer_s WriterVideoAVS2 = {
    &reset,
    &writeData,
    NULL,
    &capsAVS2
};




