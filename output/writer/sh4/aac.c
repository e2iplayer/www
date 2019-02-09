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
#include <linux/dvb/video.h>
#include <linux/dvb/audio.h>
#include <memory.h>
#include <asm/types.h>
#include <pthread.h>
#include <errno.h>
#include <sys/uio.h>

#include <libavutil/intreadwrite.h>
#include "ffmpeg/latmenc.h"
#include "ffmpeg/mpeg4audio.h"
#include "stm_ioctls.h"

#include "common.h"
#include "output.h"
#include "debug.h"
#include "misc.h"
#include "pes.h"
#include "writer.h"
#include "aac.h"

/* ***************************** */
/* Makros/Constants              */
/* ***************************** */

/* ***************************** */
/* Types                         */
/* ***************************** */

/* ***************************** */
/* Varaibles                     */
/* ***************************** */
static bool needInitHeader = true;

/// ** AAC ADTS format **
///
/// AAAAAAAA AAAABCCD EEFFFFGH HHIJKLMM
/// MMMMMMMM MMMNNNNN NNNNNNOO ........
///
/// Sign            Length          Position         Description
///
/// A                12             (31-20)          Sync code
/// B                 1              (19)            ID
/// C                 2             (18-17)          layer
/// D                 1              (16)            protect absent
/// E                 2             (15-14)          profile
/// F                 4             (13-10)          sample freq index
/// G                 1              (9)             private
/// H                 3             (8-6)            channel config
/// I                 1              (5)             original/copy
/// J                 1              (4)             home
/// K                 1              (3)             copyright id
/// L                 1              (2)             copyright start
/// M                 13         (1-0,31-21)         frame length
/// N                 11           (20-10)           adts buffer fullness
/// O                 2             (9-8)            num of raw data blocks in frame

/*
LC: Audio: aac, 44100 Hz, stereo, s16, 192 kb/ ->ff f1 50 80 00 1f fc
HE: Audio: aac, 48000 Hz, stereo, s16, 77 kb/s ->ff f1 4c 80 00 1f fc
*/

/*
ADIF = basic format called Audio Data Interchange Format (ADIF)
       consisting of a single header followed by the raw AAC audio data blocks
ADTS = streaming format called Audio Data Transport Stream (ADTS)
       consisting of a series of frames, each frame having a header followed by the AAC audio data
LOAS = Low Overhead Audio Stream (LOAS), a self-synchronizing streaming format
*/

static unsigned char DefaultAACHeader[]    =  {
    0xff,
    0xf1,
    /*0x00, 0x00*/0x50,  //((Profile & 0x03) << 6)  | (SampleIndex << 2) | ((Channels >> 2) & 0x01);s
    0x80,                //(Channels & 0x03) << 6;
    0x00,
    0x1f,
    0xfc
};

LATMContext *pLATMCtx = NULL;

/* ***************************** */
/* Prototypes                    */
/* ***************************** */

/* ***************************** */
/* MISC Functions                */
/* ***************************** */

static int reset()
{
    if (pLATMCtx)
    {
        free(pLATMCtx);
        pLATMCtx = NULL;
    }
    needInitHeader = true;
    return 0;
}

static int _writeData(void *_call, int type)
{
    WriterAVCallData_t* call = (WriterAVCallData_t*) _call;
    
    aac_printf(10, "\n _writeData type[%d]\n", type);

    if (call == NULL)
    {
        aac_err("call data is NULL...\n");
        return 0;
    }
    
    if ((call->data == NULL) || (call->len < 8))
    {
        aac_err("parsing Data with missing AAC header. ignoring...\n");
        return 0;
    }
    
    /* simple validation */
    if (0 == type) // check ADTS header
    {
        if (0xFF != call->data[0] || 0xF0 != (0xF0 & call->data[1]))
        {
            aac_err("parsing Data with missing syncword. ignoring...\n");
            return 0;
        }
        
        // STB can handle only AAC LC profile
        if (0 == (call->data[2] & 0xC0))
        {
            // change profile AAC Main -> AAC LC (Low Complexity)
            aac_printf(1, "change profile AAC Main -> AAC LC (Low Complexity) in the ADTS header");
            call->data[2] = (call->data[2] & 0x1F) | 0x40;
        }
    }
    else // check LOAS header
    {
        if( !(call->len > 2 && call->data[0] == 0x56 && (call->data[1] >> 4) == 0xe &&
            (AV_RB16(call->data + 1) & 0x1FFF) + 3 == call->len))
        {
            aac_err("parsing Data with wrong latm header. ignoring...\n");
            return 0;
        }
    }
    
    unsigned char PesHeader[PES_MAX_HEADER_SIZE];

    aac_printf(10, "AudioPts %lld\n", call->Pts);

    unsigned int  HeaderLength = InsertPesHeader (PesHeader, call->len, MPEG_AUDIO_PES_START_CODE, call->Pts, 0);

    struct iovec iov[2];
    iov[0].iov_base = PesHeader;
    iov[0].iov_len = HeaderLength;
    iov[1].iov_base = call->data;
    iov[1].iov_len = call->len;
    return call->WriteV(call->fd, iov, 2);
}

