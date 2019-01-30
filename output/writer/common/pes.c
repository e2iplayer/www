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

/* ***************************** */
/* Prototypes                    */
/* ***************************** */

/* ***************************** */
/* Functions                     */
/* ***************************** */

int32_t InsertVideoPrivateDataHeader(uint8_t *data, int32_t payload_size)
{
    BitPacker_t ld2 = {data, 0, 32};
    int32_t i = 0;

    PutBits (&ld2, PES_PRIVATE_DATA_FLAG, 8);
    PutBits (&ld2, payload_size & 0xff, 8);
    PutBits (&ld2, (payload_size >> 8) & 0xff, 8);
    PutBits (&ld2, (payload_size >> 16) & 0xff, 8);

    for (i=4; i < (PES_PRIVATE_DATA_LENGTH+1); i++)
    {
        PutBits (&ld2, 0, 8);
    }

    FlushBits (&ld2);

    return PES_PRIVATE_DATA_LENGTH + 1;
}

void UpdatePesHeaderPayloadSize(uint8_t *data, int32_t size)
{
    if (size > MAX_PES_PACKET_SIZE || size < 0)
            size = 0;
    data[4] = size >> 8;
    data[5] = size & 0xFF;
}

int32_t InsertPesHeader(uint8_t *data, int32_t size, uint8_t stream_id, uint64_t pts, int32_t pic_start_code)
{
    BitPacker_t ld2 = {data, 0, 32};

    PutBits(&ld2,0x0  ,8);
    PutBits(&ld2,0x0  ,8);
    PutBits(&ld2,0x1  ,8);       // Start Code
    PutBits(&ld2,stream_id ,8);  // Stream_id = Audio Stream

    if (size > 0) {
        size += 3 + (pts != INVALID_PTS_VALUE ? 5:0) + (pic_start_code ? (5) : 0);
    }

    if (size > MAX_PES_PACKET_SIZE || size < 0)
    {
        size = 0; // unbounded
    }

    //4
    PutBits(&ld2, size, 16); // PES_packet_length

    //6 = 4+2
    PutBits(&ld2,0x2  ,2);  // 10
    PutBits(&ld2,0x0  ,2);  // PES_Scrambling_control
    PutBits(&ld2,0x0  ,1);  // PES_Priority
    PutBits(&ld2,0x0  ,1);  // data_alignment_indicator
    PutBits(&ld2,0x0  ,1);  // Copyright
    PutBits(&ld2,0x0  ,1);  // Original or Copy
    //7 = 6+1

    if (pts != INVALID_PTS_VALUE)
    {
        PutBits(&ld2,0x2 ,2);
    }
    else
    {
        PutBits(&ld2,0x0 ,2);  // PTS_DTS flag
    }

    PutBits(&ld2,0x0 ,1);  // ESCR_flag
    PutBits(&ld2,0x0 ,1);  // ES_rate_flag
    PutBits(&ld2,0x0 ,1);  // DSM_trick_mode_flag
    PutBits(&ld2,0x0 ,1);  // additional_copy_ingo_flag
    PutBits(&ld2,0x0 ,1);  // PES_CRC_flag
    PutBits(&ld2,0x0 ,1);  // PES_extension_flag
    //8 = 7+1

    if (pts!=INVALID_PTS_VALUE)
    {
        PutBits(&ld2,0x5,8);
    }
    else
    {
        PutBits(&ld2,0x0 ,8);  // PES_header_data_length
    }
    //9 = 8+1

    if (pts!=INVALID_PTS_VALUE)
    {
        PutBits(&ld2,0x2,4);
        PutBits(&ld2,(pts>>30) & 0x7,3);
        PutBits(&ld2,0x1,1);
        PutBits(&ld2,(pts>>15) & 0x7fff,15);
        PutBits(&ld2,0x1,1);
        PutBits(&ld2,pts & 0x7fff,15);
        PutBits(&ld2,0x1,1);
    }
    //14 = 9+5

    if (pic_start_code)
    {
        PutBits(&ld2,0x0 ,8);
        PutBits(&ld2,0x0 ,8);
        PutBits(&ld2,0x1 ,8);  // Start Code
        PutBits(&ld2,pic_start_code & 0xff ,8);  // 00, for picture start
        PutBits(&ld2,(pic_start_code >> 8 )&0xff,8);  // For any extra information (like in mpeg4p2, the pic_start_code)
        //14 + 4 = 18
    }

    FlushBits(&ld2);

    return (ld2.Ptr - data);
}
