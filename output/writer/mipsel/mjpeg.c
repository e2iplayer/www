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
static bool must_send_header = true;
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
    must_send_header = true;
    return 0;
}

static int writeData(void* _call)
{
    WriterAVCallData_t* call = (WriterAVCallData_t*) _call;

    static uint8_t PesHeader[PES_MAX_HEADER_SIZE];
    int32_t len = 0;
    uint32_t Position = 0;

    mjpeg_printf(10, "\n");

    if (call == NULL)
    {
        mjpeg_err("call data is NULL...\n");
        return 0;
    }

    mjpeg_printf(10, "VideoPts %lld\n", call->Pts);

    struct iovec iov[2];

    iov[0].iov_base = PesHeader;
    iov[0].iov_len = InsertPesHeader(PesHeader, call->len, MPEG_VIDEO_PES_START_CODE, call->Pts, 0);

    iov[1].iov_base = call->data;
    iov[1].iov_len = call->len;

    return call->WriteV(call->fd, iov, 2);;
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
