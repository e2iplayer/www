/*
 * FLV structs and tables
 *
 * Copyright (c) 2006 vixy project
 *
 * This file contains the code that based on FFmpeg (http://ffmpeg.mplayerhq.hu/)
 * See original copyright notice in /FFMPEG_CREDITS and /FFMPEG_IMPORTS
 *
 * This file is part of VIXY FLV Converter.
 *
 * 'VIXY FLV Converter' is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * 'VIXY FLV Converter' is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef FLV_H
#define FLV_H

////////////////////////////////////////////////////////////////////////////////////////////////////////////////// 
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include "bitreader.h"
#include "bitwriter.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////// 
typedef struct _BLOCK
{
	int	block[64];

	int index;
	int last_index;
	
} BLOCK;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////// 
typedef struct _MICROBLOCK
{
	BLOCK	block[6];
	int dquant;
	
	int intra;
	int skip;
#define MV_TYPE_16X16	0
#define MV_TYPE_8X8		1
	int mv_type;
	VLCDEC mv_x[4];
	VLCDEC mv_y[4];
	
} MICROBLOCK;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////// 
typedef struct _PICTURE
{
	int width;
	int height;
	

#define FLV_I_TYPE		0
#define FLV_P_TYPE		1

	int picture_type;	// 0:I 1:P
	int escape_type;	// 0:h263 1:flv(11bits)
	
	int qscale;
	
	int frame_number;

} PICTURE;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////// 
int decode_picture_header(BR* p, PICTURE* picture);
int decode_I_mb(BR* p, MICROBLOCK* mb, int escape_type, int qscale);
int decode_P_mb(BR* p, MICROBLOCK* mb, int escape_type, int qscale);

void encode_picture_header(BW* bw, PICTURE* picture);
void encode_I_mb(BW* bw, MICROBLOCK* mb, int escape_type);
void encode_P_mb(BW* bw, MICROBLOCK* mb, int escape_type);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////// 

static const uint16 rl_inter_vlc[103][2] = {
{ 0x2, 2 },{ 0xf, 4 },{ 0x15, 6 },{ 0x17, 7 },
{ 0x1f, 8 },{ 0x25, 9 },{ 0x24, 9 },{ 0x21, 10 },
{ 0x20, 10 },{ 0x7, 11 },{ 0x6, 11 },{ 0x20, 11 },
{ 0x6, 3 },{ 0x14, 6 },{ 0x1e, 8 },{ 0xf, 10 },
{ 0x21, 11 },{ 0x50, 12 },{ 0xe, 4 },{ 0x1d, 8 },
{ 0xe, 10 },{ 0x51, 12 },{ 0xd, 5 },{ 0x23, 9 },
{ 0xd, 10 },{ 0xc, 5 },{ 0x22, 9 },{ 0x52, 12 },
{ 0xb, 5 },{ 0xc, 10 },{ 0x53, 12 },{ 0x13, 6 },
{ 0xb, 10 },{ 0x54, 12 },{ 0x12, 6 },{ 0xa, 10 },
{ 0x11, 6 },{ 0x9, 10 },{ 0x10, 6 },{ 0x8, 10 },
{ 0x16, 7 },{ 0x55, 12 },{ 0x15, 7 },{ 0x14, 7 },
{ 0x1c, 8 },{ 0x1b, 8 },{ 0x21, 9 },{ 0x20, 9 },
{ 0x1f, 9 },{ 0x1e, 9 },{ 0x1d, 9 },{ 0x1c, 9 },
{ 0x1b, 9 },{ 0x1a, 9 },{ 0x22, 11 },{ 0x23, 11 },
{ 0x56, 12 },{ 0x57, 12 },{ 0x7, 4 },{ 0x19, 9 },
{ 0x5, 11 },{ 0xf, 6 },{ 0x4, 11 },{ 0xe, 6 },
{ 0xd, 6 },{ 0xc, 6 },{ 0x13, 7 },{ 0x12, 7 },
{ 0x11, 7 },{ 0x10, 7 },{ 0x1a, 8 },{ 0x19, 8 },
{ 0x18, 8 },{ 0x17, 8 },{ 0x16, 8 },{ 0x15, 8 },
{ 0x14, 8 },{ 0x13, 8 },{ 0x18, 9 },{ 0x17, 9 },
{ 0x16, 9 },{ 0x15, 9 },{ 0x14, 9 },{ 0x13, 9 },
{ 0x12, 9 },{ 0x11, 9 },{ 0x7, 10 },{ 0x6, 10 },
{ 0x5, 10 },{ 0x4, 10 },{ 0x24, 11 },{ 0x25, 11 },
{ 0x26, 11 },{ 0x27, 11 },{ 0x58, 12 },{ 0x59, 12 },
{ 0x5a, 12 },{ 0x5b, 12 },{ 0x5c, 12 },{ 0x5d, 12 },
{ 0x5e, 12 },{ 0x5f, 12 },{ 0x3, 7 },
};

static const int8 rl_inter_level[102] = {
  1,  2,  3,  4,  5,  6,  7,  8,
  9, 10, 11, 12,  1,  2,  3,  4,
  5,  6,  1,  2,  3,  4,  1,  2,
  3,  1,  2,  3,  1,  2,  3,  1,
  2,  3,  1,  2,  1,  2,  1,  2,
  1,  2,  1,  1,  1,  1,  1,  1,
  1,  1,  1,  1,  1,  1,  1,  1,
  1,  1,  1,  2,  3,  1,  2,  1,
  1,  1,  1,  1,  1,  1,  1,  1,
  1,  1,  1,  1,  1,  1,  1,  1,
  1,  1,  1,  1,  1,  1,  1,  1,
  1,  1,  1,  1,  1,  1,  1,  1,
  1,  1,  1,  1,  1,  1,
};

static const int8 rl_inter_run[102] = {
  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  1,  1,  1,  1,
  1,  1,  2,  2,  2,  2,  3,  3,
  3,  4,  4,  4,  5,  5,  5,  6,
  6,  6,  7,  7,  8,  8,  9,  9,
 10, 10, 11, 12, 13, 14, 15, 16,
 17, 18, 19, 20, 21, 22, 23, 24,
 25, 26,  0,  0,  0,  1,  1,  2,
  3,  4,  5,  6,  7,  8,  9, 10,
 11, 12, 13, 14, 15, 16, 17, 18,
 19, 20, 21, 22, 23, 24, 25, 26,
 27, 28, 29, 30, 31, 32, 33, 34,
 35, 36, 37, 38, 39, 40,
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////// 
static const int rl_inter_n = 102;
static const int rl_inter_last = 58;


#endif // FLV_H
