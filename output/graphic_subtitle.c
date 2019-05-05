/*
 * Subtitle output to one registered client.
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

/* ***************************** */
/* Includes                      */
/* ***************************** */
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/mem.h>

#include "common.h"
#include "debug.h"
#include "writer.h"
#include "plugins/png.h"

/* ***************************** */
/* Makros/Constants              */
/* ***************************** */
#define MAX_RECT_DESC 4

/* ***************************** */
/* Types                         */
/* ***************************** */
typedef struct
{
    AVCodecContext *p_context;
    struct SwsContext *p_swctx;
    const AVCodec  *p_codec;
    bool b_need_ephemer; /* Does the format need the ephemer flag (no end time set) */
} decoder_sys_t;

typedef struct
{
    char filename[50];
    int x;
    int y;
    int w;
    int h;
} rec_desc_t;

/* ***************************** */
/* Varaibles                     */
/* ***************************** */
static decoder_sys_t *g_sys;

/* ***************************** */
/* Prototypes                    */
/* ***************************** */

/* ***************************** */
/* MISC Functions                */
/* ***************************** */
#include <dirent.h>
#include <fcntl.h>
#include <limits.h>
#include <fnmatch.h>
#include <sys/stat.h>
#include <sys/types.h>

static bool IsRegular(const char *pPath)
{
    struct stat st = {0};
    if(0 == lstat(pPath, &st) && S_ISREG(st.st_mode))
    {
        return true;
    }

    return false;
}

static void RemoveAllRegularFiles(const char *mainDir, const char *filePattern)
{
    if (!mainDir || !filePattern) return;
    DIR *dirp = opendir(mainDir);
    if(0 == dirp) return;

    struct dirent *pDir = 0;
    char *fullpath = malloc(PATH_MAX+2); 
    if (fullpath)
    {
        while (1)
        {
            pDir = readdir(dirp);
            if(0 == pDir) break;

            if (pDir->d_type != DT_REG && pDir->d_type != DT_UNKNOWN) continue;
            snprintf(fullpath, PATH_MAX, "%s/%s", mainDir, pDir->d_name);
            if (pDir->d_type == DT_UNKNOWN && !IsRegular(fullpath)) continue;
            if(0 == fnmatch(filePattern, pDir->d_name, 0)) remove(fullpath);
        }
    }
    free(fullpath);
    closedir(dirp);
}

/* ***************************** */
/* Functions                     */
/* ***************************** */
static int32_t Reset()
{
    if (g_sys)
        avcodec_flush_buffers(g_sys->p_context);
    
    RemoveAllRegularFiles(GetGraphicSubPath(), "[0-9]*_[0-9]*_[0-9]*.png");

    return 0;
}

static int32_t Open(SubtitleCodecId_t codecId, uint8_t *extradata, int extradata_size)
{
    enum AVCodecID avCodecId = AV_CODEC_ID_NONE;
    const AVCodec *codec;
    bool b_need_ephemer = false;

    /* */
    switch (codecId) {
    case SUBTITLE_CODEC_ID_PGS:
        avCodecId = AV_CODEC_ID_HDMV_PGS_SUBTITLE;
        b_need_ephemer = true;
        break;
    case SUBTITLE_CODEC_ID_DVB:
        avCodecId = AV_CODEC_ID_DVB_SUBTITLE;
        b_need_ephemer = true;
        break;
    case SUBTITLE_CODEC_ID_XSUB:
        avCodecId = AV_CODEC_ID_XSUB;
        break;
    default:
        subtitle_err("unsupported subtitle codecId: %d\n", (int)codecId);
        return -1;
    }

    codec = avcodec_find_decoder( avCodecId );
    AVCodecContext *context = avcodec_alloc_context3(codec);

    Reset();

    if (context == NULL)
        return -1;

    g_sys = malloc(sizeof(*g_sys));
    if (g_sys == NULL)
    {
        avcodec_free_context(&context);
        return -1;
    }

    g_sys->p_context = context;
    g_sys->p_codec = codec;

    /* this mean that new subtitles atom overwrite the previous one */
    g_sys->b_need_ephemer = b_need_ephemer;

    g_sys->p_swctx = NULL;

    /* */
    context->extradata = extradata;
    context->extradata_size = extradata_size;

    //av_codec_set_pkt_timebase(context, AV_TIME_BASE_Q);
    context->pkt_timebase = AV_TIME_BASE_Q;

    int ret = avcodec_open2(context, codec, NULL);

    if (ret < 0) {
        free(g_sys);
        avcodec_free_context(&context);
        return -1;
    }

    /* Lazy PNG plugin init */
    ret = PNGPlugin_init();
    if (0 != ret)
    {
        /* Report plugin error */
        E2iSendMsg("{\"e_plugin\":[\"png\",\"init\",%d]}\n", ret);
    }

    return 0;
}

static int32_t Close()
{
    if (g_sys)
    {
        AVCodecContext *ctx = g_sys->p_context;

        if (ctx)
        {
            /* extradata is not allocated by us */
            ctx->extradata = NULL;
            ctx->extradata_size = 0;

            avcodec_free_context(&ctx);
        }
        sws_freeContext(g_sys->p_swctx);
        free(g_sys);
        g_sys = NULL;
    }
    Reset();
    return 0;
}

