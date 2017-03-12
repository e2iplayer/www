/*
 * MPEG4 structs and tables
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

#ifndef M4V_H
#define M4V_H

////////////////////////////////////////////////////////////////////////////////////////////////////////////////// 
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include "bitreader.h"
#include "dcprediction.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////// 
#define VOS_STARTCODE				0x1B0
#define USERDATA_STARTCODE			0x1B2
#define GOP_STARTCODE				0x1B3
#define VISUAL_OBJECT_STARTCODE		0x1B5
#define VOP_STARTCODE				0x1B6

////////////////////////////////////////////////////////////////////////////////////////////////////////////////// 
typedef struct _M4V_BLOCK
{
	int	block[64];

	int index;
	int last_index;
	
} M4V_BLOCK;

typedef struct _M4V_MICROBLOCK
{
	M4V_BLOCK	block[6];
	int qscale;
	int dquant; // for encoding
	
	int ac_pred;
	
	int intra;
	int skip;
#define MV_TYPE_16X16	0
#define MV_TYPE_8X8		1
	int mv_type;
	VLCDEC mv_x[4];
	VLCDEC mv_y[4];
	
} M4V_MICROBLOCK;

typedef struct _M4V_VOL
{
	int width;
	int height;
	int time_bits;

	M4V_DCPRED dcpred;

} M4V_VOL;

typedef struct _M4V_VOP
{
#define	M4V_I_TYPE	0
#define M4V_P_TYPE	1
#define M4V_B_TYPE	2
	int picture_type;
	int time;
	int icount;

	int intra_dc_threshold;
	
	int rounding_type;

	int qscale;
	int f_code;
	int b_code;

} M4V_VOP;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////// 

// same as FLV
static const uint8 zig_zag_scan[64] = 
{
  0,1,8,16,9,2,3,10,17,24,32,25,18,11,4,5,
  12,19,26,33,40,48,41,34,27,20,13,6,7,14,21,28,
  35,42,49,56,57,50,43,36,29,22,15,23,30,37,44,51,
  58,59,52,45,38,31,39,46,53,60,61,54,47,55,62,63
};

// M4V ADDED
static const uint8 alternate_horizontal_scan[64] = {
    0,  1,   2,  3,  8,  9, 16, 17, 
    10, 11,  4,  5,  6,  7, 15, 14,
    13, 12, 19, 18, 24, 25, 32, 33, 
    26, 27, 20, 21, 22, 23, 28, 29,
    30, 31, 34, 35, 40, 41, 48, 49, 
    42, 43, 36, 37, 38, 39, 44, 45,
    46, 47, 50, 51, 56, 57, 58, 59, 
    52, 53, 54, 55, 60, 61, 62, 63,
};

// M4V ADDED
static const uint8 alternate_vertical_scan[64] = {
    0,  8,  16, 24,  1,  9,  2, 10, 
    17, 25, 32, 40, 48, 56, 57, 49,
    41, 33, 26, 18,  3, 11,  4, 12, 
    19, 27, 34, 42, 50, 58, 35, 43,
    51, 59, 20, 28,  5, 13,  6, 14, 
    21, 29, 36, 44, 52, 60, 37, 45,
    53, 61, 22, 30,  7, 15, 23, 31, 
    38, 46, 54, 62, 39, 47, 55, 63,
};


#endif // M4V_H

