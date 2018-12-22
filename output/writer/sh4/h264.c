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

#include "stm_ioctls.h"
#include "common.h"
#include "output.h"
#include "debug.h"
#include "misc.h"
#include "pes.h"
#include "writer.h"

/* ***************************** */
/* Makros/Constants              */
/* ***************************** */
#define NALU_TYPE_PLAYER2_CONTAINER_PARAMETERS          24
#define CONTAINER_PARAMETERS_VERSION                    0x00

/* ***************************** */
/* Types                         */
/* ***************************** */
typedef struct avcC_s
{
    unsigned char       Version;                /* configurationVersion        */
    unsigned char       Profile;                /* AVCProfileIndication        */
    unsigned char       Compatibility;          /* profile_compatibility       */
    unsigned char       Level;                  /* AVCLevelIndication          */
    unsigned char       NalLengthMinusOne;      /* held in bottom two bits     */
    unsigned char       NumParamSets;           /* held in bottom 5 bits       */
    unsigned char       Params[1];              /* {length,params}{length,params}...sequence then picture*/
} avcC_t;


/* ***************************** */
/* Varaibles                     */
/* ***************************** */
const  uint8_t   Head[] = {0, 0, 0, 1};
static int32_t   initialHeader = 1;
static uint32_t  NalLengthBytes = 1;
static int       avc3 = 0;
/* ***************************** */
/* Prototypes                    */
/* ***************************** */

/* ***************************** */
/* MISC Functions                */
/* ***************************** */
// Please see: https://bugzilla.mozilla.org/show_bug.cgi?id=1105771
static int32_t UpdateExtraData(uint8_t **ppExtraData, uint32_t *pExtraDataSize, uint8_t *pData, uint32_t dataSize)
{
    uint8_t *aExtraData = *ppExtraData;
    
    if (aExtraData[0] != 1 || !pData) {
        // Not AVCC or nothing to update with.
        return -1;
    }
    
    int32_t nalsize = (aExtraData[4] & 3) + 1;
    
    uint8_t sps[256];
    uint8_t spsIdx = 0;
    
    uint8_t numSps = 0;
    
    uint8_t pps[256];
    uint8_t ppsIdx = 0;
    
    uint8_t numPps = 0;

    if(nalsize != 4)
    {
        return -1;
    }
    
    // Find SPS and PPS NALUs in AVCC data
    uint8_t *d = pData;
    while (d + 4 < pData + dataSize)
    {
        uint32_t nalLen = ReadUint32(d);
        
        uint8_t nalType = d[4] & 0x1f;
        if (nalType == 7) 
        { /* SPS */
        
            // 16 bits size
            sps[spsIdx++] = (uint8_t)(0xFF & (nalLen >> 8));
            sps[spsIdx++] = (uint8_t)(0xFF & nalLen);
            
            if (spsIdx + nalLen >= sizeof(sps))
            {
                h264_err("SPS no free space to copy...\n");
                return -1;
            }
            memcpy(&(sps[spsIdx]), d + 4, nalLen);
            spsIdx += nalLen;
            
            numSps += 1;
            
            h264_printf(10, "SPS len[%u]...\n", nalLen);
        } 
        else if (nalType == 8)
        { /* PPS */

            // 16 bits size
            pps[ppsIdx++] = (uint8_t)(0xFF & (nalLen >> 8));
            pps[ppsIdx++] = (uint8_t)(0xFF & nalLen);
            
            if (ppsIdx + nalLen >= sizeof(sps))
            {
                h264_err("PPS not free space to copy...\n");
                return -1;
            }
            memcpy(&(pps[ppsIdx]), d + 4, nalLen);
            ppsIdx += nalLen;
            
            numPps += 1;
            
            h264_printf(10, "PPS len[%u]...\n", nalLen);
        }
        d += 4 + nalLen;
    }
    uint32_t idx = 0;
    *ppExtraData = malloc(7 + spsIdx + ppsIdx);
    aExtraData = *ppExtraData;
    aExtraData[idx++] = 0x1;            // version
    aExtraData[idx++] = sps[3];         // profile
    aExtraData[idx++] = sps[4];         // profile compat
    aExtraData[idx++] = sps[5];         // level
    aExtraData[idx++] = 0xff;           // nal size - 1
    
    aExtraData[idx++] = 0xe0 | numSps;
    if (numSps)
    {
        memcpy(&(aExtraData[idx]), sps, spsIdx);
        idx += spsIdx;
    }
    
    aExtraData[idx++] = numPps;
    if (numPps)
    {
        memcpy(&(aExtraData[idx]), pps, ppsIdx);
        idx += ppsIdx;
    }
    
    h264_printf(10, "aExtraData len[%u]...\n", idx);
    *pExtraDataSize = idx;
    return 0;
}

