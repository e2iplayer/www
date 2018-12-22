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
 
 /* ***************************** */
/* Includes                      */
/* ***************************** */
#include <ffmpeg/get_bits.h>
#include <ffmpeg/put_bits.h>
#include <ffmpeg/mpeg4audio.h>
#include <ffmpeg/latmenc.h>
#include "debug.h"

/* ***************************** */
/* Makros/Constants              */
/* ***************************** */

int latmenc_decode_extradata(LATMContext *ctx, uint8_t *buf, int size)
{
    MPEG4AudioConfig m4ac;

    if (size > MAX_EXTRADATA_SIZE) {
        latmenc_err("Extradata is larger than currently supported.\n");
        return AVERROR_INVALIDDATA;
    }
    ctx->off = avpriv_mpeg4audio_get_config(&m4ac, buf, size * 8, 1);
    if (ctx->off < 0)
        return ctx->off;

    if (ctx->object_type == AOT_ALS && (ctx->off & 7)) {
        // as long as avpriv_mpeg4audio_get_config works correctly this is impossible
        latmenc_err("BUG: ALS offset is not byte-aligned\n");
        return AVERROR_INVALIDDATA;
    }
    /* FIXME: are any formats not allowed in LATM? */

    if (m4ac.object_type > AOT_SBR && m4ac.object_type != AOT_ALS) {
        latmenc_err("Muxing MPEG-4 AOT %d in LATM is not supported\n", m4ac.object_type);
        return AVERROR_INVALIDDATA;
    }
    ctx->channel_conf = m4ac.chan_config;
    ctx->object_type  = m4ac.object_type;

    return 0;
}

static void latmenc_write_frame_header(LATMContext *ctx, uint8_t *extradata, int extradata_size, PutBitContext *bs)
{
    int header_size;

    /* AudioMuxElement */
    put_bits(bs, 1, !!ctx->counter);

    if (!ctx->counter) {
        /* StreamMuxConfig */
        put_bits(bs, 1, 0); /* audioMuxVersion */
        put_bits(bs, 1, 1); /* allStreamsSameTimeFraming */
        put_bits(bs, 6, 0); /* numSubFrames */
        put_bits(bs, 4, 0); /* numProgram */
        put_bits(bs, 3, 0); /* numLayer */

        /* AudioSpecificConfig */
        if (ctx->object_type == AOT_ALS) {
            header_size = extradata_size-(ctx->off >> 3);
            avpriv_copy_bits(bs, &extradata[ctx->off >> 3], header_size);
        } else {
            // + 3 assumes not scalable and dependsOnCoreCoder == 0,
            // see decode_ga_specific_config in libavcodec/aacdec.c
            avpriv_copy_bits(bs, extradata, ctx->off + 3);

            if (!ctx->channel_conf) {
                GetBitContext gb;
                int ret = init_get_bits8(&gb, extradata, extradata_size);
                av_assert0(ret >= 0); // extradata size has been checked already, so this should not fail
                skip_bits_long(&gb, ctx->off + 3);
                avpriv_copy_pce_data(bs, &gb);
            }
        }

        put_bits(bs, 3, 0); /* frameLengthType */
        put_bits(bs, 8, 0xff); /* latmBufferFullness */

        put_bits(bs, 1, 0); /* otherDataPresent */
        put_bits(bs, 1, 0); /* crcCheckPresent */
    }

    ctx->counter++;
    ctx->counter %= ctx->mod;
}

int latmenc_write_packet(LATMContext *ctx, uint8_t *data, int size, uint8_t *extradata, int extradata_size)
{
    PutBitContext bs;
    int i, len;

    if (size > 0x1fff)
        goto too_large;

    init_put_bits(&bs, ctx->buffer, size+1024+MAX_EXTRADATA_SIZE);

    latmenc_write_frame_header(ctx, extradata, extradata_size, &bs);

    /* PayloadLengthInfo() */
    for (i = 0; i <= size-255; i+=255)
        put_bits(&bs, 8, 255);

    put_bits(&bs, 8, size-i);

    /* The LATM payload is written unaligned */

    /* PayloadMux() */
    if (size && (data[0] & 0xe1) == 0x81) {
        // Convert byte-aligned DSE to non-aligned.
        // Due to the input format encoding we know that
        // it is naturally byte-aligned in the input stream,
        // so there are no padding bits to account for.
        // To avoid having to add padding bits and rearrange
        // the whole stream we just remove the byte-align flag.
        // This allows us to remux our FATE AAC samples into latm
        // files that are still playable with minimal effort.
        put_bits(&bs, 8, data[0] & 0xfe);
        avpriv_copy_bits(&bs, data + 1, 8*size - 8);
    } else
        avpriv_copy_bits(&bs, data, 8*size);

    avpriv_align_put_bits(&bs);
    flush_put_bits(&bs);

    len = put_bits_count(&bs) >> 3;

    if (len > 0x1fff)
        goto too_large;

    memcpy(ctx->loas_header, "\x56\xe0\x00", 3);
    ctx->loas_header[1] |= (len >> 8) & 0x1f;
    ctx->loas_header[2] |= len & 0xff;
    ctx->len = len;
    return 0;

too_large:
    latmenc_err("LATM packet size larger than maximum size 0x1fff\n");
    return AVERROR_INVALIDDATA;
}

