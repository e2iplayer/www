//
// mpeg4-part2 in mipsel receivers
// http://forums.openpli.org/topic/39326-gstreamer10-and-mpeg4-part2/?hl=%2Bmpeg4+%2Bpart2
//

#define MPEG4P2_MAX_B_FRAMES_COUNT 5

typedef struct
{
    int b_frames_count;
    int first_ip_frame_written;
    int64_t packet_duration;
    AVPacket *b_frames[MPEG4P2_MAX_B_FRAMES_COUNT];
    AVPacket *second_ip_frame;
} Mpeg4P2Context;


static void set_packet(AVPacket **pkt_dest, AVPacket *pkt_src)
{
    if (pkt_dest == NULL)
        return;
    if (*pkt_dest != NULL)
    {
        wrapped_packet_unref(*pkt_dest);
        av_free(*pkt_dest);
    }
    *pkt_dest = av_malloc(sizeof(AVPacket));
    av_copy_packet(*pkt_dest, pkt_src);
}

static int filter_packet(AVBitStreamFilterContext *bsf_ctx, AVCodecContext *enc_ctx, AVPacket *pkt)
{
    int ret;
    AVPacket new_pkt = *pkt;
    ret = av_bitstream_filter_filter(bsf_ctx, enc_ctx, NULL,
            &new_pkt.data, &new_pkt.size,
            pkt->data, pkt->size,
            pkt->flags & AV_PKT_FLAG_KEY);
    if (ret == 0 && new_pkt.data != pkt->data)
    {
        if ((ret = av_copy_packet(&new_pkt, pkt)) < 0)
            return -1;
        ret = 1;
    }
    if (ret > 0)
    {
        pkt->side_data = NULL;
        pkt->side_data_elems = 0;
        wrapped_packet_unref(pkt);
        new_pkt.buf = av_buffer_create(new_pkt.data, new_pkt.size,
                av_buffer_default_free, NULL, 0);
        if (!new_pkt.buf)
            return -1;
    }
    if (ret < 0)
    {
        ffmpeg_err("Failed to filter bitstream with filter %s for stream %d with codec %s\n",
                bsf_ctx->filter->name, pkt->stream_index,
                avcodec_get_name(enc_ctx->codec_id));
        return -1;
    }
    *pkt = new_pkt;
    return 0;
}

static void mpeg4p2_context_reset(Mpeg4P2Context *context)
{
    if (context == NULL)
        return;
    int i;
    for (i=0; i < MPEG4P2_MAX_B_FRAMES_COUNT; i++)
    {
        if (context->b_frames[i] != NULL)
        {
            wrapped_packet_unref(context->b_frames[i]);
            av_free(context->b_frames[i]);
        }
        context->b_frames[i] = NULL;
    }
    if (context->second_ip_frame != NULL)
    {
        wrapped_packet_unref(context->second_ip_frame);
        av_free(context->second_ip_frame);
    }
    context->second_ip_frame = NULL;

    context->b_frames_count = 0;
    context->first_ip_frame_written = 0;
    context->packet_duration = 0;
}

static void mpeg4p2_write(Context_t *ctx, Track_t *track, int avContextIdx, int64_t *currentVideoPts, int64_t *latestPts, AVPacket *pkt)
{
    *currentVideoPts = track->pts = calcPts(avContextIdx, track->stream, pkt->pts);
    if ((*currentVideoPts > *latestPts) && (*currentVideoPts != INVALID_PTS_VALUE))
    {
        *latestPts = *currentVideoPts;
    }
    track->dts = calcPts(avContextIdx, track->stream, pkt->dts);

    AudioVideoOut_t avOut;
    avOut.data       = pkt->data;
    avOut.len        = pkt->size;
    avOut.pts        = track->pts;
    avOut.dts        = track->dts;
    avOut.extradata  = track->extraData;
    avOut.extralen   = track->extraSize;
    avOut.frameRate  = track->frame_rate;
    avOut.timeScale  = track->TimeScale;
    avOut.width      = track->width;
    avOut.height     = track->height;
    avOut.type       = "video";

    if (ctx->output->video->Write(ctx, &avOut) < 0)
    {
        ffmpeg_err("writing data to video device failed\n");
    }
}

