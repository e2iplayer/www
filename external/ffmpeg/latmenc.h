/*
 * LATM/LOAS muxer
 * Copyright (c) 2011 Kieran Kunhya <kieran@kunhya.com>
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef AVCODEC_LATMENC_H
#define AVCODEC_LATMENC_H

#include <stdint.h>

#define MAX_EXTRADATA_SIZE 1024

typedef struct LATMContext {
    int off;
    int channel_conf;
    int object_type;
    int counter;
    int mod; // default 0x0014
    uint8_t loas_header[3];
    uint8_t buffer[0x1fff + MAX_EXTRADATA_SIZE + 1024];
    int len;
} LATMContext;

int latmenc_decode_extradata(LATMContext *ctx, uint8_t *buf, int size);
int latmenc_write_packet(LATMContext *ctx, uint8_t *data, int size, uint8_t *extradata, int extradata_size);

#endif /* AVCODEC_LATMENC_H */