static int32_t reset()
{   
    initialHeader = 1;
    avc3 = 0;
    return 0;
}

static int32_t writeData(void* _call)
{
    WriterAVCallData_t* call = (WriterAVCallData_t*) _call;

    uint8_t   PesHeader[PES_MAX_HEADER_SIZE];
    uint64_t  VideoPts;
    uint32_t  TimeDelta;
    uint32_t  TimeScale;
    int32_t   len = 0;
    int32_t   ic = 0;
    struct iovec iov[128];
    h264_printf(10, "\n");

    if (call == NULL)
    {
        h264_err("call data is NULL...\n");
        return 0;
    }

    TimeDelta = call->FrameRate;
    TimeScale = call->FrameScale;
    VideoPts  = call->Pts;
    
    h264_printf(10, "VideoPts %lld - %d %d\n", call->Pts, TimeDelta, TimeScale);

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

    /* AnnexA */
    if( !avc3 && ((1 < call->private_size && 0 == call->private_data[0]) || 
        (call->len > 3) && ((call->data[0] == 0x00 && call->data[1] == 0x00 && call->data[2] == 0x00 && call->data[3] == 0x01) ||
        (call->data[0] == 0xff && call->data[1] == 0xff && call->data[2] == 0xff && call->data[3] == 0xff))))
    {
        uint32_t PacketLength = 0;
        uint32_t FakeStartCode = /*(call->Version << 8) | */PES_VERSION_FAKE_START_CODE;
        
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
        
        /*Hellmaster1024: some packets will only be accepted by the player if we send one byte more than
                          data is available. The content of this byte does not matter. It will be ignored
                          by the player */
        iov[ic].iov_base = "\0";
        iov[ic++].iov_len = 1;
        
        iov[0].iov_len = InsertPesHeader(PesHeader, -1, MPEG_VIDEO_PES_START_CODE, VideoPts, FakeStartCode);
        int ret = call->WriteV(call->fd, iov, ic);
        return ret;
    }
    else if( !call->private_data || call->private_size < 7 || 1 != call->private_data[0])
    {
         h264_err("No valid private data available!\n");
         return 0;
    }

    if (initialHeader)
    {
        uint8_t  *private_data = call->private_data;
        uint32_t  private_size = call->private_size;
        avcC_t*         avcCHeader          = (avcC_t*)private_data;
        unsigned int    i;
        unsigned int    ParamSets;
        unsigned int    ParamOffset;
        unsigned int    InitialHeaderLength = 0;
        unsigned int    ParametersLength;

        ParametersLength                      = 0;

        unsigned char  HeaderData[19];
        
        if (private_size <= sizeof(avcC_t))
        {
            UpdateExtraData(&private_data, &private_size, call->data, call->len);
            if (private_data != call->private_data)
            {
                avc3 = 1;
                avcCHeader          = (avcC_t*)private_data;
            }
        }
        
        HeaderData[ParametersLength++]        = 0x00;                                         // Start code
        HeaderData[ParametersLength++]        = 0x00;
        HeaderData[ParametersLength++]        = 0x01;
        HeaderData[ParametersLength++]        = NALU_TYPE_PLAYER2_CONTAINER_PARAMETERS;
        // Container message version - changes when/if we vary the format of the message
        HeaderData[ParametersLength++]        = CONTAINER_PARAMETERS_VERSION;
        HeaderData[ParametersLength++]        = 0xff;                                         // Field separator

        if( TimeDelta == 0xffffffff )
            TimeDelta       = (TimeScale > 1000) ? 1001 : 1;

        HeaderData[ParametersLength++]        = (TimeScale >> 24) & 0xff;         // Output the timescale
        HeaderData[ParametersLength++]        = (TimeScale >> 16) & 0xff;
        HeaderData[ParametersLength++]        = 0xff;
        HeaderData[ParametersLength++]        = (TimeScale >> 8) & 0xff;
        HeaderData[ParametersLength++]        = TimeScale & 0xff;
        HeaderData[ParametersLength++]        = 0xff;

        HeaderData[ParametersLength++]        = (TimeDelta >> 24) & 0xff;         // Output frame period
        HeaderData[ParametersLength++]        = (TimeDelta >> 16) & 0xff;
        HeaderData[ParametersLength++]        = 0xff;
        HeaderData[ParametersLength++]        = (TimeDelta >> 8) & 0xff;
        HeaderData[ParametersLength++]        = TimeDelta & 0xff;
        HeaderData[ParametersLength++]        = 0xff;
        HeaderData[ParametersLength++]        = 0x80;                                         // Rsbp trailing bits

        assert(ParametersLength <= sizeof(HeaderData));

        ic = 0;
        iov[ic].iov_base = PesHeader;
        iov[ic++].iov_len = InsertPesHeader (PesHeader, ParametersLength, MPEG_VIDEO_PES_START_CODE, INVALID_PTS_VALUE, 0);
        iov[ic].iov_base = HeaderData;
        iov[ic++].iov_len = ParametersLength;
        len = call->WriteV(call->fd, iov, ic);
        if (len < 0)
        {
            return len;
        }

        NalLengthBytes  = (avcCHeader->NalLengthMinusOne & 0x03) + 1;
        ParamSets       = avcCHeader->NumParamSets & 0x1f;

        h264_printf(20, "avcC contents:\n");
        h264_printf(20, "    version:                       %d\n", avcCHeader->Version);
        h264_printf(20, "    profile:                       %d\n", avcCHeader->Profile);
        h264_printf(20, "    profile compatibility:         %d\n", avcCHeader->Compatibility);
        h264_printf(20, "    level:                         %d\n", avcCHeader->Level);
        h264_printf(20, "    nal length bytes:              %d\n", NalLengthBytes);
        h264_printf(20, "    number of sequence param sets: %d\n", ParamSets);

        ParamOffset     = 0;
        ic = 0;
        iov[ic++].iov_base = PesHeader;
        for (i = 0; i < ParamSets; i++) 
        {
            unsigned int  PsLength = (avcCHeader->Params[ParamOffset] << 8) + avcCHeader->Params[ParamOffset+1];

            h264_printf(20, "        sps %d has length           %d\n", i, PsLength);

            iov[ic].iov_base = (char *)Head;
            iov[ic++].iov_len = sizeof(Head);
                InitialHeaderLength        += sizeof(Head);
            iov[ic].iov_base = &avcCHeader->Params[ParamOffset+2];
            iov[ic++].iov_len = PsLength;
            InitialHeaderLength        += PsLength;
            ParamOffset                += PsLength+2;
        }

        ParamSets = avcCHeader->Params[ParamOffset];

        h264_printf(20,  "    number of picture param sets:  %d\n", ParamSets);

        ParamOffset++;
        for (i = 0; i < ParamSets; i++) 
        {
            unsigned int  PsLength      = (avcCHeader->Params[ParamOffset] << 8) + avcCHeader->Params[ParamOffset+1];

            h264_printf (20, "        pps %d has length           %d\n", i, PsLength);

            iov[ic].iov_base = (char *) Head;
            iov[ic++].iov_len = sizeof(Head);
                InitialHeaderLength        += sizeof(Head);
            iov[ic].iov_base = &avcCHeader->Params[ParamOffset+2];
            iov[ic++].iov_len = PsLength;
            InitialHeaderLength        += PsLength;
            ParamOffset                += PsLength+2;
        }

        iov[0].iov_len = InsertPesHeader (PesHeader, InitialHeaderLength, MPEG_VIDEO_PES_START_CODE, INVALID_PTS_VALUE, 0);
        ssize_t l = call->WriteV(call->fd, iov, ic);
        
        if (private_data != call->private_data)
        {
            free(private_data);
        }
            
        if (l < 0)
        {
            return l;
        }
        
        len += l;
        initialHeader = 0;
    }

    unsigned int SampleSize    = call->len;
    unsigned int NalStart      = 0;
    unsigned int VideoPosition = 0;

    do 
    {
        unsigned int   NalLength;
        unsigned char  NalData[4];
        int NalPresent = 1;

        memcpy (NalData, call->data + VideoPosition, NalLengthBytes);
        VideoPosition += NalLengthBytes;
        NalStart += NalLengthBytes;
        switch(NalLengthBytes) 
        {
            case 1:  NalLength = (NalData[0]); break;
            case 2:  NalLength = (NalData[0] <<  8) | (NalData[1]); break;
            case 3:  NalLength = (NalData[0] << 16) | (NalData[1] <<  8) | (NalData[2]); break;
            default: NalLength = (NalData[0] << 24) | (NalData[1] << 16) | (NalData[2] << 8) | (NalData[3]); break;
        }

        h264_printf(20, "NalStart = %u + NalLength = %u > SampleSize = %u\n", NalStart, NalLength, SampleSize);

        if (NalStart + NalLength > SampleSize) 
        {
            h264_printf(20, "nal length past end of buffer - size %u frame offset %u left %u\n",
                NalLength, NalStart , SampleSize - NalStart );

            NalStart = SampleSize;
        } 
        else 
        {
            NalStart += NalLength;
            ic = 0;
            iov[ic++].iov_base = PesHeader;

            if (NalPresent) {
                NalPresent = 0;
                iov[ic].iov_base = (char *)Head;
                iov[ic++].iov_len = sizeof(Head);
            }

            iov[ic].iov_base = call->data + VideoPosition;
            iov[ic++].iov_len = NalLength;
            VideoPosition += NalLength;

            h264_printf (20, "  pts=%llu\n", VideoPts);

            iov[0].iov_len = InsertPesHeader (PesHeader, NalLength, MPEG_VIDEO_PES_START_CODE, VideoPts, 0);
            ssize_t l = call->WriteV(call->fd, iov, ic);
            if (l < 0)
                return l;
            len += l;

            VideoPts = INVALID_PTS_VALUE;
        }
    } while (NalStart < SampleSize);

    if (len < 0)
    {
        h264_err("error writing data errno = %d\n", errno);
        h264_err("%s\n", strerror(errno));
    }

    h264_printf (10, "< len %d\n", len);
    return len;
}

static int writeReverseData(void* _call)
{
    WriterAVCallData_t* call = (WriterAVCallData_t*) _call;

    h264_printf(10, "\n");

    if (call == NULL)
    {
        h264_err("call data is NULL...\n");
        return 0;
    }

    h264_printf(10, "VideoPts %lld\n", call->Pts);

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

    return 0;
}
/* ***************************** */
/* Writer  Definition            */
/* ***************************** */

static WriterCaps_t caps = {
    "h264",
    eVideo,
    "V_MPEG4/ISO/AVC",
    VIDEO_ENCODING_H264,
    -1,
    -1
};

struct Writer_s WriterVideoH264 = {
    &reset,
    &writeData,
    &writeReverseData,
    &caps
};