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
#include <linux/dvb/stm_ioctls.h>
#include <memory.h>
#include <asm/types.h>
#include <pthread.h>
#include <errno.h>
#include <sys/uio.h>

#include "common.h"
#include "output.h"
#include "debug.h"
#include "misc.h"
#include "pes.h"
#include "writer.h"

/* ***************************** */
/* Makros/Constants              */
/* ***************************** */

#define AAC_HEADER_LENGTH       7

#ifdef SAM_WITH_DEBUG
#define AAC_DEBUG
#else
#define AAC_SILENT
#endif

#ifdef AAC_DEBUG

static short debug_level = 0;

#define aac_printf(level, fmt, x...) do { \
if (debug_level >= level) printf("[%s:%s] " fmt, __FILE__, __FUNCTION__, ## x); } while (0)
#else
#define aac_printf(level, fmt, x...)
#endif

#ifndef AAC_SILENT
#define aac_err(fmt, x...) do { printf("[%s:%s] " fmt, __FILE__, __FUNCTION__, ## x); } while (0)
#else
#define aac_err(fmt, x...)
#endif

/* ***************************** */
/* Types                         */
/* ***************************** */

/* ***************************** */
/* Varaibles                     */
/* ***************************** */

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

static int writeDataADTS(void* _call)
{
    WriterAVCallData_t* call = (WriterAVCallData_t*) _call;
    
    aac_printf(10, "\n writeDataADTS \n");

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
    if(0xFF != call->data[0] || 0xF0 != (0xF0 & call->data[1]))
    {
        aac_err("parsing Data with missing syncword. ignoring...\n");
        return 0;
    }
    
    unsigned char PesHeader[PES_MAX_HEADER_SIZE];

    aac_printf(10, "AudioPts %lld\n", call->Pts);

    unsigned int  HeaderLength = InsertPesHeader (PesHeader, call->len, MPEG_AUDIO_PES_START_CODE, call->Pts, 0);

    struct iovec iov[2];
    iov[0].iov_base = PesHeader;
    iov[0].iov_len = HeaderLength;
    iov[1].iov_base = call->data;
    iov[1].iov_len = call->len;
    
    return writev(call->fd, iov, 2);
}

static int writeData(void* _call)
{
    WriterAVCallData_t* call = (WriterAVCallData_t*) _call;
    
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
    
    if(call->private_data && 0 == strncmp("ADTS", call->private_data, call->private_size))
    {
        return writeDataADTS(_call);
    }

    uint32_t PacketLength = call->len + AAC_HEADER_LENGTH;
    uint8_t PesHeader[PES_MAX_HEADER_SIZE + AAC_HEADER_LENGTH];
    uint32_t headerSize = InsertPesHeader (PesHeader, PacketLength, MPEG_AUDIO_PES_START_CODE, call->Pts, 0);;
    uint8_t *pExtraData = &PesHeader[headerSize];
    
    aac_printf(10, "AudioPts %lld\n", call->Pts);
    if (call->private_data == NULL)
    {
        aac_printf(10, "private_data = NULL\n");
        memcpy (pExtraData, DefaultAACHeader, AAC_HEADER_LENGTH);
    }
    else
    {
        memcpy (pExtraData, call->private_data, AAC_HEADER_LENGTH);
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
    
    //PesHeader[6] = 0x81;

    struct iovec iov[2];
    iov[0].iov_base = PesHeader;
    iov[0].iov_len = headerSize + AAC_HEADER_LENGTH;
    iov[1].iov_base = call->data;
    iov[1].iov_len = call->len;
    
    return writev(call->fd, iov, 2);
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
    &writeData,
    NULL,
    &caps
};

static WriterCaps_t caps_aac_latm = {
    "aac",
    eAudio,
    "A_AAC_LATM",
    AUDIO_ENCODING_AAC,
    -1,
    -1
};

struct Writer_s WriterAudioAACLATM = {
    &reset,
    &writeData,
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
    &writeData,
    NULL,
    &caps_aacplus
};