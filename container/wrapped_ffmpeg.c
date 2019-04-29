/* 
 * Wrapper functions for FFMPEG API which 
 * allows to compile and use exteplayer3 
 * with old ffmpeg libs
 */
static void wrapped_frame_free(void *param)
{
#if (LIBAVCODEC_VERSION_MAJOR >= 55)
    av_frame_free(param);
#else
    avcodec_free_frame(param);
#endif
}

static void * wrapped_frame_alloc()
{
#if (LIBAVCODEC_VERSION_MAJOR >= 55)
    return av_frame_alloc();
#else
    return avcodec_alloc_frame();
#endif
}

static void wrapped_frame_unref(void *param)
{
#if (LIBAVCODEC_VERSION_MAJOR >= 55)
    av_frame_unref(param);
#else
    avcodec_get_frame_defaults(param);
#endif
}

static void wrapped_packet_unref(void *param)
{
#if (LIBAVCODEC_VERSION_MAJOR > 55)
    av_packet_unref(param);
#else
    av_free_packet(param);
#endif
}

static void wrapped_set_max_analyze_duration(void *param, int val)
{
#if (LIBAVFORMAT_VERSION_MAJOR > 55) && (LIBAVFORMAT_VERSION_MAJOR < 56)
    ((AVFormatContext *)param)->max_analyze_duration2 = val;
#else
    ((AVFormatContext *)param)->max_analyze_duration = 1;
#endif
}

static int64_t get_packet_duration(AVPacket *packet)
{
#if LIBAVFORMAT_VERSION_MAJOR > 56
    return packet->duration;
#else
    return packet->convergence_duration;
#endif
}

#if (LIBAVFORMAT_VERSION_MAJOR > 57) || ((LIBAVFORMAT_VERSION_MAJOR == 57) && (LIBAVFORMAT_VERSION_MINOR > 32))
static AVCodecParameters *get_codecpar(AVStream *stream)
{
    return stream->codecpar;
} 
#else
static AVCodecContext *get_codecpar(AVStream *stream)
{
    return stream->codec;
}
#endif

static AVRational get_frame_rate(AVStream *stream)
{
    AVRational rateRational = stream->avg_frame_rate;
    if (0 == rateRational.den)
    {
        rateRational = stream->r_frame_rate; 
    }
    return rateRational;
}

#if (LIBAVFORMAT_VERSION_MAJOR > 57) || ((LIBAVFORMAT_VERSION_MAJOR == 57) && (LIBAVFORMAT_VERSION_MINOR > 32))
typedef struct CodecCtxStoreItem_s {
    uint32_t cAVIdx;
    int id;
    AVCodecContext *avCodecCtx;
    void *next;
} CodecCtxStoreItem_t;

static CodecCtxStoreItem_t *g_codecCtxStoreListHead = NULL;

AVCodecContext * restore_avcodec_context(uint32_t cAVIdx, int32_t id)
{
    CodecCtxStoreItem_t *ptr = g_codecCtxStoreListHead;
    while (ptr != NULL)
    {
        if (ptr->cAVIdx == cAVIdx && ptr->id == id)
        {
            return ptr->avCodecCtx;
        }
        ptr = ptr->next;
    }
    return NULL;
}

void free_all_stored_avcodec_context()
{
    while (g_codecCtxStoreListHead != NULL)
    {
        CodecCtxStoreItem_t *ptr = g_codecCtxStoreListHead->next;
        free(g_codecCtxStoreListHead);
        g_codecCtxStoreListHead = ptr;
    }
}

int store_avcodec_context(AVCodecContext *avCodecCtx, uint32_t cAVIdx, int id)
{
    CodecCtxStoreItem_t *ptr = malloc(sizeof(CodecCtxStoreItem_t));
    if (!ptr)
    {
        return -1;
    }
    
    memset(ptr, 0x00, sizeof(CodecCtxStoreItem_t));
    ptr->next = g_codecCtxStoreListHead;
    g_codecCtxStoreListHead = ptr;
    
    return 0;
}
#else 
void free_all_stored_avcodec_context()
{
}
#endif

static AVCodecContext *wrapped_avcodec_get_context(uint32_t cAVIdx, AVStream *stream)
{
#if (LIBAVFORMAT_VERSION_MAJOR > 57) || ((LIBAVFORMAT_VERSION_MAJOR == 57) && (LIBAVFORMAT_VERSION_MINOR > 32))
    AVCodecContext *avCodecCtx = restore_avcodec_context(cAVIdx, stream->id);
    if (!avCodecCtx)
    {
        avCodecCtx = avcodec_alloc_context3(NULL);
        if (!avCodecCtx) 
        {
            ffmpeg_err("context3 alloc for stream %d failed\n", (int)stream->id);
            return NULL;
        }

        if (avcodec_parameters_to_context(avCodecCtx, stream->codecpar) < 0)
        {
            ffmpeg_err("parameters to context for stream %d failed\n", (int)stream->id);
            avcodec_free_context(&avCodecCtx);
            return NULL;
        }
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(58, 9, 100)
        av_codec_set_pkt_timebase(avCodecCtx, stream->time_base);
#else
        avCodecCtx->pkt_timebase = stream->time_base;
#endif
        store_avcodec_context(avCodecCtx, cAVIdx, stream->id);

        return avCodecCtx;
    }
#else
    return stream->codec;
#endif
}

static void wrapped_avcodec_flush_buffers(uint32_t cAVIdx)
{
#if (LIBAVFORMAT_VERSION_MAJOR > 57) || ((LIBAVFORMAT_VERSION_MAJOR == 57) && (LIBAVFORMAT_VERSION_MINOR > 32))
    CodecCtxStoreItem_t *ptr = g_codecCtxStoreListHead;
    while (ptr != NULL)
    {
        if (ptr->cAVIdx == cAVIdx && ptr->avCodecCtx && ptr->avCodecCtx->codec)
        {
            avcodec_flush_buffers(ptr->avCodecCtx);
        }
        ptr = ptr->next;
    }
#else
    uint32_t j;
    for (j = 0; j < avContextTab[cAVIdx]->nb_streams; j++)
    {
        if (avContextTab[cAVIdx]->streams[j]->codec && avContextTab[cAVIdx]->streams[j]->codec->codec)
        {
            avcodec_flush_buffers(avContextTab[cAVIdx]->streams[j]->codec);
        }
    }
#endif
}

static void wrapped_register_all(void)
{
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(58, 9, 100)
    avcodec_register_all();
    av_register_all();
#endif
}

static int64_t wrapped_frame_get_best_effort_timestamp(const AVFrame *frame)
{
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(58, 9, 100)
    return av_frame_get_best_effort_timestamp(frame);
#else
    return frame->best_effort_timestamp;
#endif
}


