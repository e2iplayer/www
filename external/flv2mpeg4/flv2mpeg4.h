/*
 * 
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

#ifndef FLV_2_MPEG4_H
#define FLV_2_MPEG4_H

#include <stdint.h>

typedef int (*flv2mpeg4_write_packet_cb)(void *usr_data, int keyframe, int pts, const uint8_t *buf, int size);
typedef int (*flv2mpeg4_write_extradata_cb)(void *usr_data, int width, int height, int bitrate, const uint8_t* extradata, int extradatasize);

typedef struct 
{
    flv2mpeg4_write_packet_cb write_packet_cb;
    flv2mpeg4_write_extradata_cb write_extradata_cb;
    void *usr_data;
    void *priv;
} flv2mpeg4_CTX;

flv2mpeg4_CTX *flv2mpeg4_init_ctx(void *priv_data, int width, int height, flv2mpeg4_write_packet_cb wp_cb, flv2mpeg4_write_extradata_cb we_cb);
void flv2mpeg4_set_frame(flv2mpeg4_CTX *ctx, int frame, int icounter);
int flv2mpeg4_process_flv_packet(flv2mpeg4_CTX *ctx, uint8_t picture_type, const uint8_t *buf, uint32_t size, uint32_t time);
int flv2mpeg4_prepare_extra_data(flv2mpeg4_CTX *ctx);
void flv2mpeg4_release_ctx(flv2mpeg4_CTX **pub_ctx);

#endif // FLV_2_MPEG4_H