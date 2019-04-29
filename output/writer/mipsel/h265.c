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
#include <assert.h>
#include <stdint.h>
#include <poll.h>

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
#define IOVEC_SIZE                                      128

/* ***************************** */
/* Types                         */
/* ***************************** */

/* ***************************** */
/* Varaibles                     */
/* ***************************** */
static unsigned char           Head[] = {0, 0, 0, 1};
static int                     initialHeader = 1;
static unsigned int            NalLengthBytes = 1;
static unsigned char           *CodecData     = NULL; 
static unsigned int            CodecDataLen   = 0;
/* ***************************** */
/* Prototypes                    */
/* ***************************** */

/* ***************************** */
/* MISC Functions                */
/* ***************************** */

static int32_t PreparCodecData(unsigned char *data, unsigned int cd_len, unsigned int *NalLength)
{
    h265_printf(10, "H265 check codec data..!\n");
    int32_t ret = -100;

    if (data)
    {
        unsigned char tmp[2048];
        unsigned int tmp_len = 0;

        h265_printf(10, "H265 have codec data..!");

        if (cd_len > 3 && (data[0] || data[1] || data[2] > 1))
        {
            if (cd_len > 22)
            {
                int i;
                if (data[0] != 0) 
                {
                    h265_printf(10, "Unsupported extra data version %d, decoding may fail", (int)data[0]);
                }
                
                *NalLength = (data[21] & 3) + 1;
                int num_param_sets = data[22];
                int pos = 23;
                for (i = 0; i < num_param_sets; i++)
                {
                    int j;
                    if (pos + 3 > cd_len) 
                    {
                        h265_printf(10, "Buffer underrun in extra header (%d >= %u)", pos + 3, cd_len);
                        break;
                    }
                    // ignore flags + NAL type (1 byte)
                    int nal_type = data[pos] & 0x3f;
                    int nal_count = data[pos + 1] << 8 | data[pos + 2];
                    pos += 3;
                    for (j = 0; j < nal_count; j++)
                    {
                        if (pos + 2 > cd_len)
                        {
                            h265_printf(10, "Buffer underrun in extra nal header (%d >= %u)\n", pos + 2, cd_len);
                            break;
                        }
                        int nal_size = data[pos] << 8 | data[pos + 1];
                        pos += 2;
                        if (pos + nal_size > cd_len)
                        {
                            h265_printf(10, "Buffer underrun in extra nal (%d >= %u)\n", pos + 2 + nal_size, cd_len);
                            break;
                        }
                        if ((nal_type == 0x20 || nal_type == 0x21 || nal_type == 0x22) && ((tmp_len + 4 + nal_size) < sizeof(tmp) )) // use only VPS, SPS, PPS nals
                        {
                            memcpy(tmp+tmp_len, "\x00\x00\x00\x01", 4);
                            tmp_len += 4;
                            memcpy(tmp + tmp_len, data + pos, nal_size);
                            tmp_len += nal_size;
                        }
                        else if ((tmp_len + 4 + nal_size) >= sizeof(tmp))
                        {
                            h264_err("Ignoring nal as tmp buffer is too small tmp_len + nal = %d\n", tmp_len + 4 + nal_size);
                        }
                        pos += nal_size;
                    }
                }
                
                CodecData = malloc(tmp_len);
                memcpy(CodecData, tmp, tmp_len);
                CodecDataLen = tmp_len;
            }
        }
    }
    else
    {
        *NalLength = 0;
    }
    
    return ret;
}

static int reset()
{
    initialHeader = 1;
    return 0;
}

