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

#define _XOPEN_SOURCE 
#include <unistd.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
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

/* ***************************** */
/* Makros/Constants              */
/* ***************************** */
#define LLPCM_VOB_HEADER_LEN (6)

/* ***************************** */
/* Types                         */
/* ***************************** */

/* ***************************** */
/* Varaibles                     */
/* ***************************** */
static uint8_t PesHeader[PES_MAX_HEADER_SIZE];
static uint8_t initialHeader = 1;

static uint8_t i_freq_code = 0;
static int32_t i_frame_samples;
static int32_t i_frame_size;
static int32_t i_buffer_used;
static int32_t i_frame_num;
static int32_t i_bitspersample;
static uint8_t *p_buffer = 0;
static uint8_t *p_frame_buffer = 0;
/* ***************************** */
/* Prototypes                    */
/* ***************************** */

/* ***************************** */
/* MISC Functions                */
/* ***************************** */

/* https://www.videolan.org/developers/vlc/modules/codec/lpcm.c
 * LPCM DVD header :
 * - number of frames in this packet (8 bits)
 * - first access unit (16 bits) == 0x0003 ?
 * - emphasis (1 bit)
 * - mute (1 bit)
 * - reserved (1 bit)
 * - current frame (5 bits)
 * - quantisation (2 bits) 0 == 16bps, 1 == 20bps, 2 == 24bps, 3 == illegal
 * - frequency (2 bits) 0 == 48 kHz, 1 == 96 kHz, 2 == 44.1 kHz, 3 == 32 kHz
 * - reserved (1 bit)
 * - number of channels - 1 (3 bits) 1 == 2 channels
 * - dynamic range (8 bits) 0x80 == neutral
 */
 
static int32_t reset()
{
    initialHeader = 1;
    return 0;
}

static int32_t writeData(void *_call)
{
    WriterAVCallData_t* call = (WriterAVCallData_t*) _call;

    lpcm_printf(10, "\n");

    if (!call)
    {
        lpcm_err("call data is NULL...\n");
        return 0;
    }

    lpcm_printf(10, "AudioPts %lld\n", call->Pts);

    if (!call->data || (call->len <= 0))
    {
        lpcm_err("parsing NULL Data. ignoring...\n");
        return 0;
    }

    if (call->fd < 0)
    {
        lpcm_err("file pointer < 0. ignoring ...\n");
        return 0;
    }
    
    pcmPrivateData_t *pcmPrivateData = (pcmPrivateData_t*)call->private_data;
    
    int32_t i_rate     = (int32_t)pcmPrivateData->sample_rate;
    int32_t i_channels = (int32_t)pcmPrivateData->channels;
    int32_t i_nb_samples = call->len / (i_channels * 2);
    int32_t i_ret_size = 0;
    if( i_channels > 8 )
    {
        lpcm_err("Error DVD LPCM supports a maximum of eight channels i_channels[%d]\n", i_channels);
        return 0;
    }
        
    if( pcmPrivateData->bResampling || NULL == p_buffer )
    {
        lpcm_printf(1, "i_rate: [%d]\n", i_rate);
        lpcm_printf(1, "i_channels: [%d]\n", i_channels);
        switch( i_rate ) 
        {
        case 48000:
            i_freq_code = 0;
            break;
        case 96000:
            i_freq_code = 1;
            break;
        case 44100:
            i_freq_code = 2;
            break;
        case 32000:
            i_freq_code = 3;
            break;
        default:
            lpcm_err("Error DVD LPCM sample_rate not supported [%d]\n", i_rate);
            return 0;
        }
        
        /* In DVD LCPM, a frame is always 150 PTS ticks. */
        i_frame_samples = i_rate * 150 / 90000;
        i_frame_size = i_frame_samples * i_channels * 2 + LLPCM_VOB_HEADER_LEN;
        if(NULL != p_buffer)
        {
            free(p_buffer);
        }
        p_buffer = malloc(i_frame_samples * i_channels * 16);
        
        if(NULL != p_frame_buffer)
        {
            free(p_frame_buffer);
        }
        p_frame_buffer = malloc(i_frame_size);
        i_buffer_used = 0;
        i_frame_num = 0;
        i_bitspersample = 16;
    }
    
    const int i_num_frames = ( i_buffer_used + i_nb_samples ) / i_frame_samples;
    const int i_leftover_samples = ( i_buffer_used + i_nb_samples ) % i_frame_samples;
    const int i_start_offset = -i_buffer_used;
    
    int32_t i_bytes_consumed = 0;
    int32_t i = 0;
    for ( i = 0; i < i_num_frames; ++i )
    {

        uint8_t *frame = (uint8_t *)p_frame_buffer;
        frame[0] = 1;  /* one frame in packet */
        frame[1] = 0;
        frame[2] = 0;  /* no first access unit */
        frame[3] = (i_frame_num + i) & 0x1f;  /* no emphasis, no mute */
        frame[4] = (i_freq_code << 4) | (i_channels - 1);
        frame[5] = 0x80;  /* neutral dynamic range */

        const int i_consume_samples = i_frame_samples - i_buffer_used;
        const int i_kept_bytes = i_buffer_used * i_channels * 2;
        const int i_consume_bytes = i_consume_samples * i_channels * 2;

#ifdef WORDS_BIGENDIAN
        memcpy( frame + 6, p_buffer, i_kept_bytes );
        memcpy( frame + 6 + i_kept_bytes, call->data + i_bytes_consumed, i_consume_bytes );
#else
       swab( p_buffer, frame + 6, i_kept_bytes );
       swab( call->data + i_bytes_consumed, frame + 6 + i_kept_bytes, i_consume_bytes );
#endif

        i_frame_num++;
        i_buffer_used = 0;
        i_bytes_consumed += i_consume_bytes;

        /* We need to find i_length by means of next_pts due to possible roundoff errors. */
        uint64_t this_pts = call->Pts + (i * i_frame_samples + i_start_offset) * 90000 / i_rate;

        uint32_t pes_header_size = 0;
        pes_header_size = InsertPesHeader(PesHeader, i_frame_size+1, MPEG_AUDIO_PES_START_CODE, this_pts, 0);

        PesHeader[pes_header_size] = 0xa0;
        pes_header_size += 1;
        
        struct iovec iov[2];
        iov[0].iov_base = PesHeader;
        iov[0].iov_len  = pes_header_size;
        iov[1].iov_base = frame;
        iov[1].iov_len  = i_frame_size;
        i_ret_size += call->WriteV(call->fd, iov, 2);
    }
    
    memcpy( p_buffer, call->data + i_bytes_consumed, i_leftover_samples * i_channels * 2 );
    i_buffer_used = i_leftover_samples;

    return i_ret_size;
}

/* ***************************** */
/* Writer  Definition            */
/* ***************************** */

static WriterCaps_t caps_lpcm = 
{
    "ipcm",
    eAudio,
    "A_LPCM",
    AUDIO_ENCODING_LPCMA,
    AUDIOTYPE_LPCM,
    -1
};

struct Writer_s WriterAudioLPCM = 
{
    &reset,
    &writeData, /* writeDataLPCM */
    NULL,
    &caps_lpcm
};
