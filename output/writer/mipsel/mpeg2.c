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

    mpeg2_printf(10, "\n");

    if (call == NULL)
    {
        mpeg2_err("call data is NULL...\n");
        return 0;
    }

    mpeg2_printf(10, "VideoPts %lld\n", call->Pts);

    if ((call->data == NULL) || (call->len <= 0))
    {
        mpeg2_err("parsing NULL Data. ignoring...\n");
        return 0;
    }

    if (call->fd < 0)
    {
        mpeg2_err("file pointer < 0. ignoring ...\n");
        return 0;
    }

    uint8_t *data = call->data;
    uint32_t data_len = call->len;

    if (!private_data && !call->private_data && data_len > 3 && !memcmp(data, "\x00\x00\x01\xb3", 4))
    {
        bool ok = true;
        uint32_t pos = 4;
        uint32_t sheader_data_len = 0;
        while (pos < data_len && ok)
        {
            if (pos >= data_len) break;
            pos += 7;
            if (pos >=data_len) break;
            sheader_data_len = 12;
            if (data[pos] & 2)
            { // intra matrix
                pos += 64;
                if (pos >=data_len) break;
                sheader_data_len += 64;
            }
            if (data[pos] & 1)
            { // non intra matrix
                pos += 64;
                if (pos >=data_len) break;
                sheader_data_len += 64;
            }
            pos += 1;
            if (pos + 3 >=data_len) break;
            if (!memcmp(&data[pos], "\x00\x00\x01\xb5", 4))
            {
                // extended start code
                pos += 3;
                sheader_data_len += 3;
                do
                {
                    pos += 1;
                    ++sheader_data_len;
                    if (pos + 2 > data_len)
                    {
                        ok = false;
                        break;
                    }
                } while (memcmp(&data[pos], "\x00\x00\x01", 3));
                if (!ok) break;
            }
            if (pos + 3 >= data_len) break;
            if (!memcmp(&data[pos], "\x00\x00\x01\xb2", 4))
            {
                // private data
                pos += 3;
                sheader_data_len += 3;
                do
                {
                    pos += 1;
                    ++sheader_data_len;
                    if (pos + 2 > data_len)
                    {
                        ok = false;
                        break;
                    }
                } while (memcmp(&data[pos], "\x00\x00\x01", 3));
                if (!ok) break;
            }

            free(private_data);
            private_data = malloc(sheader_data_len);
            if (private_data)
            {
                private_size = sheader_data_len;
                memcpy(private_data, data + pos - sheader_data_len, sheader_data_len);
            }
            must_send_header = false;
            break;
        }
    }
    else if ((private_data || call->private_data) && must_send_header)
    {
        uint8_t *codec_data = NULL;
        uint32_t codec_data_size = 0;
        int pos = 0;

        if (private_data) {
            codec_data = private_data;
            codec_data_size = private_size;
        }
        else {
            codec_data = call->private_data;
            codec_data_size = call->private_size;
        }

        while (pos <= data_len - 4)
        {
            if (memcmp(&data[pos], "\x00\x00\x01\xb8", 4)) /* find group start code */
            {
                pos++;
                continue;
            }

            struct iovec iov[4];
            iov[0].iov_base = PesHeader;
            iov[0].iov_len = InsertPesHeader(PesHeader, call->len + codec_data_size, MPEG_VIDEO_PES_START_CODE, call->Pts, 0);

            iov[1].iov_base = data;
            iov[1].iov_len = pos;

            iov[2].iov_base = codec_data;
            iov[2].iov_len = codec_data_size;

            iov[3].iov_base = data + pos;
            iov[3].iov_len = data_len - pos;

            must_send_header = false;
            return call->WriteV(call->fd, iov, 4);
        }
    }

    struct iovec iov[2];

    iov[0].iov_base = PesHeader;
    iov[0].iov_len = InsertPesHeader(PesHeader, call->len, MPEG_VIDEO_PES_START_CODE, call->Pts, 0);

    iov[1].iov_base = data;
    iov[1].iov_len = data_len;

    PesHeader[6] = 0x81;
    
    UpdatePesHeaderPayloadSize(PesHeader, data_len + iov[0].iov_len - 6);
    if (iov[0].iov_len != WriteExt(call->WriteV, call->fd, iov[0].iov_base, iov[0].iov_len)) return -1;
    if (iov[1].iov_len != WriteExt(call->WriteV, call->fd, iov[1].iov_base, iov[1].iov_len)) return -1;

    return 1;
}

/* ***************************** */
/* Writer  Definition            */
/* ***************************** */
static WriterCaps_t caps = {
    "mpeg2",
    eVideo,
    "V_MPEG2",
    VIDEO_ENCODING_AUTO,
    STREAMTYPE_MPEG2,
    -1
};

struct Writer_s WriterVideoMPEG2 = {
    &reset,
    &writeData,
    NULL,
    &caps
};

static WriterCaps_t mpg1_caps = {
    "mpge1",
    eVideo,
    "V_MPEG1",
    VIDEO_ENCODING_AUTO,
    STREAMTYPE_MPEG1,
    -1
};

struct Writer_s WriterVideoMPEG1 = {
    &reset,
    &writeData,
    NULL,
    &mpg1_caps
};