static int mpeg4p2_write_packet(Context_t *ctx, Mpeg4P2Context *mpeg4p2_ctx, Track_t *track, int cAVIdx, int64_t *pts_current, int64_t *pts_latest, AVPacket *pkt)
{
    uint8_t *data = pkt->data;
    int data_len = pkt->size;
    int pos = 0;
    if (mpeg4p2_ctx->packet_duration == 0)
    {
        mpeg4p2_ctx->packet_duration = pkt->duration;
    }
    while (pos < data_len)
    {
        if (memcmp(&data[pos], "\x00\x00\x01\xb6", 4))
        {
            pos++;
            continue;
        }
        pos += 4;
        switch ((data[pos] & 0xC0) >> 6)
        {
            case 0: // I-Frame
            case 1: // P-Frame
                if (!mpeg4p2_ctx->first_ip_frame_written)
                {
                    mpeg4p2_ctx->first_ip_frame_written = 1;
                    pkt->pts = pkt->dts + mpeg4p2_ctx->packet_duration;
                    ffmpeg_printf(100, "Writing first I/P packet\n");
                    mpeg4p2_write(ctx, track, cAVIdx, pts_current, pts_latest, pkt);
                    return 0;
                }
                else if (!mpeg4p2_ctx->second_ip_frame)
                {
                    set_packet(&mpeg4p2_ctx->second_ip_frame, pkt);
                    return 0;
                }
                else
                {
                    if (!mpeg4p2_ctx->b_frames_count)
                    {
                        mpeg4p2_ctx->second_ip_frame->pts = mpeg4p2_ctx->second_ip_frame->dts + mpeg4p2_ctx->packet_duration;
                        ffmpeg_printf(100,"Writing second I/P packet(1)\n");
                        mpeg4p2_write(ctx, track, cAVIdx, pts_current, pts_latest, mpeg4p2_ctx->second_ip_frame);
                        set_packet(&mpeg4p2_ctx->second_ip_frame, pkt);
                        return 0;
                    }
                    else
                    {
                        mpeg4p2_ctx->second_ip_frame->pts = mpeg4p2_ctx->b_frames[mpeg4p2_ctx->b_frames_count -1]->dts + mpeg4p2_ctx->packet_duration;
                        mpeg4p2_ctx->b_frames[0]->pts = mpeg4p2_ctx->second_ip_frame->dts + mpeg4p2_ctx->packet_duration;
                        int i;
                        for (i =1; i < mpeg4p2_ctx->b_frames_count; i++)
                        {
                            mpeg4p2_ctx->b_frames[i]->pts = mpeg4p2_ctx->b_frames[i-1]->dts + mpeg4p2_ctx->packet_duration;
                        }
                        ffmpeg_printf(100, "Writing second I/P packet(2)\n");
                        mpeg4p2_write(ctx, track, cAVIdx, pts_current, pts_latest, mpeg4p2_ctx->second_ip_frame);
                        set_packet(&mpeg4p2_ctx->second_ip_frame, pkt);
                        for (i =0; i< mpeg4p2_ctx->b_frames_count; i++)
                        {
                            ffmpeg_printf(100, "Writing B-frame[%d]\n", i);
                            mpeg4p2_write(ctx, track, cAVIdx, pts_current, pts_latest, mpeg4p2_ctx->b_frames[i]);
                        }
                        mpeg4p2_ctx->b_frames_count = 0;
                        return 0;
                    }
                }
                break;
            case 3: // S-Frame
                break;
            case 2: // B-Frame
                if (!mpeg4p2_ctx->second_ip_frame)
                {
                    ffmpeg_err("Cannot predict B-Frame without surrounding I/P-Frames, dropping...");
                    return 0;
                }
                if (mpeg4p2_ctx->b_frames_count == MPEG4P2_MAX_B_FRAMES_COUNT)
                {
                    ffmpeg_err("Oops max B-Frames count = %d, reached", MPEG4P2_MAX_B_FRAMES_COUNT);
                    // not recoverable, to fix just increase MPEG4P2_MAX_B_FRAMES_COUNT
                    return -1;
                }
                else
                {
                    ffmpeg_printf(100, "Storing B-Frame\n");
                    set_packet(&mpeg4p2_ctx->b_frames[mpeg4p2_ctx->b_frames_count++], pkt);
                    return 0;
                }
            case 4:
            default:
                break;
        }
    }
    return 0;
}

