/*
 * DC Prediction
 *
 * Copyright (c) 2006 vixy project
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

#ifndef DCPREDICTION_H
#define DCPREDICTION_H

#include "type.h"
#include <stdlib.h>

typedef struct _M4V_DCPRED
{
	int* _dc[6]; // for malloc(),free()
	int* dc[6]; // for point (0,0)
	int* dc_cur[6]; // for point current pos

	int stride[6];
	int height[6];
	
	int block_offset[6];
	
	int y_dc_scale;
	int c_dc_scale;


} M4V_DCPRED;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////// 

void dcpred_set_qscale(M4V_DCPRED* pred, int qscale);
void dcpred_set_pos(M4V_DCPRED* pred, int mb_x, int mb_y);
int dcpred_for_enc(M4V_DCPRED* pred, int n, int level);
int dcpred_for_dec(M4V_DCPRED* pred, int n, int level);
void init_dcpred(M4V_DCPRED* pred);
void alloc_dcpred(M4V_DCPRED* pred, int mb_width, int mb_height);
void free_dcpred(M4V_DCPRED* pred);

#endif // DCPREDICTION_H
