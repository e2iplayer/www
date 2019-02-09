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
/* Includes		      */
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
/* Makros/Constants	      */
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

static uint8_t PesHeader[256];
static int writeData(void *_call, bool is_vp6, bool is_vp9)
{
    WriterAVCallData_t* call = (WriterAVCallData_t*) _call;
    vp_printf(10, "\n");

    if (call == NULL) 
    {
        vp_err("call data is NULL...\n");
        return 0;
    }

    if ((call->data == NULL) || (call->len <= 0)) 
    {
        vp_err("parsing NULL Data. ignoring...\n");
        return 0;
    }

    if (call->fd < 0) 
    {
        vp_err("file pointer < 0. ignoring ...\n");
        return 0;
    }

    vp_printf(10, "VideoPts %lld\n", call->Pts);
    vp_printf(10, "Got Private Size %d\n", call->private_size);

    struct iovec iov[2];
    uint64_t pts = is_vp9 && STB_VUPLUS == GetSTBType() ? 0 : call->Pts;
    
    iov[0].iov_base = PesHeader;
    uint32_t pes_header_len = InsertPesHeader(PesHeader, call->len, MPEG_VIDEO_PES_START_CODE, pts, 0);
    uint32_t len = call->len + 4 + 6;
    memcpy(PesHeader + pes_header_len, "BCMV", 4);
    pes_header_len += 4;
    
    if (is_vp9 && STB_VUPLUS == GetSTBType()) {
        uint32_t vp9_pts = (call->Pts == INVALID_PTS_VALUE ? call->Dts : call->Pts) / 2;
        memcpy(&PesHeader[9], &vp9_pts, sizeof(vp9_pts));
    }
    
    if (is_vp6)
            ++len;
    PesHeader[pes_header_len++] = (len & 0xFF000000) >> 24;
    PesHeader[pes_header_len++] = (len & 0x00FF0000) >> 16;
    PesHeader[pes_header_len++] = (len & 0x0000FF00) >> 8;
    PesHeader[pes_header_len++] = (len & 0x000000FF) >> 0;
    PesHeader[pes_header_len++] = 0;
    PesHeader[pes_header_len++] = STB_VUPLUS != GetSTBType() && is_vp9 ? 1 : 0;
    if (is_vp6)
            PesHeader[pes_header_len++] = 0;
    iov[0].iov_len = pes_header_len;
    iov[1].iov_base = call->data;
    iov[1].iov_len = call->len;

    int32_t payload_len = call->len + pes_header_len - 6;

    if (!is_vp9 || STB_VUPLUS == GetSTBType() || STB_HISILICON == GetSTBType() || STB_DREAMBOX == GetSTBType()) {
        UpdatePesHeaderPayloadSize(PesHeader, payload_len);
        // it looks like for VUPLUS drivers PES header must be written separately
        int ret = call->WriteV(call->fd, iov, 1);
        if (iov[0].iov_len != ret)
            return ret;
        ret = call->WriteV(call->fd, iov + 1, 1);
        return iov[0].iov_len + ret;
    }
    else {
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

        //PesHeader[0] = 0x00;
        //PesHeader[1] = 0x00;
        //PesHeader[2] = 0x01;
        //PesHeader[3] = 0xE0;
        PesHeader[4] = 0x00;
        PesHeader[5] = 0xB2;
        PesHeader[6] = 0x81;
        PesHeader[7] = 0x01;
        PesHeader[8] = 0x14;
        PesHeader[9] = 0x80;
        PesHeader[10] = 'B';
        PesHeader[11] = 'R';
        PesHeader[12] = 'C';
        PesHeader[13] = 'M';
        memset(PesHeader+14, 0, 170);
        PesHeader[26] = 0xFF;
        PesHeader[27] = 0xFF;
        PesHeader[28] = 0xFF;
        PesHeader[29] = 0xFF;
        PesHeader[33] = 0x85;

        if (pes_header_len != WriteExt(call->WriteV, call->fd, PesHeader, 184)) return -1;

        return 1;
    }

    //return call->WriteV(call->fd, iov, 2);
}

static int writeDataVP6(void *_call)
{
    return writeData(_call, true, false);
}

static int writeDataVP8(void *_call)
{
    return writeData(_call, false, false);
}

static int writeDataVP9(void *_call)
{
    return writeData(_call, false, true);
}

/* ***************************** */
/* Writer  Definition            */
/* ***************************** */

static WriterCaps_t capsVP6 = {
    "vp6",
    eVideo,
    "V_VP6",
    VIDEO_ENCODING_VC1,
    STREAMTYPE_VB6,
    CT_VP6
};

struct Writer_s WriterVideoVP6 = {
    &reset,
    &writeDataVP6,
    NULL,
    &capsVP6
};

static WriterCaps_t capsVP8 = {
    "vp8",
    eVideo,
    "V_VP8",
    VIDEO_ENCODING_VC1,
    STREAMTYPE_VB8,
    CT_VP8
};

struct Writer_s WriterVideoVP8 = {
    &reset,
    &writeDataVP8,
    NULL,
    &capsVP8
};

static WriterCaps_t capsVP9 = {
    "vp9",
    eVideo,
    "V_VP9",
    VIDEO_ENCODING_VC1,
    STREAMTYPE_VB9,
    CT_VP9
};

struct Writer_s WriterVideoVP9 = {
    &reset,
    &writeDataVP9,
    NULL,
    &capsVP9
};

static WriterCaps_t capsFLV= {
    "flv1",
    eVideo,
    "V_FLV",
    VIDEO_ENCODING_VC1,
    STREAMTYPE_SPARK,
    CT_SPARK
};

struct Writer_s WriterVideoFLV = {
    &reset,
    &writeDataVP8,
    NULL,
    &capsFLV
};
