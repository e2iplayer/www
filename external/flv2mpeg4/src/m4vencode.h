/*
 * MPEG4 Bitstream/VLC Encoder
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

#ifndef M4VENCODE_H
#define M4VENCODE_H

#include "m4v.h"
#include "bitwriter.h"

void m4v_encode_m4v_header(BW* bw, M4V_VOL* vol, uint32 time);
void m4v_encode_vop_header(BW* bw, M4V_VOP* vop, int time_bits, int vop_not_coded);

void m4v_encode_I_mb(BW* bw, M4V_MICROBLOCK* mb);
void m4v_encode_P_mb(BW* bw, M4V_MICROBLOCK* mb);

int  m4v_encode_I_dcpred(M4V_MICROBLOCK* mb, M4V_DCPRED* dcpred, int mb_x, int mb_y);

#endif // M4VENCODE_H