static int writeDataADTS(void *_call)
{
    WriterAVCallData_t *call = (WriterAVCallData_t *) _call;
    
    aac_printf(10, "\n");

    if (call == NULL)
    {
        aac_err("call data is NULL...\n");
        return 0;
    }
    
    if ((call->data == NULL) || (call->len <= 0))
    {
        aac_err("parsing NULL Data. ignoring...\n");
        return 0;
    }

    if (call->fd < 0)
    {
        aac_err("file pointer < 0. ignoring ...\n");
        return 0;
    }
    
    if( (call->private_data && 0 == strncmp("ADTS", call->private_data, call->private_size)) || 
        HasADTSHeader(call->data, call->len) )
    {
        return _writeData(_call, 0);
    }

    uint32_t adtsHeaderSize = (call->private_data == NULL || needInitHeader == false) ? AAC_HEADER_LENGTH : call->private_size;
    uint32_t PacketLength = call->len + adtsHeaderSize;
    uint8_t PesHeader[PES_MAX_HEADER_SIZE + AAC_HEADER_LENGTH + MAX_PCE_SIZE];
    uint32_t headerSize = InsertPesHeader (PesHeader, PacketLength, MPEG_AUDIO_PES_START_CODE, call->Pts, 0);
    uint8_t *pExtraData = &PesHeader[headerSize];

    needInitHeader = false;
    aac_printf(10, "AudioPts %lld\n", call->Pts);
    if (call->private_data == NULL) {
        aac_printf(10, "private_data = NULL\n");
        memcpy (pExtraData, DefaultAACHeader, AAC_HEADER_LENGTH);
    }
    else {
        memcpy (pExtraData, call->private_data, adtsHeaderSize);
    }

    pExtraData[3] &= 0xC0;
    /* frame size over last 2 bits */
    pExtraData[3] |= (PacketLength & 0x1800) >> 11;
    /* frame size continued over full byte */
    pExtraData[4] = (PacketLength & 0x1FF8) >> 3;
    /* frame size continued first 3 bits */
    pExtraData[5] = (PacketLength & 7) << 5;
    /* buffer fullness(0x7FF for VBR) over 5 last bits */
    pExtraData[5] |= 0x1F;
    /* buffer fullness(0x7FF for VBR) continued over 6 first bits + 2 zeros for
     * number of raw data blocks */
    pExtraData[6] = 0xFC;

    struct iovec iov[2];
    iov[0].iov_base = PesHeader;
    iov[0].iov_len = headerSize + adtsHeaderSize;
    iov[1].iov_base = call->data;
    iov[1].iov_len = call->len;
    
    return call->WriteV(call->fd, iov, 2);
}

static int writeDataLATM(void *_call)
{
    WriterAVCallData_t *call = (WriterAVCallData_t *) _call;
    
    aac_printf(10, "\n");

    if (call == NULL)
    {
        aac_err("call data is NULL...\n");
        return 0;
    }
    
    if ((call->data == NULL) || (call->len <= 0))
    {
        aac_err("parsing NULL Data. ignoring...\n");
        return 0;
    }
    
    if( call->private_data && 0 == strncmp("LATM", call->private_data, call->private_size))
    {
        return _writeData(_call, 1);
    }

    aac_printf(10, "AudioPts %lld\n", call->Pts);
    
    if (!pLATMCtx)
    {
        pLATMCtx = malloc(sizeof(LATMContext));
        memset(pLATMCtx, 0x00, sizeof(LATMContext));
        pLATMCtx->mod = 14;
        pLATMCtx->counter = 0;
    }
    
    if (!pLATMCtx)
    {
        aac_err("parsing NULL pLATMCtx. ignoring...\n");
        return 0;
    }
    
    unsigned char PesHeader[PES_MAX_HEADER_SIZE];
    int ret = latmenc_decode_extradata(pLATMCtx, call->private_data, call->private_size);
    if (ret)
    {
        //printf("%02x %02x %02x %02x %02x %02x %02x %02x\n", (int)call->data[0], (int)call->data[1], (int)call->data[2], (int)call->data[3],\
        //                                                    (int)call->data[4], (int)call->data[5], (int)call->data[6], (int)call->data[7]);
        aac_err("latm_decode_extradata failed. ignoring...\n");
        return 0;
    }
    ret = latmenc_write_packet(pLATMCtx, call->data, call->len, call->private_data, call->private_size);
    if (ret)
    {
        aac_err("latm_write_packet failed. ignoring...\n");
        return 0;
    }
    
    unsigned int  HeaderLength = InsertPesHeader (PesHeader,  pLATMCtx->len + 3, MPEG_AUDIO_PES_START_CODE, call->Pts, 0);

    struct iovec iov[3];
    iov[0].iov_base = PesHeader;
    iov[0].iov_len  = HeaderLength;
    
    iov[1].iov_base = pLATMCtx->loas_header;
    iov[1].iov_len  = 3;
    
    iov[2].iov_base = pLATMCtx->buffer;
    iov[2].iov_len  = pLATMCtx->len;
    
    return call->WriteV(call->fd, iov, 3);
}

/* ***************************** */
/* Writer  Definition            */
/* ***************************** */

static WriterCaps_t caps = {
    "aac",
    eAudio,
    "A_AAC",
    AUDIO_ENCODING_AAC,
    -1,
    -1
};

struct Writer_s WriterAudioAAC = {
    &reset,
    &writeDataADTS,
    NULL,
    &caps
};

static WriterCaps_t caps_aac_latm = {
    "aac",
    eAudio,
    "A_AAC_LATM",
    AUDIO_ENCODING_AAC,
    -1, // it is some misunderstanding, this should be AUDIOTYPE_AAC_LATM
    -1
};

struct Writer_s WriterAudioAACLATM = {
    &reset,
    &writeDataLATM,
    NULL,
    &caps_aac_latm
};

static WriterCaps_t caps_aacplus = {
    "aac",
    eAudio,
    "A_AAC_PLUS",
    AUDIO_ENCODING_AAC,
    -1,
    -1
};

struct Writer_s WriterAudioAACPLUS = {
    &reset,
    &writeDataADTS,
    NULL,
    &caps_aacplus
};