static int32_t Write(WriterSubCallData_t *subPacket)
{
    if (!subPacket)
        return -1;

    if (!g_sys)
        if (Open(subPacket->codecId, subPacket->private_data, subPacket->private_size))
            return -1;

    AVSubtitle subtitle;
    memset(&subtitle, 0, sizeof(subtitle));

    AVPacket pkt;
    av_init_packet(&pkt);
    pkt.data = subPacket->data;
    pkt.size = subPacket->len;
    pkt.pts  = subPacket->pts;

    int has_subtitle = 0;
    int used = avcodec_decode_subtitle2(g_sys->p_context, &subtitle, &has_subtitle, &pkt);
    uint32_t width = g_sys->p_context->width > 0 ? g_sys->p_context->width : subPacket->width;
    uint32_t height = g_sys->p_context->height > 0 ? g_sys->p_context->height : subPacket->height;
    if (has_subtitle && width > 0 && height > 0)
    {
        uint32_t i = 0;
        uint32_t j = 0;

        uint64_t startTimestamp = subPacket->pts / 90  + subtitle.start_display_time;
        uint64_t endTimestamp = subPacket->pts / 90  + subtitle.end_display_time;
        rec_desc_t desc_tab[MAX_RECT_DESC];

        for (; i < subtitle.num_rects && j < MAX_RECT_DESC; i++)
        {
            AVSubtitleRect *rec = subtitle.rects[i];

            switch (subtitle.format)
            {
            case 0: /* 0 = graphics */
            {
                snprintf(desc_tab[j].filename, sizeof(desc_tab[j].filename), "%u_%"PRId64"_%u.png", subPacket->trackId, startTimestamp, i);

                ssize_t bufsz = snprintf(NULL, 0, "%s/%s", GetGraphicSubPath(), desc_tab[j].filename);
                char *filepath = malloc(bufsz + 1);
                if (!filepath)
                {
                    subtitle_err("out of memory\n");
                    break;
                }
                snprintf(filepath, bufsz + 1, "%s/%s", GetGraphicSubPath(), desc_tab[j].filename);

                desc_tab[j].x = av_rescale(rec->x, GetGraphicWindowWidth(), width);
                desc_tab[j].y = av_rescale(rec->y, GetGraphicWindowHeight(), height);
                desc_tab[j].w = av_rescale(rec->w, GetGraphicWindowWidth(), width);
                desc_tab[j].h = av_rescale(rec->h, GetGraphicWindowHeight(), height);

                subtitle_printf(50, "SUB_REC: src  x[%d], y[%d], %dx%d\n", rec->x, rec->y, rec->w, rec->h);
                subtitle_printf(50, "SUB_REC: dest x[%d], y[%d], %dx%d\n", desc_tab[j].x, desc_tab[j].y, desc_tab[j].w, desc_tab[j].h);

                uint8_t *data[AV_NUM_DATA_POINTERS] = {NULL};
                int linesize[AV_NUM_DATA_POINTERS] = {0};
 
                data[0] = av_malloc(desc_tab[j].w * desc_tab[j].h * 4);
                linesize[0] = desc_tab[j].w * 4;

                if (!data[0])
                {
                    subtitle_err("out of memory\n");
                    free(filepath);
                    break;
                }

                g_sys->p_swctx = sws_getCachedContext(g_sys->p_swctx, rec->w, rec->h, AV_PIX_FMT_PAL8, desc_tab[j].w, desc_tab[j].h, AV_PIX_FMT_RGBA, SWS_BICUBIC, NULL, NULL, NULL); // SWS_FAST_BILINEAR
                sws_scale(g_sys->p_swctx, (const uint8_t * const *)rec->data, rec->linesize, 0, rec->h, data, linesize);

                if (0 == PNGPlugin_saveRGBAImage(filepath, &(data[0][0]), desc_tab[j].w, desc_tab[j].h))
                {
                    j += 1;
                }

                av_freep(&data[0]);

                free(filepath);
                break;
            }
            default:
                subtitle_err("unsupported subtitle type\n");
                break;
            }
        }

        char sep[2] = {'\0'};
        E2iStartMsg();
        E2iSendMsg("{\"s_a\":{\"id\":%d,\"s\":%"PRId64, subPacket->trackId, startTimestamp);
        if (g_sys->b_need_ephemer)
            E2iSendMsg(",\"e\":null,\"r\":[");
        else
            E2iSendMsg(",\"e\":%"PRId64",\"r\":[", endTimestamp);

        for (i = 0; i < j; i++)
        {
            E2iSendMsg("%s{\"x\":%d,\"y\":%d,\"w\":%d,\"h\":%d,\"f\":\"%s\"}", sep, desc_tab[i].x, desc_tab[i].y, desc_tab[i].w, desc_tab[i].h, desc_tab[i].filename);
            sep[0] = ',';
        }
        E2iSendMsg("]}}\n");
        E2iEndMsg();
    }

    avsubtitle_free(&subtitle);

    return 0;
}

SubWriter_t WriterSubPGS = {
    NULL,
    Close,
    Reset,
    Write
};
