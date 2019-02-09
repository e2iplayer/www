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

#include <libavcodec/avcodec.h>

#include "stm_ioctls.h"
#include "bcm_ioctls.h"

#include "common.h"
#include "output.h"
#include "debug.h"
#include "misc.h"
#include "pes.h"
#include "writer.h"
#include "pcm.h"
#include "ffmpeg/xiph.h"

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

static int writeData(void *_call)
{
    WriterAVCallData_t *call = (WriterAVCallData_t*) _call;

    if (call == NULL || call->data == NULL || call->len <= 0 || call->fd < 0 || \
        !call->private_data || call->private_size != sizeof(pcmPrivateData_t)) {
        bcma_err("Wrong input call: %p, data: %p, len: %d, fd: %d\n", call, call->data, call->len, call->fd);
        return 0;
    }

    bcma_printf(10, "AudioPts %lld\n", call->Pts);
    uint8_t PesHeader[PES_MAX_HEADER_SIZE + 22];
    uint32_t i;
    uint32_t private_size = 0;
    const uint8_t *vorbis_header_start[3];
    int vorbis_header_len[3];
    uint8_t vorbis_header_len_raw[3][2];
    pcmPrivateData_t *pcmPrivateData  = (pcmPrivateData_t*)call->private_data;

    uint32_t headerSize = InsertPesHeader(PesHeader, call->len, MPEG_AUDIO_PES_START_CODE, call->Pts, 0);
    
    if (pcmPrivateData->codec_id == AV_CODEC_ID_VORBIS) {

        if (avpriv_split_xiph_headers(pcmPrivateData->private_data, pcmPrivateData->private_size, 30, vorbis_header_start, vorbis_header_len) < 0) {
            bcma_err("Wrong VORBIS codec data : %p, len: %d\n", pcmPrivateData->private_data, pcmPrivateData->private_size);
            return -1;
        }

        for (i=0; i<3; ++i) {
            vorbis_header_len_raw[i][0] = (vorbis_header_len[i] >> 8) & 0xff;
            vorbis_header_len_raw[i][1] = vorbis_header_len[i] & 0xff;
            private_size += 2 + vorbis_header_len[i];
        }
    }
    else {
        private_size = pcmPrivateData->private_size;
    }

    if( STB_DREAMBOX == GetSTBType() ) {
        PesHeader[headerSize++] = 'B';
        PesHeader[headerSize++] = 'C';
        PesHeader[headerSize++] = 'M';
        PesHeader[headerSize++] = 'A';
    }

    if (pcmPrivateData->codec_id != AV_CODEC_ID_VORBIS || pcmPrivateData->codec_id != AV_CODEC_ID_OPUS || STB_HISILICON != GetSTBType()) {
        uint32_t payloadSize = call->len;
        PesHeader[headerSize++] = (payloadSize >> 24) & 0xFF;
        PesHeader[headerSize++] = (payloadSize >> 16) & 0xFF;
        PesHeader[headerSize++] = (payloadSize >> 8) & 0xFF;
        PesHeader[headerSize++] = payloadSize & 0xFF;

        int32_t channels        = pcmPrivateData->channels;
        uint32_t sample_rate    = pcmPrivateData->sample_rate;

        int32_t bits_per_sample = pcmPrivateData->bits_per_coded_sample;
        uint32_t byte_rate      = pcmPrivateData->bit_rate / 8;
        uint32_t block_align    = pcmPrivateData->block_align;

        int32_t format_tag;
        switch(pcmPrivateData->codec_id)
        {
        case AV_CODEC_ID_WMAV1:
            format_tag = 0x160;
            break;
        case AV_CODEC_ID_WMAV2:
            format_tag = 0x161;
            break;
        case AV_CODEC_ID_WMAPRO:
            format_tag = 0x162;
            break;
        case AV_CODEC_ID_WMALOSSLESS:
            format_tag = 0x163;
            break;
        case AV_CODEC_ID_VORBIS:
            bits_per_sample = 8;
            byte_rate = 32000;
            block_align = 1;
        default:
            format_tag = 0xFFFF;
            break;
        }

        /* format tag */
        PesHeader[headerSize++] = format_tag & 0xff;
        PesHeader[headerSize++] = (format_tag >> 8) & 0xff;

        /* channels */
        PesHeader[headerSize++] = channels & 0xff;
        PesHeader[headerSize++] = (channels >> 8) & 0xff;

        /* sample rate */
        PesHeader[headerSize++] = sample_rate & 0xff;
        PesHeader[headerSize++] = (sample_rate >> 8) & 0xff;
        PesHeader[headerSize++] = (sample_rate >> 16) & 0xff;
        PesHeader[headerSize++] = (sample_rate >> 24) & 0xff;

        /* byte rate */
        PesHeader[headerSize++] = byte_rate & 0xff;
        PesHeader[headerSize++] = (byte_rate >> 8) & 0xff;
        PesHeader[headerSize++] = (byte_rate >> 16) & 0xff;
        PesHeader[headerSize++] = (byte_rate >> 24) & 0xff;

        /* block align */
        PesHeader[headerSize++] = block_align & 0xff;
        PesHeader[headerSize++] = (block_align >> 8) & 0xff;

        /* bits per sample */
        PesHeader[headerSize++] = bits_per_sample & 0xff;
        PesHeader[headerSize++] = (bits_per_sample >> 8) & 0xff;

        /* Codec Specific Data Size */
        PesHeader[headerSize++] = private_size & 0xff;
        PesHeader[headerSize++] = (private_size >> 8) & 0xff;
    }

    PesHeader[6] |= 1;
    UpdatePesHeaderPayloadSize(PesHeader, headerSize - 6 + private_size + call->len);

    struct iovec iov[5];

    i = 0;
    iov[i].iov_base = PesHeader;
    iov[i++].iov_len  = headerSize;

    if (private_size > 0) {
        if (pcmPrivateData->codec_id == AV_CODEC_ID_VORBIS) {
            for (i=0; i<3; ++i) {
                iov[i].iov_base = vorbis_header_len_raw[i];
                iov[i++].iov_len  = 2;

                iov[i].iov_base = vorbis_header_start;
                iov[i++].iov_len  = vorbis_header_len[i];
            }
        }
        else {
            iov[i].iov_base = pcmPrivateData->private_data;
            iov[i++].iov_len  = private_size;
        }
    }

    iov[i].iov_base = call->data;
    iov[i++].iov_len  = call->len;

    return call->WriteV(call->fd, iov, i);
}

/* ***************************** */
/* Writer Definition            */
/* ***************************** */

static WriterCaps_t capsVORBIS = {
    "vorbis",
    eAudio,
    "A_VORBIS",
    -1,
    AUDIOTYPE_VORBIS,
    -1
};

struct Writer_s WriterAudioVORBIS = {
    &reset,
    &writeData,
    NULL,
    &capsVORBIS
};

static WriterCaps_t capsOPUS = {
    "opus",
    eAudio,
    "A_OPUS",
    -1,
    AUDIOTYPE_OPUS,
    -1
};

struct Writer_s WriterAudioOPUS = {
    &reset,
    &writeData,
    NULL,
    &capsOPUS
};


static WriterCaps_t capsWMAPRO = {
    "wma/pro",
    eAudio,
    "A_WMA/PRO",
    -1,
    AUDIOTYPE_WMA_PRO,
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
    -1,
    AUDIOTYPE_WMA,
    -1
};

struct Writer_s WriterAudioWMA = {
    &reset,
    &writeData,
    NULL,
    &capsWMA
};
