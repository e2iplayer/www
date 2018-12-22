//
// mpeg4-part2 in mipsel receivers
// http://forums.openpli.org/topic/39326-gstreamer10-and-mpeg4-part2/?hl=%2Bmpeg4+%2Bpart2
//

// mpeg4_unpack_bframes
typedef struct
{
    const AVBitStreamFilter *bsf;
    AVBSFContext *ctx;
} Mpeg4P2Context;

static Mpeg4P2Context * mpeg4p2_context_open()
{
    Mpeg4P2Context *context = NULL;
    const AVBitStreamFilter *bsf = av_bsf_get_by_name("mpeg4_unpack_bframes");
    if (bsf) {
        context = malloc(sizeof(Mpeg4P2Context));
        if (context) {
            memset(context, 0x00, sizeof(Mpeg4P2Context));
            context->bsf = bsf;
        }
    }
    return context;
}

static void mpeg4p2_write(Context_t *ctx, Mpeg4P2Context *mpeg4p2_ctx, Track_t *track, int64_t start_time, int64_t *currentVideoPts, int64_t *latestPts, AVPacket *pkt)
{
    *currentVideoPts = track->pts = doCalcPts(start_time, mpeg4p2_ctx->ctx->time_base_out, pkt->pts);
    if ((*currentVideoPts > *latestPts) && (*currentVideoPts != INVALID_PTS_VALUE)) {
        *latestPts = *currentVideoPts;
    }

    track->dts = doCalcPts(start_time, mpeg4p2_ctx->ctx->time_base_out, pkt->dts);

    AudioVideoOut_t avOut;
    avOut.data       = pkt->data;
    avOut.len        = pkt->size;
    avOut.pts        = track->pts;
    avOut.dts        = track->dts;
    avOut.extradata  = mpeg4p2_ctx->ctx->par_out->extradata;
    avOut.extralen   = mpeg4p2_ctx->ctx->par_out->extradata_size;
    avOut.frameRate  = track->frame_rate;
    avOut.timeScale  = track->TimeScale;
    avOut.width      = track->width;
    avOut.height     = track->height;
    avOut.type       = "video";

    if (Write(ctx->output->video->Write, ctx, &avOut, avOut.pts) < 0) {
        ffmpeg_err("writing data to video device failed\n");
    }
}

static int mpeg4p2_context_reset(Mpeg4P2Context *context)
{
    int ret = 0;
    if (context && context->ctx) {
        // Flush
        ret = av_bsf_send_packet(context->ctx, NULL);
        if (ret == 0) {
            AVPacket *pkt = NULL;
            while ((ret = av_bsf_receive_packet(context->ctx, pkt)) == 0) {
                wrapped_frame_unref(pkt);
            }
        }
        av_bsf_free(&context->ctx);
    }

    return ret;
}

static int mpeg4p2_write_packet(Context_t *ctx, Mpeg4P2Context *mpeg4p2_ctx, Track_t *track, int cAVIdx, int64_t *pts_current, int64_t *pts_latest, AVPacket *pkt)
{
    int ret = 0;
    if (mpeg4p2_ctx) {
        // Setup is needed
        if (!mpeg4p2_ctx->ctx) {
            ret = av_bsf_alloc(mpeg4p2_ctx->bsf, &mpeg4p2_ctx->ctx);
            if (ret == 0) {
                AVStream *in = track->stream;
                ret = avcodec_parameters_copy(mpeg4p2_ctx->ctx->par_in, in->codecpar);
                if (ret == 0) {
                    mpeg4p2_ctx->ctx->time_base_in = in->time_base;
                    ret = av_bsf_init(mpeg4p2_ctx->ctx);
                }
            }
        }

        if (ret == 0) {
            ret = av_bsf_send_packet(mpeg4p2_ctx->ctx, pkt);
            if (ret == 0) {
                while ((ret = av_bsf_receive_packet(mpeg4p2_ctx->ctx, pkt)) == 0) {
                    mpeg4p2_write(ctx, mpeg4p2_ctx, track, avContextTab[cAVIdx]->start_time, pts_current, pts_latest, pkt);
                }

                if (ret == AVERROR(EAGAIN)) {
                    return 0;
                }

                if (ret < 0) {
                    ffmpeg_err("av_bsf_receive_packet failed error 0x%x\n", ret);
                    mpeg4p2_context_reset(mpeg4p2_ctx);
                }
            }
        } else {
            ffmpeg_err("bsf setup failed error 0x%x\n", ret);
        }
        
    } else {
        ret = -1;
    }
    return ret;
}

static void mpeg4p2_context_close(Mpeg4P2Context *context)
{
    if (context) {
        mpeg4p2_context_reset(context);
        free(context);
        return;
    }
}

