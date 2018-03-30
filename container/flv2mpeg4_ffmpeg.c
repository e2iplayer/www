//
// more info
// http://forum.doom9.org/archive/index.php/t-157998.html
//

#include "flv2mpeg4/flv2mpeg4.h"


typedef struct
{
    flv2mpeg4_CTX *ctx;
    uint8_t *extradata;
    int extradatasize;
    
    Context_t *out_ctx;
    Track_t   *track;
} Flv2Mpeg4Context;


static int flv2mpeg4_context_write_packet_cb(void *usr_data, int keyframe, int pts, const uint8_t *buf, int size)
{
    Flv2Mpeg4Context *ctx = usr_data;
    if (!ctx)
    {
        return -1;
    }
    
    AudioVideoOut_t avOut;
    avOut.data       = (char *)buf;
    avOut.len        = size;
    avOut.pts        = ctx->track->pts;
    avOut.dts        = ctx->track->dts;
    avOut.extradata  = ctx->extradata;
    avOut.extralen   = ctx->extradatasize;
    avOut.frameRate  = ctx->track->frame_rate;
    avOut.timeScale  = ctx->track->TimeScale;
    avOut.width      = ctx->track->width;
    avOut.height     = ctx->track->height;
    avOut.type       = "video";

    if (Write(ctx->out_ctx->output->video->Write, ctx->out_ctx, &avOut, avOut.pts) < 0)
    {
        ffmpeg_err("writing data to video device failed\n");
    }
    
    return 0;
}

static int flv2mpeg4_context_write_extradata_cb(void *usr_data, int width, int height, int bitrate, const uint8_t *extradata, int extradatasize)
{
    Flv2Mpeg4Context *ctx = usr_data;
    if (!ctx)
    {
        return -1;
    }
    
    free(ctx->extradata);
    ctx->extradata = malloc(extradatasize);
    memcpy(ctx->extradata, extradata, extradatasize);
    ctx->extradatasize = extradatasize;
    
    return 0;
}

static void flv2mpeg4_context_reset(Flv2Mpeg4Context *context)
{
    if (context == NULL || context->ctx == NULL)
        return;
    
    flv2mpeg4_set_frame(context->ctx, 0, 0);
}

static int flv2mpeg4_write_packet(Context_t *out_ctx, Flv2Mpeg4Context *mpeg4p2_ctx, Track_t *track, int cAVIdx, int64_t *pts_current, int64_t *pts_latest, AVPacket *pkt)
{
    if (!mpeg4p2_ctx->ctx)
    {
        mpeg4p2_ctx->ctx = flv2mpeg4_init_ctx(mpeg4p2_ctx, track->width, track->height, flv2mpeg4_context_write_packet_cb, flv2mpeg4_context_write_extradata_cb);
        flv2mpeg4_prepare_extra_data(mpeg4p2_ctx->ctx);
    }
    
    *pts_current = track->pts = calcPts(cAVIdx, track->stream, pkt->pts);
    if ((*pts_current > *pts_latest) && (*pts_current != INVALID_PTS_VALUE))
    {
        *pts_latest = *pts_current;
    }
    track->dts = calcPts(cAVIdx, track->stream, pkt->dts);
    
    mpeg4p2_ctx->out_ctx = out_ctx;    
    mpeg4p2_ctx->track = track;

    uint32_t time_ms = (uint32_t)(track->pts / 90);
    
    return flv2mpeg4_process_flv_packet(mpeg4p2_ctx->ctx, 0, pkt->data, pkt->size, time_ms);
}

