/*
 * FLV to MPEG4 converter
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

#include "m4v.h"
#include "bitwriter.h"
#include "flv.h"
#include "m4vencode.h"
#include "../flv2mpeg4.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////// 
typedef struct _CONVCTX
{
    int width;
    int height;

    int frame;
    int icounter;

    M4V_VOL vol;
} CONVCTX;


typedef struct 
{
    uint8   *out_buf;
    M4V_VOL  vol;
    CONVCTX  conv;
} CTX;

#define VOL_TIME_BITS		5
#define	PACKETBUFFER_SIZE	(256*1024*4)

////////////////////////////////////////////////////////////////////////////////////////////////////////////////// 
static const uint8 ff_mpeg4_y_dc_scale_table[32]={
//  0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31
    0, 8, 8, 8, 8,10,12,14,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,34,36,38,40,42,44,46
};

static const uint8 ff_mpeg4_c_dc_scale_table[32]={
//  0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31
    0, 8, 8, 8, 8, 9, 9,10,10,11,11,12,12,13,13,14,14,15,15,16,16,17,17,18,18,19,20,21,22,23,24,25
};


static void copy_vol(PICTURE* flv_pic, M4V_VOL* vol)
{
    vol->width = flv_pic->width;
    vol->height = flv_pic->height;
    vol->time_bits = VOL_TIME_BITS; // 0-31
}

static void copy_vop(PICTURE* flv_pic, M4V_VOP* vop, CONVCTX* c)
{
    vop->qscale = flv_pic->qscale;

    vop->time = c->frame % 30;
    vop->icount = (c->icounter + 29) / 30;
    vop->intra_dc_threshold = 99;

    if (flv_pic->picture_type == FLV_I_TYPE)
    {
        vop->picture_type = M4V_I_TYPE;
    }
    else
    {
        vop->picture_type = M4V_P_TYPE;
        vop->f_code = 1;
    }
}

static void copy_microblock(MICROBLOCK* flv_mb, M4V_MICROBLOCK* m4v_mb)
{
    int i;

    m4v_mb->dquant = flv_mb->dquant;
    memcpy(m4v_mb->block, flv_mb->block, sizeof(m4v_mb->block)); // !!!!!!!
    m4v_mb->intra = flv_mb->intra;
    m4v_mb->skip = flv_mb->skip;
    m4v_mb->mv_type = flv_mb->mv_type;

    memcpy(m4v_mb->mv_x, flv_mb->mv_x, sizeof(m4v_mb->mv_x)); // !!!!!!
    memcpy(m4v_mb->mv_y, flv_mb->mv_y, sizeof(m4v_mb->mv_y)); // !!!!!!

    // dc rescale
    if (m4v_mb->intra)
    {
        for (i = 0; i < 4; i++)
        {
            m4v_mb->block[i].block[0] *= 8;
            m4v_mb->block[i].block[0] /= ff_mpeg4_y_dc_scale_table[m4v_mb->qscale];
        }

        for (i = 4; i < 6; i++)
        {
            m4v_mb->block[i].block[0] *= 8;
            m4v_mb->block[i].block[0] /= ff_mpeg4_c_dc_scale_table[m4v_mb->qscale];
        }
    }
}

static int write_pad_not_coded_frames(flv2mpeg4_CTX *pub_ctx, CONVCTX *c, BW *bw, uint32 time)
{
    // if any timecode padding is needed, then pad.
    while (c->frame * 1000 / 30 < time)
    {
        M4V_VOP vop;
        memset(&vop, 0, sizeof(vop));
        vop.picture_type = M4V_P_TYPE;
        vop.time = c->frame % 30;
        vop.icount = (c->icounter + 29) / 30;
        m4v_encode_vop_header(bw, &vop, VOL_TIME_BITS, 1);
        m4v_stuffing(bw);

        flash_bw(bw);
        
        // write frame
        if (pub_ctx->write_packet_cb(pub_ctx->usr_data, 
            0, 
            0,//c->frame, 
            bw->buf, 
            bw->pos) < 0)
        {
            return -1;
        }

        clear_bw(bw);
        
        c->frame++;
        c->icounter++;
    }

    return 0;
}

static int write_m4v_picture_frame(flv2mpeg4_CTX *pub_ctx, CONVCTX *c, BR *br, BW *bw, PICTURE *flvpic, uint32 time)
{
    MICROBLOCK mb;
    M4V_VOP vop;
    M4V_MICROBLOCK m4v_mb;
    int x, y;
    int mb_width = (flvpic->width + 15) / 16;
    int mb_height = (flvpic->height + 15) / 16;

    memset(&vop, 0, sizeof(vop));

    copy_vop(flvpic, &vop, c);
    m4v_encode_vop_header(bw, &vop, VOL_TIME_BITS, 0);
        
    // transcode flv to mpeg4
    for (y = 0; y < mb_height; y++)
    {
        for (x = 0; x < mb_width; x++)
        {
            memset(&mb, 0, sizeof(mb));
            memset(&m4v_mb, 0, sizeof(m4v_mb));
            
            if (vop.picture_type == M4V_I_TYPE)
            {
                mb.intra = 1;
                if (decode_I_mb(br, &mb, flvpic->escape_type, flvpic->qscale) < 0) return -1;
                m4v_mb.qscale = vop.qscale;
                copy_microblock(&mb, &m4v_mb);
                m4v_encode_I_dcpred(&m4v_mb, &c->vol.dcpred, x, y);
                m4v_encode_I_mb(bw, &m4v_mb);
            }
            else
            {
                if (decode_P_mb(br, &mb, flvpic->escape_type, flvpic->qscale) < 0) return -1;
                m4v_mb.qscale = vop.qscale;
                copy_microblock(&mb, &m4v_mb);
                m4v_encode_I_dcpred(&m4v_mb, &c->vol.dcpred, x, y);
                m4v_encode_P_mb(bw, &m4v_mb);
            }
        }
    }

    m4v_stuffing(bw);
    flash_bw(bw);

    // write frame
    if (pub_ctx->write_packet_cb(pub_ctx->usr_data, 
        vop.picture_type == M4V_I_TYPE, 
        0,//c->frame, 
        bw->buf, 
        bw->pos) < 0)
    {
        return -1;
    }

    c->frame++;
    c->icounter++;

    return 0;
}

static int write_m4v_frame(flv2mpeg4_CTX *pub_ctx, CONVCTX* c, BR* br, BW* bw, uint32 time)
{
    PICTURE picture;

    memset(&picture, 0, sizeof(picture));
    init_dcpred(&c->vol.dcpred);

    if (decode_picture_header(br, &picture) < 0) return -1;
    if (c->width != picture.width || c->height != picture.height) return -1; //size changed..

    copy_vol(&picture, &c->vol);

    if (picture.picture_type == FLV_I_TYPE)
    {
        c->icounter = 0;
    }
    else
    {
        if (write_pad_not_coded_frames(pub_ctx, c, bw, time) < 0) return -1;
    }

    if (write_m4v_picture_frame(pub_ctx, c, br, bw, &picture, time) < 0)
    {
        return -1;
    }

    return 0;
}

int flv2mpeg4_process_flv_packet(flv2mpeg4_CTX *ctx, uint8 picture_type, const uint8 *buf, uint32 size, uint32 time)
{
    CTX *p = ctx->priv;
    BR br;
    BW bw;
    init_br(&br, buf, size);
    init_bw(&bw, p->out_buf, PACKETBUFFER_SIZE);
    write_m4v_frame(ctx, &p->conv, &br, &bw, time);
    return 0;
}

int flv2mpeg4_prepare_extra_data(flv2mpeg4_CTX *ctx)
{
    CTX *p = ctx->priv;
    BW bw;
    CONVCTX *c = &(p->conv);

    M4V_VOP vop;
    memset(&vop, 0, sizeof(vop));

    init_bw(&bw, p->out_buf, PACKETBUFFER_SIZE);

    c->vol.width = c->width;
    c->vol.height = c->height;
    c->vol.time_bits = VOL_TIME_BITS; // 0-31

    m4v_encode_m4v_header(&bw, &c->vol, 0);

    m4v_stuffing(&bw);
    flash_bw(&bw);

    alloc_dcpred(&c->vol.dcpred, (c->width+15) / 16, (c->height+15) / 16);
    
    return ctx->write_extradata_cb(ctx->usr_data, c->width, c->height, 200 * 1000, bw.buf, bw.pos);
}

void flv2mpeg4_set_frame(flv2mpeg4_CTX *ctx, int frame, int icounter)
{
    CTX *p = ctx->priv;
    p->conv.frame = frame;
    p->conv.icounter = icounter;
}

flv2mpeg4_CTX *flv2mpeg4_init_ctx(void *priv_data, int width, int height, flv2mpeg4_write_packet_cb wp_cb, flv2mpeg4_write_extradata_cb we_cb)
{
    flv2mpeg4_CTX *pub_ctx = malloc(sizeof(flv2mpeg4_CTX));
    memset(pub_ctx, 0x0, sizeof(flv2mpeg4_CTX));
    pub_ctx->usr_data = priv_data;
    pub_ctx->write_packet_cb = wp_cb;
    pub_ctx->write_extradata_cb = we_cb;
    pub_ctx->priv = malloc(sizeof(CTX));
    memset(pub_ctx->priv, 0x0, sizeof(CTX));
    CTX *ctx = pub_ctx->priv;
    
    ctx->conv.width  = width;
    ctx->conv.height = height;
    ctx->out_buf = malloc(PACKETBUFFER_SIZE);
    memset(ctx->out_buf, 0x0, PACKETBUFFER_SIZE);
    memset(&(ctx->vol), 0x0, sizeof(ctx->vol));
    
    return pub_ctx;
}

void flv2mpeg4_release_ctx(flv2mpeg4_CTX **pub_ctx)
{
    CTX *ctx = (*pub_ctx)->priv;
    
    free_dcpred(&ctx->conv.vol.dcpred);
    free(ctx->out_buf);
    free(ctx);
    free(*pub_ctx);
    *pub_ctx = NULL;
}



