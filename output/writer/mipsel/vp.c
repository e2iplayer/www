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
//#define SAM_WITH_DEBUG
#ifdef SAM_WITH_DEBUG
#define VP_DEBUG
#else
#define VP_SILENT
#endif

#ifdef VP_DEBUG

static short debug_level = 10;

#define vp_printf(level, fmt, x...) do { \
if (debug_level >= level) printf("[%s:%s] " fmt, __FILE__, __FUNCTION__, ## x); } while (0)
#else
#define vp_printf(level, fmt, x...)
#endif

#ifndef VP_SILENT
#define vp_err(fmt, x...) do { printf("[%s:%s] " fmt, __FILE__, __FUNCTION__, ## x); } while (0)
#else
#define vp_err(fmt, x...)
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

static int writeData(void* _call, int is_vp6)
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
    
    unsigned char PesHeader[PES_MAX_HEADER_SIZE];
    struct iovec iov[2];
    
    iov[0].iov_base = PesHeader;
    uint32_t pes_header_len = InsertPesHeader(PesHeader, call->len, MPEG_VIDEO_PES_START_CODE, call->Pts, 0);
    uint32_t len = call->len + 4 + 6;
    memcpy(PesHeader + pes_header_len, "BCMV", 4);
    pes_header_len += 4;
    if (is_vp6)
            ++len;
    PesHeader[pes_header_len++] = (len & 0xFF000000) >> 24;
    PesHeader[pes_header_len++] = (len & 0x00FF0000) >> 16;
    PesHeader[pes_header_len++] = (len & 0x0000FF00) >> 8;
    PesHeader[pes_header_len++] = (len & 0x000000FF) >> 0;
    PesHeader[pes_header_len++] = 0;
    PesHeader[pes_header_len++] = 0;
    if (is_vp6)
            PesHeader[pes_header_len++] = 0;
    iov[0].iov_len = pes_header_len;
    iov[1].iov_base = call->data;
    iov[1].iov_len = call->len;
    
    return writev_with_retry(call->fd, iov, 2);
}

static int writeDataVP6(void *_call)
{
    return writeData(_call, 1);
}

static int writeDataVP89(void *_call)
{
    return writeData(_call, 0);
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
    &writeDataVP89,
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
    &writeDataVP89,
    NULL,
    &capsVP9
};

static WriterCaps_t capsSPARK = {
    "spark",
    eVideo,
    "V_SPARK",
    VIDEO_ENCODING_VC1,
    STREAMTYPE_SPARK,
    CT_SPARK
};

struct Writer_s WriterVideoSPARK = {
    &reset,
    &writeDataVP89,
    NULL,
    &capsSPARK
};
