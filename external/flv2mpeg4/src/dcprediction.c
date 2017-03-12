/*
 * DC prediction
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

#include "dcprediction.h"


// M4V ADDED
static const uint8 mpeg4_y_dc_scale_table[32]={
//  0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31
    0, 8, 8, 8, 8,10,12,14,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,34,36,38,40,42,44,46
};

// M4V ADDED
static const uint8 mpeg4_c_dc_scale_table[32]={
//  0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31
    0, 8, 8, 8, 8, 9, 9,10,10,11,11,12,12,13,13,14,14,15,15,16,16,17,17,18,18,19,20,21,22,23,24,25
};

static int __inline get_pred(int* dc_cur, int stride, int scale)
{
	/* B C
	   A X */

	int A = dc_cur[-1];
	int B = dc_cur[-1 - stride];
	int C = dc_cur[-stride];
	int pred;
	
	if (abs(A - B) < abs(B - C))
	{
		pred = C;
	}
	else
	{
		pred = A;
	}
	
	return (pred + (scale >> 1)) / scale;
}

static void __inline set_dc_to_dc_cur(int *dc_cur, int level, int scale)
{
	level *= scale;
	if (level & (~2047))
	{
		if (level < 0)
			level = 0;
		else
			level = 2047;
	}

	dc_cur[0] = level;
}

static int* get_dc_cur(M4V_DCPRED* pred, int mb_x, int mb_y, int n)
{
	if (n < 4)
	{
		return pred->dc[n] + mb_x * 2 + mb_y * 2 * pred->stride[n] + pred->block_offset[n];
	}
	else
	{
		return pred->dc[n] + mb_x * 1 + mb_y * pred->stride[n];
	}
}

static int __inline get_scale(M4V_DCPRED* pred, int n)
{
	if (n < 4)
	{
		return pred->y_dc_scale;
	}
	else
	{
		return pred->c_dc_scale;
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////// 
void dcpred_set_qscale(M4V_DCPRED* pred, int qscale)
{
	if (qscale < 0) qscale = 0;
	if (qscale > 31) qscale = 31;
	pred->y_dc_scale = mpeg4_y_dc_scale_table[qscale];
	pred->c_dc_scale = mpeg4_c_dc_scale_table[qscale];
}

void dcpred_set_pos(M4V_DCPRED* pred, int mb_x, int mb_y)
{
	int n;
	for (n = 0; n < 6; n++)
	{
		pred->dc_cur[n] = get_dc_cur(pred, mb_x, mb_y, n);
	}
}

int dcpred_for_enc(M4V_DCPRED* p, int n, int level)
{
	int* dc_cur = p->dc_cur[n];
	int scale = get_scale(p, n);
	int pred = get_pred(dc_cur, p->stride[n], scale);

	set_dc_to_dc_cur(dc_cur, level, scale);
	return level - pred; 
}

int dcpred_for_dec(M4V_DCPRED* p, int n, int level)
{
	int* dc_cur = p->dc_cur[n];
	int scale = get_scale(p, n);
	int pred = get_pred(dc_cur, p->stride[n], scale);

	level += pred;
	set_dc_to_dc_cur(dc_cur, level, scale);
	return level; 
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////// 
static void init_plane(M4V_DCPRED* pred, int n)
{
	int x, len = pred->stride[n]*pred->height[n];
	int* p = pred->_dc[n];

	for (x = 0; x < len; x++)
	{
		p[x] = 1024;
	}
}

void init_dcpred(M4V_DCPRED* pred)
{
	init_plane(pred, 0);
	init_plane(pred, 4);
	init_plane(pred, 5);
}

void alloc_dcpred(M4V_DCPRED* pred, int mb_width, int mb_height)
{
	const int w2 = mb_width  * 2 + 1;
	const int h2 = mb_height * 2 + 1;
	const int w = mb_width      + 1;
	const int h = mb_height     + 1;

	pred->_dc[0] = pred->_dc[1] = pred->_dc[2] = pred->_dc[3] = (int*)malloc(sizeof(int) * w2 * h2);
	pred->_dc[4] = (int*)malloc(sizeof(int) * w * h);
	pred->_dc[5] = (int*)malloc(sizeof(int) * w * h);

	pred->dc[0] = pred->dc[1] = pred->dc[2] = pred->dc[3] = pred->_dc[0] + w2 + 1;
	pred->dc[4] = pred->_dc[4] + w + 1;
	pred->dc[5] = pred->_dc[5] + w + 1;

	pred->stride[0] = pred->stride[1] = pred->stride[2] = pred->stride[3] = w2;
	pred->height[0] = pred->height[1] = pred->height[2] = pred->height[3] = h2;
	pred->stride[4] = pred->stride[5] = w;
	pred->height[4] = pred->height[5] = h;
		
	pred->block_offset[0] = 0;
	pred->block_offset[1] = 1;
	pred->block_offset[2] = w2;
	pred->block_offset[3] = w2 + 1;
	pred->block_offset[4] = 0;
	pred->block_offset[5] = 0;
}

void free_dcpred(M4V_DCPRED* pred)
{
	free(pred->_dc[0]);
	free(pred->_dc[4]);
	free(pred->_dc[5]);
}