static int writeData(void* _call)
{
    WriterAVCallData_t* call = (WriterAVCallData_t*) _call;

    unsigned char           PesHeader[PES_MAX_HEADER_SIZE];
    unsigned long long int  VideoPts;
    unsigned int            TimeDelta;
    unsigned int            TimeScale;
    int                     len = 0;
    int ic = 0;
    struct iovec iov[IOVEC_SIZE];
    h265_printf(20, "\n");

    if (call == NULL)
    {
        h264_err("call data is NULL...\n");
        return 0;
    }

    TimeDelta = call->FrameRate;
    TimeScale = call->FrameScale;
    VideoPts  = call->Pts;
    
    h265_printf(20, "VideoPts %lld - %d %d\n", call->Pts, TimeDelta, TimeScale);

    if ((call->data == NULL) || (call->len <= 0))
    {
        h264_err("NULL Data. ignoring...\n");
        return 0;
    }

    if (call->fd < 0)
    {
        h264_err("file pointer < 0. ignoring ...\n");
        return 0;
    }
    
    if( call->InfoFlags & 0x1) // TS container
    {
        h265_printf(10, "H265 simple inject method!\n");
        uint32_t PacketLength = 0;
        uint32_t FakeStartCode = (call->Version << 8) | PES_VERSION_FAKE_START_CODE;
        
        iov[ic++].iov_base = PesHeader;
        initialHeader = 0;
        if (initialHeader) 
        {
            initialHeader = 0;
            iov[ic].iov_base  = call->private_data;
            iov[ic++].iov_len = call->private_size;
            PacketLength     += call->private_size;
        }

        iov[ic].iov_base = "";
        iov[ic++].iov_len = 1;
        
        iov[ic].iov_base  = call->data;
        iov[ic++].iov_len = call->len;
        PacketLength     += call->len;
        
        iov[0].iov_len = InsertPesHeader(PesHeader, -1, MPEG_VIDEO_PES_START_CODE, VideoPts, FakeStartCode);
        
        return call->WriteV(call->fd, iov, ic);
    }

    uint32_t PacketLength = 0;
    
    ic = 0;
    iov[ic++].iov_base = PesHeader;
    
    if (initialHeader)
    {
        if (CodecData)
        {
            free(CodecData);
            CodecData = NULL;
        }
        
        uint8_t  *private_data = call->private_data;
        uint32_t  private_size = call->private_size;
    
        PreparCodecData(private_data, private_size, &NalLengthBytes);
        
        if (CodecData != NULL)
        {
            iov[ic].iov_base  = CodecData;
            iov[ic++].iov_len = CodecDataLen;
            PacketLength     += CodecDataLen;
            initialHeader = 0;
        }
    }

    if (CodecData != NULL)
    {
        uint32_t pos = 0;
        do
        {
            if (ic >= IOVEC_SIZE)
            {
                h264_err(">> Drop data due to ic overflow\n");
                exit(-1);
                break;
            }
            
            uint32_t pack_len = 0;
            uint32_t i = 0;
            for (i = 0; i < NalLengthBytes; i++, pos++)
            {
                pack_len <<= 8;
                pack_len += call->data[pos];
            }
            
            if ( (pos + pack_len) > call->len )
            {
                pack_len = call->len - pos;
            }
            
            iov[ic].iov_base  = Head;
            iov[ic++].iov_len = sizeof(Head);
            PacketLength += sizeof(Head);
            
            iov[ic].iov_base  = call->data + pos;
            iov[ic++].iov_len = pack_len;
            PacketLength     += pack_len;
    
            pos += pack_len;
            
        } while ((pos + NalLengthBytes) < call->len);
        
        h265_printf (10, "<<<< PacketLength [%d]\n", PacketLength);
        iov[0].iov_len = InsertPesHeader(PesHeader, -1, MPEG_VIDEO_PES_START_CODE, VideoPts, 0);
        
        len = call->WriteV(call->fd, iov, ic);
        PacketLength += iov[0].iov_len;
        if (PacketLength != len)
        {
            h264_err("<<<< not all data have been written [%d/%d]\n", len, PacketLength);
        }
    }

    h265_printf (10, "< len %d\n", len);
    return len;
}

static int writeReverseData(void* _call)
{
    WriterAVCallData_t* call = (WriterAVCallData_t*) _call;
    
    return 0;
}
/* ***************************** */
/* Writer  Definition            */
/* ***************************** */

static WriterCaps_t caps = {
    "h265",
    eVideo,
    "V_HEVC",
    -1,
    STREAMTYPE_MPEG4_H265,
    CT_H265
};

struct Writer_s WriterVideoH265 = {
    &reset,
    &writeData,
    &writeReverseData,
    &caps
};
