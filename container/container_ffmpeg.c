/*
 * Container handling for all stream's handled by ffmpeg
 * konfetti 2010; based on code from crow
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

 /* Support Large File */
#define _FILE_OFFSET_BITS 64

/* ***************************** */
/* Includes                      */
/* ***************************** */

#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <memory.h>
#include <string.h>

#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/poll.h>
#include <pthread.h>
#include <sys/prctl.h>
#include <stdint.h>

#include <libavutil/avutil.h>
#include <libavutil/time.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <libavutil/opt.h>

#include "common.h"
#include "misc.h"
#include "debug.h"
#include "aac.h"
#include "pcm.h"
#include "ffmpeg_metadata.h"
/* ***************************** */
/* Makros/Constants              */
/* ***************************** */
/* Some STB with old kernels have problem with default 
 * read/write functions in ffmpeg which use open/read
 * due to this we set own which use fopen/fread from
 * std library.
 */
#define SAM_CUSTOM_IO

//SULGE DEBUG ENABLED
//#define SAM_WITH_DEBUG
#ifdef SAM_WITH_DEBUG
#define FFMPEG_DEBUG
#else
#define FFMPEG_SILENT
#endif

//#define FFMPEG_DEBUG

#ifdef FFMPEG_DEBUG

static short debug_level = 1;

#define ffmpeg_printf(level, fmt, x...) do { \
if (debug_level >= level) printf("[%s:%s] " fmt, __FILE__, __FUNCTION__, ## x); } while (0)
#else
#define ffmpeg_printf(level, fmt, x...)
#endif

#ifndef FFMPEG_SILENT
#define ffmpeg_err(fmt, x...) do { printf("[%s:%s] " fmt, __FILE__, __FUNCTION__, ## x); } while (0)
#else
#define ffmpeg_err(fmt, x...)
#endif

/* Error Constants */
#define cERR_CONTAINER_FFMPEG_NO_ERROR        0
#define cERR_CONTAINER_FFMPEG_INIT           -1
#define cERR_CONTAINER_FFMPEG_NOT_SUPPORTED  -2
#define cERR_CONTAINER_FFMPEG_INVALID_FILE   -3
#define cERR_CONTAINER_FFMPEG_RUNNING        -4
#define cERR_CONTAINER_FFMPEG_NOMEM          -5
#define cERR_CONTAINER_FFMPEG_OPEN           -6
#define cERR_CONTAINER_FFMPEG_STREAM         -7
#define cERR_CONTAINER_FFMPEG_NULL           -8
#define cERR_CONTAINER_FFMPEG_ERR            -9
#define cERR_CONTAINER_FFMPEG_END_OF_FILE    -10

#define IPTV_AV_CONTEXT_MAX_NUM 2
/* ***************************** */
/* Types                         */
/* ***************************** */

/* ***************************** */
/* Varaibles                     */
/* ***************************** */

static pthread_mutex_t mutex;

static pthread_t PlayThread;
static int32_t hasPlayThreadStarted = 0;

static AVFormatContext *avContextTab[IPTV_AV_CONTEXT_MAX_NUM] = {NULL, NULL};
static int32_t use_custom_io[IPTV_AV_CONTEXT_MAX_NUM] = {0, 0};
static AVDictionary *avio_opts = NULL;

static uint8_t isContainerRunning = 0;

static int64_t latestPts = 0;

static int32_t restart_audio_resampling = 0;

static off_t seek_target_bytes = 0;
static int32_t do_seek_target_bytes = 0;

static int64_t seek_target_seconds = 0;
static int8_t do_seek_target_seconds = 0;

static int32_t seek_target_flag = 0;

/* ***************************** */
/* Prototypes                    */
/* ***************************** */
static int32_t container_ffmpeg_seek_bytes(off_t pos);
static int32_t container_ffmpeg_seek(Context_t *context, int64_t sec, uint8_t absolute);
static int32_t container_ffmpeg_seek_rel(Context_t *context, off_t pos, int64_t pts, int64_t sec);
static int32_t container_ffmpeg_get_length(Context_t *context, int64_t *length);
static int64_t calcPts(uint32_t avContextIdx, AVStream *stream, int64_t pts);

/* Progressive playback means that we play local file
 * but this local file can grows up, for example 
 * we start playback before download was finished 
 */
static int32_t progressive_download = 0;
void progressive_download_set(int32_t val)
{
    progressive_download = val;
}

/* This is very bad to include source file 
 * and must be corrected in the future
 */
#include "buff_ffmpeg.c"

#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(56, 34, 100)
#include "mpeg4p2_ffmpeg.c"
#endif

/* This is also bad solution 
 * such configuration should passed maybe 
 * via struct 
 */
static int32_t wma_software_decode = 0;
static int32_t aac_software_decode = 0;
static int32_t ac3_software_decode = 0;
static int32_t eac3_software_decode = 0;
static int32_t dts_software_decode = 0;
static int32_t pcm_resampling = 1;
static int32_t stereo_software_decoder = 0;
static int32_t insert_pcm_as_lpcm = 0;

/* ***************************** */
/* MISC Functions                */
/* ***************************** */

static void ffmpeg_silen_callback (void * avcl, int level, const char * fmt, va_list vl)
{
    return;
}

static int32_t mutexInitialized = 0;

void wma_software_decoder_set(int32_t val)
{
    wma_software_decode = val;
}

void aac_software_decoder_set(int32_t val)
{
    aac_software_decode = val;
}

void ac3_software_decoder_set(int32_t val)
{
    ac3_software_decode = val;
}

void eac3_software_decoder_set(int32_t val)
{
    eac3_software_decode = val;
}

void dts_software_decoder_set(int32_t val)
{
    dts_software_decode = val;
}

void stereo_software_decoder_set(int32_t val)
{
    stereo_software_decoder = val;
}

void insert_pcm_as_lpcm_set(int32_t val)
{
    insert_pcm_as_lpcm = val;
}

void pcm_resampling_set(int32_t val)
{
    pcm_resampling = val;
}

int32_t ffmpeg_av_dict_set(const char *key, const char *value, int32_t flags)
{
    return av_dict_set(&avio_opts, key, value, flags);
}

static void initMutex(void)
{
    pthread_mutex_init(&mutex, NULL);
    mutexInitialized = 1;
}

static void getMutex(const char *filename __attribute__((unused)), const char *function __attribute__((unused)), int32_t line)
{
    ffmpeg_printf(100, "::%d requesting mutex\n", line);

    if (!mutexInitialized)
    {
        initMutex();
    }

    pthread_mutex_lock(&mutex);

    ffmpeg_printf(100, "::%d received mutex\n", line);
}

static void releaseMutex(const char *filename __attribute__((unused)), const const char *function __attribute__((unused)), int32_t line) 
{
    pthread_mutex_unlock(&mutex);

    ffmpeg_printf(100, "::%d released mutex\n", line);
}

static char* Codec2Encoding(AVCodecContext *codec, int32_t *version)
{
    ffmpeg_printf(10, "Codec ID: %d (%.8lx)\n", (int32_t)codec->codec_id, (int32_t)codec->codec_id);
    switch (codec->codec_id)
    {
    case AV_CODEC_ID_MPEG1VIDEO:
        return "V_MPEG1";
    case AV_CODEC_ID_MPEG2VIDEO:
        return "V_MPEG1";
    case AV_CODEC_ID_H263:
    case AV_CODEC_ID_H263P:
    case AV_CODEC_ID_H263I:
        return "V_H263";
    case AV_CODEC_ID_FLV1:
        return "V_FLV";
    case AV_CODEC_ID_VP5:
    case AV_CODEC_ID_VP6:
    case AV_CODEC_ID_VP6F:
        return "V_VP6";
    case AV_CODEC_ID_RV10:
    case AV_CODEC_ID_RV20:
        return "V_RMV";
    case AV_CODEC_ID_MPEG4:
        return "V_MPEG4";
#if LIBAVCODEC_VERSION_MAJOR < 53
    case AV_CODEC_ID_XVID:
#endif
    case AV_CODEC_ID_MSMPEG4V1:
    case AV_CODEC_ID_MSMPEG4V2:
    case AV_CODEC_ID_MSMPEG4V3:
        return "V_DIVX3";
    case AV_CODEC_ID_WMV1:
        *version = 1;
        return "V_WMV";
    case AV_CODEC_ID_WMV2:
        *version = 2;
        return "V_WMV";
    case AV_CODEC_ID_WMV3:
        *version = 3;
        return "V_WMV";
    case AV_CODEC_ID_VC1:
        return "V_VC1";
    case AV_CODEC_ID_H264:
#if LIBAVCODEC_VERSION_MAJOR < 54
    case AV_CODEC_ID_FFH264:
#endif
        return "V_MPEG4/ISO/AVC";
    case AV_CODEC_ID_AVS:
        return "V_AVS";
    case AV_CODEC_ID_MP2:
        return "A_MPEG/L3";
    case AV_CODEC_ID_MP3:
        return "A_MP3";
    case AV_CODEC_ID_AAC:
        return (aac_software_decode) ? "A_IPCM" : "A_AAC";
    case AV_CODEC_ID_AC3:
        return  (ac3_software_decode) ? "A_IPCM" : "A_AC3";
    case AV_CODEC_ID_EAC3:
        return  (eac3_software_decode) ? "A_IPCM" : "A_EAC3";
    case AV_CODEC_ID_DTS:
        return (dts_software_decode) ? "A_IPCM" : "A_DTS";
    case AV_CODEC_ID_WMAV1:
    case AV_CODEC_ID_WMAV2:
            return (wma_software_decode) ? "A_IPCM" : "A_WMA";
    case 86056:
    case AV_CODEC_ID_WMAPRO:
        return (wma_software_decode) ? "A_IPCM" : "A_WMA/PRO";
    case AV_CODEC_ID_WMALOSSLESS:
        return "A_IPCM";
    case AV_CODEC_ID_MLP:
        return "A_IPCM";
    case AV_CODEC_ID_RA_144:
        return "A_IPCM";
    case AV_CODEC_ID_RA_288:
        return "A_IPCM";
    case AV_CODEC_ID_VORBIS:
        return "A_IPCM";
    case AV_CODEC_ID_FLAC:
        return "A_IPCM";
    case AV_CODEC_ID_PCM_S8:
    case AV_CODEC_ID_PCM_U8:
    case AV_CODEC_ID_PCM_S16LE:
    case AV_CODEC_ID_PCM_S16BE:
    case AV_CODEC_ID_PCM_U16LE:
    case AV_CODEC_ID_PCM_U16BE:
    case AV_CODEC_ID_PCM_S24LE:
    case AV_CODEC_ID_PCM_S24BE:
    case AV_CODEC_ID_PCM_U24LE:
    case AV_CODEC_ID_PCM_U24BE:
    case AV_CODEC_ID_PCM_S32LE:
    case AV_CODEC_ID_PCM_S32BE:
    case AV_CODEC_ID_PCM_U32LE:
    case AV_CODEC_ID_PCM_U32BE:
        return pcm_resampling ? "A_IPCM" : "A_PCM";
    case AV_CODEC_ID_AMR_NB:
        return "A_IPCM";//return "A_AMR";

/* In exteplayer3 embedded text subtitle simple printed
 * to output like other data.  Maybe worth to consider is to use 
 * linux socket or pipe to put
 */

/* subtitle */
    case AV_CODEC_ID_SSA:
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(55, 3, 100)
    case AV_CODEC_ID_ASS:
#endif
        return "S_TEXT/ASS"; /* Hellmaster1024: seems to be ASS instead of SSA */
    case AV_CODEC_ID_DVD_SUBTITLE:
    case AV_CODEC_ID_DVB_SUBTITLE:
    case AV_CODEC_ID_XSUB:
    case AV_CODEC_ID_MOV_TEXT:
    case AV_CODEC_ID_HDMV_PGS_SUBTITLE:
    case AV_CODEC_ID_DVB_TELETEXT:
//    case CODEC_ID_DVB_TELETEXT:
//        return "S_TEXT/SRT"; /* fixme */
    case AV_CODEC_ID_TEXT: ///< raw UTF-8 text
        return "S_TEXT/UTF-8";
    case AV_CODEC_ID_SRT:
        return "S_TEXT/SRT";
    case AV_CODEC_ID_SUBRIP:
        return "S_TEXT/SUBRIP"; 
    default:
        ffmpeg_err("Codec ID %d (%.8lx) not found\n", (int32_t)codec->codec_id, (int32_t)codec->codec_id);
        // Default to injected-pcm for unhandled audio types.
        if (codec->codec_type == AVMEDIA_TYPE_AUDIO)
        {
            return "A_IPCM";
        }
        /*
        if (codec->codec_type == AVMEDIA_TYPE_SUBTITLE)
        {
            return "S_TEXT/SRT";
        }
        */
        ffmpeg_err("Codec ID %d (%.8lx) not found\n", (int32_t)codec->codec_id, (int32_t)codec->codec_id);
    }
    return NULL;
}

static int64_t calcPts(uint32_t avContextIdx, AVStream *stream, int64_t pts)
{
    if (!stream || pts == (int64_t)AV_NOPTS_VALUE)
    {
        ffmpeg_err("stream / packet null\n");
        return INVALID_PTS_VALUE;
    }
    else if (stream->time_base.den > 0)
    {
        pts = av_rescale(pts, (int64_t)stream->time_base.num * 90000, stream->time_base.den);
    }
    
    if (avContextTab[avContextIdx]->start_time != AV_NOPTS_VALUE)
    {
        pts -= 90000 * avContextTab[avContextIdx]->start_time / AV_TIME_BASE;
    }

    if (pts & 0x8000000000000000ull)
    {
        pts = INVALID_PTS_VALUE;
    }

    return pts;
}

/* search for metatdata in context and stream
 * and map it to our metadata.
 */

#if LIBAVCODEC_VERSION_MAJOR < 54
static char* searchMeta(AVMetadata *metadata, char* ourTag)
#else
static char* searchMeta(AVDictionary * metadata, char* ourTag)
#endif
{
#if LIBAVCODEC_VERSION_MAJOR < 54
   AVMetadataTag *tag = NULL;
#else
   AVDictionaryEntry *tag = NULL;
#endif
   int i = 0;

   while (metadata_map[i] != NULL)
   {
      if (strcmp(ourTag, metadata_map[i]) == 0)
      {
#if LIBAVCODEC_VERSION_MAJOR < 54
          while ((tag = av_metadata_get(metadata, "", tag, AV_METADATA_IGNORE_SUFFIX)))
#else
          while ((tag = av_dict_get(metadata, "", tag, AV_DICT_IGNORE_SUFFIX)))
#endif
          {
              if (strcmp(tag->key, metadata_map[ i + 1 ]) == 0)
              {
                  return tag->value;
              }
          }
      }
      i++;
   }

   return NULL;
}

/* **************************** */
/* Worker Thread                */
/* **************************** */

static void FFMPEGThread(Context_t *context) 
{
    char threadname[17];
    strncpy(threadname, __func__, sizeof(threadname));
    threadname[16] = 0;
    prctl (PR_SET_NAME, (unsigned long)&threadname);
    AVPacket   packet;
    off_t   lastSeek = -1;
    int64_t lastPts = -1;
    int64_t currentVideoPts = -1;
    int64_t currentAudioPts = -1;
    int64_t showtime = 0;
    int64_t bofcount = 0;
    int32_t       err = 0;
    AudioVideoOut_t avOut;
    
    g_context = context;

    SwrContext *swr = NULL;
    AVFrame *decoded_frame = NULL;
    int32_t out_sample_rate = 44100;
    int32_t out_channels = 2;
    uint64_t out_channel_layout = AV_CH_LAYOUT_STEREO;
    uint32_t cAVIdx = 0;

#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(56, 34, 100)
    Mpeg4P2Context mpeg4p2_context;
    memset(&mpeg4p2_context, 0, sizeof(Mpeg4P2Context));
    AVBitStreamFilterContext *mpeg4p2_bsf_context = av_bitstream_filter_init("mpeg4_unpack_bframes");
#endif
    
    ffmpeg_printf(10, "\n");
    while ( context->playback->isCreationPhase )
    {
        ffmpeg_err("Thread waiting for end of init phase...\n");
        usleep(1000);
    }
    ffmpeg_printf(10, "Running!\n");

    int8_t isWaitingForFinish = 0;
    while ( context && context->playback && context->playback->isPlaying ) 
    {
        //IF MOVIE IS PAUSED, WAIT
        if (context->playback->isPaused) 
        {
            ffmpeg_printf(20, "paused\n");
            reset_finish_timeout();
            usleep(10000);
            continue;
        }

        if (context->playback->isSeeking) 
        {
            ffmpeg_printf(10, "seeking\n");
            reset_finish_timeout();
            usleep(10000);
            continue;
        }

        getMutex(__FILE__, __FUNCTION__,__LINE__);

        if (!context->playback || !context->playback->isPlaying)
        {
            releaseMutex(__FILE__, __FUNCTION__,__LINE__);
            if(!isWaitingForFinish)
            {
                reset_finish_timeout();
            }
            continue;
        }

        if (do_seek_target_seconds || do_seek_target_bytes) 
        {
            isWaitingForFinish = 0;
            if (do_seek_target_seconds)
            {
                ffmpeg_printf(10, "seek_target_seconds[%lld]\n", seek_target_seconds);
                uint32_t i = 0;
                for(i=0; i<IPTV_AV_CONTEXT_MAX_NUM; i+=1)
                {
                    if(NULL != avContextTab[i])
                    {
                        av_seek_frame(avContextTab[i], -1, seek_target_seconds, 0);
                    }
                    else
                    {
                        break;
                    }
                }
                reset_finish_timeout();
            } 
            else
            {
                container_ffmpeg_seek_bytes(seek_target_bytes);
            }
            do_seek_target_seconds = 0;
            do_seek_target_bytes = 0;
            
            restart_audio_resampling = 1;
            currentVideoPts = -1;
            currentAudioPts = -1;
            latestPts = 0;
            seek_target_flag = 0;

            // flush streams
            uint32_t i = 0;
            for(i=0; i<IPTV_AV_CONTEXT_MAX_NUM; i+=1)
            {
                if(NULL != avContextTab[i])
                {
                    uint32_t j;
                    for (j = 0; j < avContextTab[i]->nb_streams; j++)
                    {
                        if (avContextTab[i]->streams[j]->codec && avContextTab[i]->streams[j]->codec->codec)
                        {
                            avcodec_flush_buffers(avContextTab[i]->streams[j]->codec);
                        }
                    }
                }
                else
                {
                    break;
                }
            }
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(56, 34, 100)
            mpeg4p2_context_reset(&mpeg4p2_context);
            if (NULL != mpeg4p2_bsf_context)
            {
                av_bitstream_filter_close(mpeg4p2_bsf_context);
                mpeg4p2_bsf_context = av_bitstream_filter_init("mpeg4_unpack_bframes");
            }
#endif
        }

        int ffmpegStatus = 0;
        if(!isWaitingForFinish)
        {
            if(NULL != avContextTab[1])
            {
                cAVIdx = currentVideoPts <= currentAudioPts ? 0 : 1;
            }
            else
            {
                cAVIdx = 0;
            }
        }
        
        if (!isWaitingForFinish && (ffmpegStatus = av_read_frame(avContextTab[cAVIdx], &packet)) == 0 )
        {
            int64_t pts = 0;
            int64_t dts = 0;
            Track_t *videoTrack = NULL;
            Track_t *audioTrack = NULL;
            Track_t *subtitleTrack = NULL;

            int32_t pid = avContextTab[cAVIdx]->streams[packet.stream_index]->id;
            
            reset_finish_timeout();

            if (context->manager->video->Command(context, MANAGER_GET_TRACK, &videoTrack) < 0)
            {
                ffmpeg_err("error getting video track\n");
            }
            
            if (context->manager->audio->Command(context, MANAGER_GET_TRACK, &audioTrack) < 0)
            {
                ffmpeg_err("error getting audio track\n");
            }
            
            if (context->manager->subtitle->Command(context, MANAGER_GET_TRACK, &subtitleTrack) < 0)
            {
                ffmpeg_err("error getting subtitle track\n");
            }

            ffmpeg_printf(200, "packet.size %d - index %d\n", packet.size, pid);

            if (videoTrack && (videoTrack->AVIdx == cAVIdx) && (videoTrack->Id == pid))
            {
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(56, 34, 100)
                AVCodecContext *codec_context = ((AVStream*)(videoTrack->stream))->codec;
                if (codec_context->codec_id == AV_CODEC_ID_MPEG4 && NULL != mpeg4p2_bsf_context)
                {
                    // should never happen, if it does print error and exit immediately, so we can easily spot it
                    if (filter_packet(mpeg4p2_bsf_context, codec_context, &packet) < 0)
                    {
                        ffmpeg_err("cannot filter mpegp2 packet\n");
                        exit(1);
                    }
                    if (mpeg4p2_write_packet(context, &mpeg4p2_context, videoTrack, cAVIdx, &currentVideoPts, &latestPts, &packet) < 0)
                    {
                        ffmpeg_err("cannot write mpeg4p2 packet\n");
                        exit(1);
                    }
                    update_max_injected_pts(latestPts);
                }
                else
#endif
                {
                    currentVideoPts = videoTrack->pts = pts = calcPts(cAVIdx, videoTrack->stream, packet.pts);
                    videoTrack->dts = dts = calcPts(cAVIdx, videoTrack->stream, packet.dts);

                    if ((currentVideoPts > latestPts) && (currentVideoPts != INVALID_PTS_VALUE))
                    {
                        latestPts = currentVideoPts;
                        update_max_injected_pts(latestPts);
                    }
                    ffmpeg_printf(200, "VideoTrack index = %d %lld\n",pid, currentVideoPts);

                    avOut.data       = packet.data;
                    avOut.len        = packet.size;
                    avOut.pts        = pts;
                    avOut.dts        = dts;
                    avOut.extradata  = videoTrack->extraData;
                    avOut.extralen   = videoTrack->extraSize;
                    avOut.frameRate  = videoTrack->frame_rate;
                    avOut.timeScale  = videoTrack->TimeScale;
                    avOut.width      = videoTrack->width;
                    avOut.height     = videoTrack->height;
                    avOut.type       = "video";

                    if (context->output->video->Write(context, &avOut) < 0)
                    {
                        ffmpeg_err("writing data to video device failed\n");
                    }
                }
            }
            else if (audioTrack && (audioTrack->AVIdx == cAVIdx) && (audioTrack->Id == pid)) 
            {
                currentAudioPts = audioTrack->pts = pts = calcPts(cAVIdx, audioTrack->stream, packet.pts);

                if ((currentAudioPts != INVALID_PTS_VALUE) && (currentAudioPts > latestPts) && (!videoTrack))
                {
                    latestPts = currentAudioPts;
                    update_max_injected_pts(latestPts);
                }
                
                pcmPrivateData_t pcmExtradata;
                pcmExtradata.channels              = ((AVStream*) audioTrack->stream)->codec->channels;
                pcmExtradata.bits_per_coded_sample = ((AVStream*) audioTrack->stream)->codec->bits_per_coded_sample;
                pcmExtradata.sample_rate           = ((AVStream*) audioTrack->stream)->codec->sample_rate;
                pcmExtradata.bit_rate              = ((AVStream*) audioTrack->stream)->codec->bit_rate;
                pcmExtradata.ffmpeg_codec_id       = ((AVStream*) audioTrack->stream)->codec->codec_id;
                pcmExtradata.bResampling           = restart_audio_resampling;
                
                uint8_t *pAudioExtradata    = ((AVStream*) audioTrack->stream)->codec->extradata;
                uint32_t audioExtradataSize = ((AVStream*) audioTrack->stream)->codec->extradata_size;
                    
                ffmpeg_printf(200, "AudioTrack index = %d\n",pid);
                if (audioTrack->inject_raw_pcm == 1)
                {
                    ffmpeg_printf(200,"write audio raw pcm\n");
                    restart_audio_resampling = 0;

                    avOut.data       = packet.data;
                    avOut.len        = packet.size;
                    avOut.pts        = pts;
                    avOut.extradata  = (uint8_t *) &pcmExtradata;
                    avOut.extralen   = sizeof(pcmExtradata);
                    avOut.frameRate  = 0;
                    avOut.timeScale  = 0;
                    avOut.width      = 0;
                    avOut.height     = 0;
                    avOut.type       = "audio";

                    if (context->output->audio->Write(context, &avOut) < 0)
                    {
                        ffmpeg_err("(raw pcm) writing data to audio device failed\n");
                    }
                }
                else if (audioTrack->inject_as_pcm == 1)
                {
                    AVCodecContext *c = ((AVStream*)(audioTrack->stream))->codec;

                    if (restart_audio_resampling)
                    {
                        restart_audio_resampling = 0;
                        if (swr)
                        {
                            swr_free(&swr);
                            swr = NULL;
                        }
                        if (decoded_frame)
                        {

#if (LIBAVCODEC_VERSION_MAJOR >= 55)
                            av_frame_free(&decoded_frame);
#else
                            avcodec_free_frame(&decoded_frame);
#endif
                            decoded_frame = NULL;
                        }
                        /* It seems that this is not needed and make long black 
                         * on BRCM STBs, but maybe this is needed for SH4 STBs
                         * there is need to check
                         */
                        //context->output->Command(context, OUTPUT_CLEAR, NULL);
                        //context->output->Command(context, OUTPUT_PLAY, NULL);
                    }

                    while(packet.size > 0)
                    {
                        if(do_seek_target_seconds || do_seek_target_bytes)
                        {
                            break;
                        }
                        
                        int32_t got_frame = 0;
                        if (!decoded_frame)
                        {
#if (LIBAVCODEC_VERSION_MAJOR >= 55)
                            decoded_frame = av_frame_alloc();
#else
                            decoded_frame = avcodec_alloc_frame();
#endif
                            if (!decoded_frame)
                            {
                                ffmpeg_err("out of memory\n");
                                exit(1);
                            }
                        } 
                        else
                        {
#if (LIBAVCODEC_VERSION_MAJOR >= 55)
                            av_frame_unref(decoded_frame);
#else
                            avcodec_get_frame_defaults(decoded_frame);
#endif
                        }

                        int32_t len = avcodec_decode_audio4(c, decoded_frame, &got_frame, &packet);
                        if (len < 0) 
                        {
                            ffmpeg_err("avcodec_decode_audio4: %d\n", len);
                            break;
                        }

                        packet.data += len;
                        packet.size -= len;
                        
                        if (!got_frame)
                        {
                            continue;
                        }
                        
                        int32_t e = 0;
                        if (!swr) 
                        {
                            if(insert_pcm_as_lpcm)
                            {
                                out_sample_rate = 48000;
                            }
                            else
                            {
                                int32_t rates[] = { 48000, 96000, 192000, 44100, 88200, 176400, 0 };
                                int32_t *rate = rates;
                                int32_t in_rate = c->sample_rate;
                                while (*rate && ((*rate / in_rate) * in_rate != *rate) && (in_rate / *rate) * *rate != in_rate)
                                {
                                    rate++;
                                }
                                out_sample_rate = *rate ? *rate : 44100;
                            }
                            
                            swr = swr_alloc();
                            out_channels = c->channels;
                            
                            if (c->channel_layout == 0)
                            {
                                c->channel_layout = av_get_default_channel_layout( c->channels );
                            }
                            out_channel_layout = c->channel_layout;
                            
                            uint8_t downmix = stereo_software_decoder && out_channels > 2 ? 1 : 0;
#ifdef __sh__
                            // player2 won't play mono
                            if (out_channel_layout == AV_CH_LAYOUT_MONO)
                            {
                                downmix = 1;
                            }
#endif
                            if(downmix)
                            {
                                out_channel_layout = AV_CH_LAYOUT_STEREO_DOWNMIX;
                                out_channels = 2;
                            }
                            
                            av_opt_set_int(swr, "in_channel_layout",	c->channel_layout,	0);
                            av_opt_set_int(swr, "out_channel_layout",	out_channel_layout,	0);
                            av_opt_set_int(swr, "in_sample_rate",		c->sample_rate,		0);
                            av_opt_set_int(swr, "out_sample_rate",		out_sample_rate,	0);
                            av_opt_set_int(swr, "in_sample_fmt",		c->sample_fmt,		0);
                            av_opt_set_int(swr, "out_sample_fmt",		AV_SAMPLE_FMT_S16,	0);
        

                            e = swr_init(swr);
                            if (e < 0) 
                            {
                                ffmpeg_err("swr_init: %d (icl=%d ocl=%d isr=%d osr=%d isf=%d osf=%d\n",
                                    -e, (int32_t)c->channel_layout, (int32_t)out_channel_layout, c->sample_rate, out_sample_rate, c->sample_fmt, AV_SAMPLE_FMT_S16);
                                swr_free(&swr);
                                swr = NULL;
                            }
                        }
                        
                        uint8_t *output[8] = {NULL};
                        int32_t in_samples = decoded_frame->nb_samples;
                        int32_t out_samples = av_rescale_rnd(swr_get_delay(swr, c->sample_rate) + in_samples, out_sample_rate, c->sample_rate, AV_ROUND_UP);
                        e = av_samples_alloc(&output[0], NULL, out_channels, out_samples, AV_SAMPLE_FMT_S16, 1);
                        if (e < 0) 
                        {
                            ffmpeg_err("av_samples_alloc: %d\n", -e);
                            continue;
                        }
                        int64_t next_in_pts = av_rescale(av_frame_get_best_effort_timestamp(decoded_frame),
                                         ((AVStream*) audioTrack->stream)->time_base.num * (int64_t)out_sample_rate * c->sample_rate,
                                         ((AVStream*) audioTrack->stream)->time_base.den);
                        int64_t next_out_pts = av_rescale(swr_next_pts(swr, next_in_pts),
                                         ((AVStream*) audioTrack->stream)->time_base.den,
                                         ((AVStream*) audioTrack->stream)->time_base.num * (int64_t)out_sample_rate * c->sample_rate);
                        
                        currentAudioPts = audioTrack->pts = pts = calcPts(cAVIdx, audioTrack->stream, next_out_pts);
                        out_samples = swr_convert(swr, &output[0], out_samples, (const uint8_t **) &decoded_frame->data[0], in_samples);
                        
                        //////////////////////////////////////////////////////////////////////
                        // Update pcmExtradata according to decode parameters
                        pcmExtradata.channels              = av_get_channel_layout_nb_channels(out_channel_layout);
                        pcmExtradata.bits_per_coded_sample = 16;
                        pcmExtradata.sample_rate           = out_sample_rate;
                        // The data described by the sample format is always in native-endian order
#ifdef WORDS_BIGENDIAN
                        pcmExtradata.ffmpeg_codec_id       = AV_CODEC_ID_PCM_S16BE;
#else
                        pcmExtradata.ffmpeg_codec_id       = AV_CODEC_ID_PCM_S16LE;
#endif

                        //////////////////////////////////////////////////////////////////////

                        avOut.data       = output[0];
                        avOut.len        = out_samples * sizeof(int16_t) * out_channels;

                        avOut.pts        = pts;
                        avOut.extradata  = (unsigned char *) &pcmExtradata;
                        avOut.extralen   = sizeof(pcmExtradata);
                        avOut.frameRate  = 0;
                        avOut.timeScale  = 0;
                        avOut.width      = 0;
                        avOut.height     = 0;
                        avOut.type       = "audio";

                        if (!context->playback->BackWard && context->output->audio->Write(context, &avOut) < 0)
                        {
                            ffmpeg_err("writing data to audio device failed\n");
                        }
                        av_freep(&output[0]);
                    }
                }
                else if (audioTrack->have_aacheader == 1)
                {
                    ffmpeg_printf(200, "write audio aac\n");
                    ffmpeg_printf(200, ">>>>>>> %x %x %x %x %x %x\n", packet.data[0], packet.data[1], packet.data[2], packet.data[3], packet.data[4], packet.data[5], packet.data[6]);

                    avOut.data       = packet.data;
                    avOut.len        = packet.size;
                    avOut.pts        = pts;
                    avOut.extradata  = audioTrack->aacbuf;
                    avOut.extralen   = audioTrack->aacbuflen;
                    avOut.frameRate  = 0;
                    avOut.timeScale  = 0;
                    avOut.width      = 0;
                    avOut.height     = 0;
                    avOut.type       = "audio";

                    if (!context->playback->BackWard && context->output->audio->Write(context, &avOut) < 0)
                    {
                        ffmpeg_err("(aac) writing data to audio device failed\n");
                    }
                }
                else
                {
                    avOut.data       = packet.data;
                    avOut.len        = packet.size;
                    avOut.pts        = pts;
                    avOut.extradata  = pAudioExtradata;
                    avOut.extralen   = audioExtradataSize;
                    avOut.frameRate  = 0;
                    avOut.timeScale  = 0;
                    avOut.width      = 0;
                    avOut.height     = 0;
                    avOut.type       = "audio";

                    if (!context->playback->BackWard && context->output->audio->Write(context, &avOut) < 0)
                    {
                        ffmpeg_err("writing data to audio device failed\n");
                    }
                }
            }
            else if (subtitleTrack && (subtitleTrack->Id == pid))
            {
                int64_t duration = -1;
                int64_t pts = calcPts(cAVIdx, subtitleTrack->stream, packet.pts);
                AVStream *stream = subtitleTrack->stream;
                
                if (packet.duration != 0)
                {
                    // duration in milliseconds
                    duration = (int64_t)av_rescale(packet.duration, (int64_t)stream->time_base.num * 1000, stream->time_base.den); 
                }
                else if(packet.convergence_duration != 0 && packet.convergence_duration != AV_NOPTS_VALUE )
                {
                    // duration in milliseconds
                    duration = (int64_t)av_rescale(packet.convergence_duration, (int64_t)stream->time_base.num * 1000, stream->time_base.den); 
                }

                if (duration > 0)
                {
                    SubtitleOut_t subOut;
                    memset(&subOut, 0, sizeof(subOut));
                    subOut.trackId = pid;
                    subOut.data = (uint8_t *)packet.data;
                    subOut.pts = pts;
                    subOut.durationMS = duration;
                    if (context->output->subtitle->Write(context, &subOut) < 0)
                    {
                        ffmpeg_err("writing data to teletext fifo failed\n");
                    }
                }
            }
        }
        else 
        {
            if( 0 != ffmpegStatus )
            {
                static char errbuf[256];
                
                if( 0 == av_strerror(ffmpegStatus, errbuf, sizeof(errbuf)) )
                {
                    /* In this way we inform user about error within the core
                     */
                    printf("{\"log\":\"Frame read error: '%s'\"}\n", errbuf);
                }
                
                if( ffmpegStatus == AVERROR(EAGAIN) )
                {
                    continue;
                }
                ffmpegStatus = 0;
            }
            
            if(!is_finish_timeout())
            {
                isWaitingForFinish = 1;
                update_finish_timeout();
                releaseMutex(__FILE__, __FUNCTION__,__LINE__);
                usleep(100000);
                continue;
            }
            else
            {
                uint8_t bEndProcess = 1;
                if( 1 == context->playback->isLoopMode )
                {
                    int64_t tmpLength = 0;
                    if( 0 == container_ffmpeg_get_length(context, &tmpLength) && tmpLength > 0 && get_play_pts() > 0)
                    {
                        if (avContextTab[0]->iformat->flags & AVFMT_TS_DISCONT)
                        {
                            seek_target_bytes = 0;
                            do_seek_target_bytes = 1;
                        }
                        else
                        {
                            seek_target_seconds = 0;
                            do_seek_target_seconds = 1;
                        }
                        bEndProcess = 0;
                        context->output->Command(context, OUTPUT_CLEAR, NULL);
                        context->output->Command(context, OUTPUT_PLAY, NULL);
                        printf("{\"log\":\"Loop mode: jump to the start.\"}\n");
                    }
                }
                
                // av_read_frame failed
                ffmpeg_err("no data ->end of file reached ? \n");
#if LIBAVCODEC_VERSION_MAJOR >= 56
                av_packet_unref(&packet);
#else
                av_free_packet(&packet);
#endif
                releaseMutex(__FILE__, __FUNCTION__,__LINE__);
                if( bEndProcess )
                {
                    break; // while
                }
                else
                {
                    continue;
                }
            }
        }

#if LIBAVCODEC_VERSION_MAJOR >= 56
        av_packet_unref(&packet);
#else
        av_free_packet(&packet);
#endif
        releaseMutex(__FILE__, __FUNCTION__,__LINE__);
    } /* while */

    if (swr)
    {
        swr_free(&swr);
    }
    
    if (decoded_frame)
    {
#if (LIBAVCODEC_VERSION_MAJOR >= 55)
        av_frame_free(&decoded_frame);
#else
        avcodec_free_frame(&decoded_frame);
#endif
    }
    
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(56, 34, 100)
    mpeg4p2_context_reset(&mpeg4p2_context);
    if (NULL != mpeg4p2_bsf_context)
    {
        av_bitstream_filter_close(mpeg4p2_bsf_context);
    }
#endif

    hasPlayThreadStarted = 0;
    context->playback->isPlaying = 0;
    PlaybackDieNow(1);
    ffmpeg_printf(10, "terminating\n");
}

/* **************************** */
/* Container part for ffmpeg    */
/* **************************** */

static int32_t terminating = 0;
static int32_t interrupt_cb(void *ctx)
{
    PlaybackHandler_t *p = (PlaybackHandler_t *)ctx;
    return p->abortRequested || PlaybackDieNow(0);
}

#ifdef SAM_CUSTOM_IO
int SAM_ReadFunc(void *ptr, uint8_t *buffer, int lSize)
{
    size_t ret = fread ( (void *) buffer, (size_t) 1, (size_t) lSize, (FILE *)ptr );
    return (int)ret;
}

// whence: SEEK_SET, SEEK_CUR, SEEK_END (like fseek) and AVSEEK_SIZE
int64_t SAM_SeekFunc(void* ptr, int64_t pos, int whence)
{   
    if( AVSEEK_SIZE == whence )
    {
        return -1;
    }
    int ret = fseeko((FILE *)ptr, (off_t)pos, whence);
    if(0 == ret)
    {
        return (off_t)ftello((FILE *)ptr);
    }
    return ret;
}

AVIOContext* container_ffmpeg_get_avio_context(char *filename, size_t avio_ctx_buffer_size)
{
        if(strstr(filename, "file://") == filename)
        {
            filename += 7;
        }
        
        FILE *pFile = fopen(filename, "rb");
        if(NULL == pFile)
        {
            return NULL;
        }
        
        AVIOContext *avio_ctx = NULL;
        uint8_t *avio_ctx_buffer = NULL;
        
        avio_ctx_buffer = av_malloc(avio_ctx_buffer_size);
        if (!avio_ctx_buffer) 
        {
            return NULL;
        }
        avio_ctx = avio_alloc_context(avio_ctx_buffer, avio_ctx_buffer_size, 0, pFile, &SAM_ReadFunc, NULL, &SAM_SeekFunc);
        if (!avio_ctx) 
        {
            return NULL;
        }
        return avio_ctx;
}
#endif

int32_t container_ffmpeg_init_av_context(Context_t *context, char *filename, int32_t AVIdx)
{
    int32_t err = 0;
    avContextTab[AVIdx] = avformat_alloc_context();
    avContextTab[AVIdx]->interrupt_callback.callback = interrupt_cb;
    avContextTab[AVIdx]->interrupt_callback.opaque = context->playback;

#ifdef SAM_CUSTOM_IO
    if(strstr(filename, "://") == 0 || 
       strstr(filename, "file://") == filename)
    {
        AVIOContext *avio_ctx = container_ffmpeg_get_avio_context(filename, 4096);
        if(avio_ctx)
        {
            avContextTab[AVIdx]->pb = avio_ctx;
            use_custom_io[AVIdx] = 1;
        }
        else
        {
            return cERR_CONTAINER_FFMPEG_OPEN;
        }
    }
#endif

    if(strstr(filename, "http://") == filename || 
       strstr(filename, "https://") == filename)
    {
        av_dict_set(&avio_opts, "timeout", "20000000", 0); //20sec
        av_dict_set(&avio_opts, "reconnect", "1", 0);
        if ((err = avformat_open_input(&avContextTab[AVIdx], filename, NULL, &avio_opts)) != 0)
        {
            char error[512];

            ffmpeg_err("avformat_open_input failed %d (%s)\n", err, filename);
            av_strerror(err, error, 512);
            ffmpeg_err("Cause: %s\n", error);

            if(avio_opts != NULL)
            {
                av_dict_free(&avio_opts);
            }
            releaseMutex(__FILE__, __FUNCTION__,__LINE__);
            return cERR_CONTAINER_FFMPEG_OPEN;
        }
    }
    else if ((err = avformat_open_input(&avContextTab[AVIdx], filename, NULL, 0)) != 0)
    {
        char error[512] = "";

        ffmpeg_err("avformat_open_input failed %d (%s)\n", err, filename);
        av_strerror(err, error, 512);
        ffmpeg_err("Cause: %s\n", error);

        if(avio_opts != NULL)
        {
            av_dict_free(&avio_opts);
        }
        releaseMutex(__FILE__, __FUNCTION__,__LINE__);
        return cERR_CONTAINER_FFMPEG_OPEN;
    }

    avContextTab[AVIdx]->iformat->flags |= AVFMT_SEEK_TO_PTS;
    avContextTab[AVIdx]->flags = AVFMT_FLAG_GENPTS;

    if (context->playback->noprobe)
    {
#if (LIBAVFORMAT_VERSION_MAJOR > 55) && (LIBAVFORMAT_VERSION_MAJOR < 57)
        avContextTab[AVIdx]->max_analyze_duration2 = 1;
#else
        avContextTab[AVIdx]->max_analyze_duration = 1;
#endif
    }

    ffmpeg_printf(20, "find_streaminfo\n");

    if (avformat_find_stream_info(avContextTab[AVIdx], NULL) < 0) 
    {
        ffmpeg_err("Error avformat_find_stream_info\n");
#ifdef this_is_ok
        /* crow reports that sometimes this returns an error
         * but the file is played back well. so remove this
         * until other works are done and we can prove this.
         */
        avformat_close_input(&avContextTab[AVIdx]);
        releaseMutex(__FILE__, __FUNCTION__,__LINE__);
        return cERR_CONTAINER_FFMPEG_STREAM;
#endif
    }

//for buffered io
    if(avContextTab[AVIdx] != NULL && avContextTab[AVIdx]->pb != NULL)
    {
        ffmpeg_real_read_org = avContextTab[AVIdx]->pb->read_packet;
        
        if(0 ==AVIdx && strstr(filename, "://") != 0 && 
           strstr(filename, "file://") != filename)
        {
            if(ffmpeg_buf_size > 0 && ffmpeg_buf_size > FILLBUFDIFF + FILLBUFPAKET)
            {
                if(avContextTab[AVIdx] != NULL && avContextTab[AVIdx]->pb != NULL)
                {
                    ffmpeg_buf = av_malloc(ffmpeg_buf_size);

                    if(ffmpeg_buf != NULL)
                    {
                        ffmpeg_printf(10, "buffer size=%d\n", ffmpeg_buf_size);
                        
                        ffmpeg_read_org = avContextTab[AVIdx]->pb->read_packet;
                        avContextTab[AVIdx]->pb->read_packet = ffmpeg_read;
                        ffmpeg_seek_org = avContextTab[AVIdx]->pb->seek;
                        avContextTab[AVIdx]->pb->seek = ffmpeg_seek;
                        ffmpeg_buf_read = ffmpeg_buf;
                        ffmpeg_buf_write = ffmpeg_buf;

                        //fill buffer
                        ffmpeg_filler(context, -1, NULL, 0);
                        ffmpeg_start_fillerTHREAD(context);
                    }
                }
            }
        }
        else if (progressive_download)
        {
            avContextTab[AVIdx]->pb->read_packet = ffmpeg_read_wrapper;
        }
    }
//for buffered io (end)
    
    return 0;
}

int32_t container_ffmpeg_init(Context_t *context, PlayFiles_t *playFilesNames)
{
    int32_t err = 0;

    ffmpeg_printf(10, ">\n");

    if (playFilesNames == NULL) 
    {
        ffmpeg_err("playFilesNames NULL\n");
        return cERR_CONTAINER_FFMPEG_NULL;
    }
    
    if (playFilesNames->szFirstFile == NULL) 
    {
        ffmpeg_err("playFilesNames->szFirstFile NULL\n");
        return cERR_CONTAINER_FFMPEG_NULL;
    }

    if (context == NULL) 
    {
        ffmpeg_err("context NULL\n");
        return cERR_CONTAINER_FFMPEG_NULL;
    }

    ffmpeg_printf(10, "filename %s\n", playFilesNames->szFirstFile);
    if(playFilesNames->szSecondFile)
    {
        ffmpeg_printf(10, "second filename %s\n", playFilesNames->szSecondFile);
    }

    if (isContainerRunning) 
    {
        ffmpeg_err("ups already running?\n");
        releaseMutex(__FILE__, __FUNCTION__,__LINE__);
        return cERR_CONTAINER_FFMPEG_RUNNING;
    }

    /* initialize ffmpeg */
    avcodec_register_all();
    av_register_all();
    avformat_network_init();
    
    // SULGE DEBUG ENABLED
    // make ffmpeg silen
    //av_log_set_level( AV_LOG_DEBUG );
    av_log_set_callback(ffmpeg_silen_callback);
 
    context->playback->abortRequested = 0;
    int32_t res = container_ffmpeg_init_av_context(context, playFilesNames->szFirstFile, 0);
    if(0 != res)
    {
        return res;
    }

    if(playFilesNames->szSecondFile)
    {
        res = container_ffmpeg_init_av_context(context, playFilesNames->szSecondFile, 1);
    }
    
    if(0 != res)
    {
        return res;
    }
    
    terminating = 0;
    latestPts = 0;
    isContainerRunning = 1;
    res = container_ffmpeg_update_tracks(context, playFilesNames->szFirstFile, 1);
    return res;
}

int32_t container_ffmpeg_update_tracks(Context_t *context, char *filename, int32_t initial)
{
    Track_t *audioTrack = NULL;
    Track_t *subtitleTrack = NULL;
    
    if (terminating)
    {
        return cERR_CONTAINER_FFMPEG_NO_ERROR;
    }
    
    if (initial && context->manager->subtitle)
    {
        context->manager->subtitle->Command(context, MANAGER_GET_TRACK, &subtitleTrack);
    }

    if (context->manager->audio)
    {
        context->manager->audio->Command(context, MANAGER_GET_TRACK, &audioTrack);
    }

    if (context->manager->video)
    {
        context->manager->video->Command(context, MANAGER_INIT_UPDATE, NULL);
    }
    
    if (context->manager->audio)
    {
        context->manager->audio->Command(context, MANAGER_INIT_UPDATE, NULL);
    }
    
#if 0
    if (context->manager->subtitle)
    {
        context->manager->subtitle->Command(context, MANAGER_INIT_UPDATE, NULL);
    }
#endif


    ffmpeg_printf(20, "dump format\n");
    av_dump_format(avContextTab[0], 0, filename, 0);
    
    
    uint32_t cAVIdx = 0;
    for(cAVIdx=0; cAVIdx<IPTV_AV_CONTEXT_MAX_NUM; cAVIdx+=1)
    {
        if(NULL == avContextTab[cAVIdx])
        {
            break;
        }
        AVFormatContext *avContext = avContextTab[cAVIdx];
        ffmpeg_printf(1, "cAVIdx[%d]: number of streams: %d\n", cAVIdx, avContext->nb_streams);
        
        int32_t n = 0;
        for (n = 0; n < avContext->nb_streams; n++) 
        {
            Track_t track;
            AVStream *stream = avContext->streams[n];
            int32_t version = 0;

            char *encoding = Codec2Encoding(stream->codec, &version);
            
            if(encoding != NULL && !strncmp(encoding, "A_IPCM", 6) && insert_pcm_as_lpcm)
            {
                encoding = "A_LPCM";
            }

            if (encoding != NULL)
            {
               ffmpeg_printf(1, "%d. encoding = %s - version %d\n", n, encoding, version);
            }

            if (!stream->id)
            {
                stream->id = n;
            }

            /* some values in track are unset and therefor copyTrack segfaults.
             * so set it by default to NULL!
             */
            memset(&track, 0, sizeof(track));
            track.AVIdx = cAVIdx;

            switch (stream->codec->codec_type) 
            {
            case AVMEDIA_TYPE_VIDEO:
                ffmpeg_printf(10, "CODEC_TYPE_VIDEO %d\n",stream->codec->codec_type);

                if (encoding != NULL) 
                {
                    track.type           = eTypeES;
                    track.version        = version;

                    track.width          = stream->codec->width;
                    track.height         = stream->codec->height;

                    track.extraData      = stream->codec->extradata;
                    track.extraSize      = stream->codec->extradata_size;

                    track.aacbuf         = 0;
                    track.have_aacheader = -1;
                    
                    track.frame_rate = (uint32_t)(1000 * (int64_t)(stream->r_frame_rate.num) / (int64_t)(stream->r_frame_rate.den));

                    /* fixme: revise this */
                    if (track.frame_rate < 23970)
                    {
                        track.TimeScale = 1001;
                    }
                    else
                    {
                        track.TimeScale = 1000;
                    }

                    ffmpeg_printf(10, "bit_rate = %d\n",stream->codec->bit_rate);
                    ffmpeg_printf(10, "flags = %d\n",stream->codec->flags);
                    ffmpeg_printf(10, "frame_bits = %d\n",stream->codec->frame_bits);
                    ffmpeg_printf(10, "time_base.den %d\n",stream->time_base.den);
                    ffmpeg_printf(10, "time_base.num %d\n",stream->time_base.num);
                    ffmpeg_printf(10, "frame_rate %d\n",stream->r_frame_rate.num);
                    ffmpeg_printf(10, "TimeScale %d\n",stream->r_frame_rate.den);

                    ffmpeg_printf(10, "frame_rate %lld\n", track.frame_rate);
                    ffmpeg_printf(10, "TimeScale %d\n", track.TimeScale);

                    track.Name      = "und";
                    track.Encoding  = encoding;
                    track.stream    = stream;
                    track.Id        = ((AVStream *) (track.stream))->id;

                    track.duration = (int64_t)av_rescale(stream->duration, (int64_t)stream->time_base.num * 1000, stream->time_base.den); 
                    if(stream->duration == AV_NOPTS_VALUE) 
                    {
                        ffmpeg_printf(10, "Stream has no duration so we take the duration from context\n");
                        track.duration = (int64_t) avContext->duration / 1000;
                    }
                    
                    if (context->manager->video)
                    {
                        ffmpeg_printf(1, "cAVIdx[%d]: MANAGER_ADD track VIDEO\n");
                        if( context->manager->video->Command(context, MANAGER_ADD, &track) < 0) 
                        {
                            /* konfetti: fixme: is this a reason to return with error? */
                            ffmpeg_err("failed to add track %d\n", n);
                        }
                    }
                }
                else 
                {
                    ffmpeg_err("codec type video but codec unknown %d\n", stream->codec->codec_id);
                }
                break;
            case AVMEDIA_TYPE_AUDIO:
                ffmpeg_printf(10, "CODEC_TYPE_AUDIO %d\n",stream->codec->codec_type);

                if (encoding != NULL)
                {
                    AVDictionaryEntry *lang;
                    track.type           = eTypeES;

                    lang = av_dict_get(stream->metadata, "language", NULL, 0);

                    track.Name = lang ? lang->value : "und";

                    ffmpeg_printf(10, "Language %s\n", track.Name);

                    track.Encoding       = encoding;
                    track.stream         = stream;
                    track.Id             = ((AVStream *) (track.stream))->id;
                    track.aacbuf         = 0;
                    track.have_aacheader = -1;

                    track.duration       = (int64_t)av_rescale(stream->duration, (int64_t)stream->time_base.num * 1000, stream->time_base.den);
                    if(stream->duration == AV_NOPTS_VALUE) 
                    {
                        ffmpeg_printf(10, "Stream has no duration so we take the duration from context\n");
                        track.duration = (int64_t) avContext->duration / 1000;
                    }
                    
                    if(!strncmp(encoding, "A_IPCM", 6) || !strncmp(encoding, "A_LPCM", 6))
                    {
                        track.inject_as_pcm = 1;
                        ffmpeg_printf(10, " Handle inject_as_pcm = %d\n", track.inject_as_pcm);

                        AVCodec *codec = avcodec_find_decoder(stream->codec->codec_id);

                        //( (AVStream*) audioTrack->stream)->codec->flags |= CODEC_FLAG_TRUNCATED;
                        int errorCode = avcodec_open2(stream->codec, codec, NULL);
                        if(codec != NULL && !errorCode)
                        {
                           ffmpeg_printf(10, "AVCODEC__INIT__SUCCESS\n");
                        }
                        else
                        {
                           ffmpeg_printf(10, "AVCODEC__INIT__FAILED error[%d]\n", errorCode);
                        }
                    }
                    else if(!strncmp(encoding, "A_PCM", 5))
                    {
                        track.inject_raw_pcm = 1;
                    }
                    else if(stream->codec->codec_id == AV_CODEC_ID_AAC)
                    {
                        if( 0 == strncmp(avContext->iformat->name, "mpegts", 6) || 
                            0 == strncmp(avContext->iformat->name, "hls,", 4) )
                        {
                            const char marker[] = "ADTS";
                            track.aacbuflen = sizeof(marker)/sizeof(char);
                            track.aacbuf = malloc(track.aacbuflen);
                            //assert(track.aacbuf);
                            memcpy(track.aacbuf, marker, track.aacbuflen);
                            
                            ffmpeg_printf(10, "AV_CODEC_ID_AAC no extradata ACC header should be available in each frame\n");
                            track.have_aacheader = 1;
                        }
                        else
                        {
                            ffmpeg_printf(10, "Create AAC ExtraData\n");
                            ffmpeg_printf(10, "stream->codec->extradata_size %d\n", stream->codec->extradata_size);
                            Hexdump(stream->codec->extradata, stream->codec->extradata_size);

                            /*  extradata:
                                13 10 56 e5 9d 48 00 (anderen cops)
                                object_type: 00010 2 = LC
                                sample_rate: 011 0 6 = 24000
                                chan_config: 0010 2 = Stereo
                                000 0
                                1010110 111 = 0x2b7
                                00101 = SBR
                                1
                                0011 = 48000
                                101 01001000 = 0x548
                                ps = 0
                                0000000
                            */

                            int32_t object_type = 2; // LC
                            int32_t sample_index = aac_get_sample_rate_index(stream->codec->sample_rate);
                            int32_t chan_config = stream->codec->channels;
                            if(stream->codec->extradata_size >= 2) 
                            {
                                uint8_t *h = stream->codec->extradata;
                                object_type = h[0] >> 3;
                                sample_index = ((h[0] & 0x7) << 1) |((h[1] & 0x80) >> 7);
                                chan_config = (h[1] & 0x78) >> 3;
                            }

                            ffmpeg_printf(10,"aac object_type %d\n", object_type);
                            ffmpeg_printf(10,"aac sample_index %d\n", sample_index);
                            ffmpeg_printf(10,"aac chan_config %d\n", chan_config);
                            ffmpeg_printf(10,"aac chan_config %d\n", chan_config);

                            object_type -= 1; // Cause of ADTS

                            track.aacbuflen = AAC_HEADER_LENGTH;
                            track.aacbuf = malloc(8);
                            track.aacbuf[0] = 0xFF;
                            track.aacbuf[1] = 0xF1;
                            //track.aacbuf[1] |=0x8;
                            track.aacbuf[2] = ((object_type & 0x03) << 6)  | (sample_index << 2) | ((chan_config >> 2) & 0x01);
                            track.aacbuf[3] = (chan_config & 0x03) << 6;
                            //track.aacbuf[3]|= 0x10;
                            track.aacbuf[4] = 0x00;
                            track.aacbuf[5] = 0x1F;
                            track.aacbuf[6] = 0xFC;

                            printf("AAC_HEADER -> ");
                            Hexdump(track.aacbuf,7);
                            track.have_aacheader = 1;
                        }
                        /*
                        else
                        {
                            ffmpeg_err("AV_CODEC_ID_AAC extradata not available\n");
                        }
                        */

                    }
                    else if(stream->codec->codec_id == AV_CODEC_ID_WMAV1
                        || stream->codec->codec_id == AV_CODEC_ID_WMAV2
                        || stream->codec->codec_id == AV_CODEC_ID_WMAPRO
                        || stream->codec->codec_id == AV_CODEC_ID_WMALOSSLESS) //if (stream->codec->extradata_size > 0)
                    {
                        ffmpeg_printf(10,"Create WMA ExtraData\n");
                        uint16_t channels = stream->codec->channels;
                        uint32_t rate = stream->codec->sample_rate;
                        uint32_t bitrate = stream->codec->bit_rate;
                        uint16_t block_align = stream->codec->block_align;
                        uint16_t depth = stream->codec->bits_per_coded_sample;
                        uint32_t codec_data_size = stream->codec->extradata_size;
                        uint8_t *codec_data_pointer = stream->codec->extradata;
                        
                        // type_specific_data
                        #define WMA_VERSION_1           0x160
                        #define WMA_VERSION_2_9         0x161
                        #define WMA_VERSION_9_PRO       0x162
                        #define WMA_LOSSLESS            0x163
                        uint16_t codec_id = 0;
                        switch(stream->codec->codec_id)
                        {
                            //TODO: What code for lossless ?
                            case AV_CODEC_ID_WMALOSSLESS:
                                codec_id = WMA_LOSSLESS;
                                break;
                            case AV_CODEC_ID_WMAPRO:
                                codec_id = WMA_VERSION_9_PRO;
                                break;
                            case AV_CODEC_ID_WMAV2:
                                codec_id = WMA_VERSION_2_9 ;
                                break;
                            case AV_CODEC_ID_WMAV1:
                            default:
                                codec_id = WMA_VERSION_1;
                                break;
                        }
#ifdef __sh__
                        track.aacbuflen = 104 + stream->codec->extradata_size;
                        track.aacbuf = malloc(track.aacbuflen);
                        memset (track.aacbuf, 0, track.aacbuflen);
                        
                        uint8_t ASF_Stream_Properties_Object[16] =
                        {0x91,0x07,0xDC,0xB7,0xB7,0xA9,0xCF,0x11,0x8E,0xE6,0x00,0xC0,0x0C,0x20,0x53,0x65};
                        
                        memcpy(track.aacbuf + 0, ASF_Stream_Properties_Object, 16); // ASF_Stream_Properties_Object
                        memcpy(track.aacbuf + 16, &track.aacbuflen, 4); //FrameDateLength

                        uint32_t sizehi = 0;
                        memcpy(track.aacbuf + 20, &sizehi, 4); // sizehi (not used)

                        uint8_t ASF_Audio_Media[16] =
                        {0x40,0x9E,0x69,0xF8,0x4D,0x5B,0xCF,0x11,0xA8,0xFD,0x00,0x80,0x5F,0x5C,0x44,0x2B};
                        
                        memcpy(track.aacbuf + 24, ASF_Audio_Media, 16); //ASF_Audio_Media

                        uint8_t ASF_Audio_Spread[16] =
                        {0x50,0xCD,0xC3,0xBF,0x8F,0x61,0xCF,0x11,0x8B,0xB2,0x00,0xAA,0x00,0xB4,0xE2,0x20};
                        
                        memcpy(track.aacbuf + 40, ASF_Audio_Spread, 16); //ASF_Audio_Spread

                        memset(track.aacbuf + 56, 0, 4); // time_offset (not used)
                        memset(track.aacbuf + 60, 0, 4); // time_offset_hi (not used)

                        uint8_t type_specific_data_length = 18 + stream->codec->extradata_size;
                        memcpy(track.aacbuf + 64, &type_specific_data_length, 4); //type_specific_data_length

                        uint8_t error_correction_data_length = 8;
                        memcpy(track.aacbuf + 68, &error_correction_data_length, 4); //error_correction_data_length

                        uint16_t flags = 1; // stream_number
                        memcpy(track.aacbuf + 72, &flags, 2); //flags

                        uint32_t reserved = 0;
                        memcpy(track.aacbuf + 74, &reserved, 4); // reserved

                        memcpy(track.aacbuf + 78, &codec_id, 2); //codec_id

                        uint16_t number_of_channels = stream->codec->channels;
                        memcpy(track.aacbuf + 80, &number_of_channels, 2); //number_of_channels

                        uint32_t samples_per_second = stream->codec->sample_rate;
                        ffmpeg_printf(1, "samples_per_second = %d\n", samples_per_second);
                        memcpy(track.aacbuf + 82, &samples_per_second, 4); //samples_per_second

                        uint32_t average_number_of_bytes_per_second = stream->codec->bit_rate / 8;
                        ffmpeg_printf(1, "average_number_of_bytes_per_second = %d\n", average_number_of_bytes_per_second);
                        memcpy(track.aacbuf + 86, &average_number_of_bytes_per_second, 4); //average_number_of_bytes_per_second

                        uint16_t block_alignment = stream->codec->block_align;
                        ffmpeg_printf(1, "block_alignment = %d\n", block_alignment);
                        memcpy(track.aacbuf + 90, &block_alignment, 2); //block_alignment

                        uint16_t bits_per_sample =
                        stream->codec->sample_fmt>=0?(stream->codec->sample_fmt+1)*8:8;
                        ffmpeg_printf(1, "bits_per_sample = %d (%d)\n", bits_per_sample, stream->codec->sample_fmt);
                        memcpy(track.aacbuf + 92, &bits_per_sample, 2); //bits_per_sample

                        memcpy(track.aacbuf + 94, &stream->codec->extradata_size, 2); //bits_per_sample

                        memcpy(track.aacbuf + 96, stream->codec->extradata, stream->codec->extradata_size);
#else
                        track.aacbuflen = 18 + stream->codec->extradata_size;
                        track.aacbuf = malloc(track.aacbuflen);
                        memset (track.aacbuf, 0, track.aacbuflen);
                        
                        uint8_t *data = track.aacbuf;
                        /* codec tag */
                        *(data++) = codec_id & 0xff;
                        *(data++) = (codec_id >> 8) & 0xff;
                        /* channels */
                        *(data++) = channels & 0xff;
                        *(data++) = (channels >> 8) & 0xff;
                        /* sample rate */
                        *(data++) = rate & 0xff;
                        *(data++) = (rate >> 8) & 0xff;
                        *(data++) = (rate >> 16) & 0xff;
                        *(data++) = (rate >> 24) & 0xff;
                        /* byte rate */
                        bitrate /= 8;
                        *(data++) = bitrate & 0xff;
                        *(data++) = (bitrate >> 8) & 0xff;
                        *(data++) = (bitrate >> 16) & 0xff;
                        *(data++) = (bitrate >> 24) & 0xff;
                        /* block align */
                        *(data++) = block_align & 0xff;
                        *(data++) = (block_align >> 8) & 0xff;
                        /* word size */
                        *(data++) = depth & 0xff;
                        *(data++) = (depth >> 8) & 0xff;
                        /* codec data size */
                        *(data++) = codec_data_size & 0xff;
                        *(data++) = (codec_data_size >> 8) & 0xff;
                        memcpy(data, codec_data_pointer, codec_data_size);
#endif
                        ffmpeg_printf(1, "aacbuf:\n");
                        Hexdump(track.aacbuf, track.aacbuflen);

                        //ffmpeg_printf(1, "priv_data:\n");
                        //Hexdump(stream->codec->priv_data, track.aacbuflen);

                        track.have_aacheader = 1;
                    }
                    
                    if (context->manager->audio)
                    {
                        ffmpeg_printf(1, "cAVIdx[%d]: MANAGER_ADD track AUDIO\n");
                        if (context->manager->audio->Command(context, MANAGER_ADD, &track) < 0) 
                        {
                            /* konfetti: fixme: is this a reason to return with error? */
                            ffmpeg_err("failed to add track %d\n", n);
                        }
                    }
                }
                else //encoding != NULL
                {
                    ffmpeg_err("codec type audio but codec unknown %d\n", stream->codec->codec_id);
                }
                break;
            case AVMEDIA_TYPE_SUBTITLE:
            {
                if (stream->codec->codec_id != AV_CODEC_ID_SSA &&
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(55, 3, 100)
                    stream->codec->codec_id != AV_CODEC_ID_ASS &&
#endif
                    stream->codec->codec_id != AV_CODEC_ID_SUBRIP && 
                    stream->codec->codec_id != AV_CODEC_ID_TEXT &&
                    stream->codec->codec_id != AV_CODEC_ID_SRT)
                {
                    ffmpeg_printf(10, "subtitle with not supported codec codec_id[%u]\n", (uint32_t)stream->codec->codec_id);
                    printf("SULGE subtitle with not supported codec codec_id[%u]\n", (uint32_t)stream->codec->codec_id);
                }
                else if (initial && context->manager->subtitle)
                {
                    AVDictionaryEntry *lang = NULL;
                    memset(&track, 0, sizeof(track));

                    ffmpeg_printf(10, "CODEC_TYPE_SUBTITLE %d\n", stream->codec->codec_type);
                    
                    lang = av_dict_get(stream->metadata, "language", NULL, 0);
                    track.Name = lang ? lang->value : "und";
                    ffmpeg_printf(10, "Language %s\n", track.Name);

                    track.Encoding       = encoding;
                    track.stream         = stream;
                    track.Id             = ((AVStream *) (track.stream))->id;
                    track.duration       = (int64_t)av_rescale(stream->duration, (int64_t)stream->time_base.num * 1000, stream->time_base.den);
                    
                    if(stream->duration == AV_NOPTS_VALUE) 
                    {
                        ffmpeg_printf(10, "Stream has no duration so we take the duration from context\n");
                        track.duration = (int64_t) avContext->duration / 1000;
                    }

                    track.extraData      = stream->codec->extradata;
                    track.extraSize      = stream->codec->extradata_size;

                    ffmpeg_printf(1, "subtitle codec %d\n", stream->codec->codec_id);
                    ffmpeg_printf(1, "subtitle width %d\n", stream->codec->width);
                    ffmpeg_printf(1, "subtitle height %d\n", stream->codec->height);
                    ffmpeg_printf(1, "subtitle stream %p\n", stream);

                    ffmpeg_printf(10, "FOUND SUBTITLE %s\n", track.Name);
                    
                    if (context->manager->subtitle->Command(context, MANAGER_ADD, &track) < 0)
                    {
                        ffmpeg_err("failed to add subtitle track %d\n", n);
                    }
                }
                break;
            }
            case AVMEDIA_TYPE_UNKNOWN:
            case AVMEDIA_TYPE_DATA:
            case AVMEDIA_TYPE_ATTACHMENT:
            case AVMEDIA_TYPE_NB:
            default:
                ffmpeg_err("not handled or unknown codec_type %d\n", stream->codec->codec_type);
             break;
            }
        } /* for */
    
    }

    return cERR_CONTAINER_FFMPEG_NO_ERROR;
}

static int32_t container_ffmpeg_play(Context_t *context)
{
    int32_t error = 0;
    int32_t ret = 0;
    pthread_attr_t attr;

    ffmpeg_printf(10, "\n");

    if ( context && context->playback && context->playback->isPlaying ) 
    {
        ffmpeg_printf(10, "is Playing\n");
    }
    else 
    {
        ffmpeg_printf(10, "is NOT Playing\n");
    }

    if (hasPlayThreadStarted == 0) 
    {
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

        if((error = pthread_create(&PlayThread, &attr, (void *)&FFMPEGThread, context)) != 0) 
        {
            ffmpeg_printf(10, "Error creating thread, error:%d:%s\n", error,strerror(error));

            hasPlayThreadStarted = 0;
            ret = cERR_CONTAINER_FFMPEG_ERR;
        }
        else 
        {
            ffmpeg_printf(10, "Created thread\n");

            hasPlayThreadStarted = 1;
        }
    }
    else 
    {
        ffmpeg_printf(10, "A thread already exists!\n");
        ret = cERR_CONTAINER_FFMPEG_ERR;
    }

    ffmpeg_printf(10, "exiting with value %d\n", ret);
    return ret;
}

static int32_t container_ffmpeg_stop(Context_t *context) 
{
    int32_t ret = cERR_CONTAINER_FFMPEG_NO_ERROR;
    int32_t wait_time = 10; // we give 1s to close otherwise we will force close

    ffmpeg_printf(10, "\n");

    if (!isContainerRunning)
    {
        ffmpeg_err("Container not running\n");
        return cERR_CONTAINER_FFMPEG_ERR;
    }
    
    if (context->playback)
    {
        context->playback->isPlaying = 0;
    }

    while ( (hasPlayThreadStarted != 0) && (--wait_time) > 0 ) 
    {
        ffmpeg_printf(10, "Waiting for ffmpeg thread to terminate itself, will try another %d times\n", wait_time);
        usleep(100000);
    }

    if (wait_time == 0) 
    {
        /* force close */
        ffmpeg_err( "Timeout waiting for thread!\n");
        ret = cERR_CONTAINER_FFMPEG_ERR;
        /* to speed up close - we are in separate process for the moment this process will 
         * be closed and whole resources will be free by the system  
         */ 
        return ret;
    }

    hasPlayThreadStarted = 0;
    terminating = 1;

    getMutex(__FILE__, __FUNCTION__,__LINE__);

    uint32_t i = 0;
    for(i=0; i<IPTV_AV_CONTEXT_MAX_NUM; i+=1)
    {
        if(NULL != avContextTab[i])
        {
            if(0 != use_custom_io[i])
            {
                /*
                 * Free custom IO independently to avoid segfault/bus error 
                 * avformat_close_input do not expect custom io, so it try
                 * to release incorrectly
                 */
                av_freep(&(avContextTab[i]->pb->buffer));
                av_freep(&(avContextTab[i]->pb));
                use_custom_io[i] = 0;
            }
            avformat_close_input(&avContextTab[i]);
            avContextTab[i] = NULL;
        }
        else
        {
            break;
        }
    }

    if(avio_opts != NULL)
    {
        av_dict_free(&avio_opts);
    }

    isContainerRunning = 0;
    avformat_network_deinit();
    ffmpeg_buf_free();

    releaseMutex(__FILE__, __FUNCTION__,__LINE__);

    ffmpeg_printf(10, "ret %d\n", ret);
    return ret;
}

static int32_t container_ffmpeg_seek_bytes(off_t pos) 
{
    int32_t flag = AVSEEK_FLAG_BYTE;
    off_t current_pos = avio_tell(avContextTab[0]->pb);

    ffmpeg_printf(20, "seeking to position %lld (bytes)\n", pos);

    if (current_pos > pos)
    {
        flag |= AVSEEK_FLAG_BACKWARD;
    }

    if (avformat_seek_file(avContextTab[0], -1, INT64_MIN, pos, INT64_MAX, flag) < 0)
    {
        ffmpeg_err( "Error seeking\n");
        return cERR_CONTAINER_FFMPEG_ERR;
    }

    ffmpeg_printf(30, "current_pos after seek %lld\n", avio_tell(avContextTab[0]->pb));

    return cERR_CONTAINER_FFMPEG_NO_ERROR;
}

/* seeking relative to a given byteposition N seconds ->for reverse playback needed */
static int32_t container_ffmpeg_seek_rel(Context_t *context, off_t pos, int64_t pts, int64_t sec) 
{
    Track_t *videoTrack = NULL;
    Track_t *audioTrack = NULL;
    Track_t *current = NULL;
    seek_target_flag = 0;

    ffmpeg_printf(10, "seeking %f sec relativ to %lld\n", sec, pos);

    context->manager->video->Command(context, MANAGER_GET_TRACK, &videoTrack);
    context->manager->audio->Command(context, MANAGER_GET_TRACK, &audioTrack);

    if (videoTrack != NULL)
    {
        current = videoTrack;
    }
    else if (audioTrack != NULL)
    {
        current = audioTrack;
    }
    
    if (current == NULL) 
    {
        ffmpeg_err( "no track avaibale to seek\n");
        return cERR_CONTAINER_FFMPEG_ERR;
    }

    if (pos == -1)
    {
        pos = avio_tell(avContextTab[0]->pb);
    }

    if (pts == -1)
    {
        pts = current->pts;
    }
    
    if (sec < 0)
    {
        seek_target_flag |= AVSEEK_FLAG_BACKWARD;
    }

    ffmpeg_printf(10, "iformat->flags %d\n", avContextTab[0]->iformat->flags);

    if (avContextTab[0]->iformat->flags & AVFMT_TS_DISCONT)
    {
        if (avContextTab[0]->bit_rate)
        {
            sec *= avContextTab[0]->bit_rate / 8;
            ffmpeg_printf(10, "bit_rate %d\n", avContextTab[0]->bit_rate);
        }
        else
        {
            sec *= 180000;
        }

        pos += sec;

        if (pos < 0)
        {
           ffmpeg_err("end of file reached\n");
           releaseMutex(__FILE__, __FUNCTION__,__LINE__);
           return cERR_CONTAINER_FFMPEG_END_OF_FILE;
        }

        ffmpeg_printf(10, "1. seeking to position %lld bytes ->sec %f\n", pos, sec);

        seek_target_bytes = pos;
        do_seek_target_bytes = 1;

        return pos;
    }
    else
    {
        sec += pts / 90000;

        if (sec < 0)
        {
            sec = 0;
        }

        ffmpeg_printf(10, "2. seeking to position %f sec ->time base %f %d\n", sec, av_q2d(((AVStream*) current->stream)->time_base), AV_TIME_BASE);

        seek_target_seconds = sec * AV_TIME_BASE;
        do_seek_target_seconds = 1;
    }

    releaseMutex(__FILE__, __FUNCTION__,__LINE__);
    return cERR_CONTAINER_FFMPEG_NO_ERROR;
}

static int32_t container_ffmpeg_seek(Context_t *context, int64_t sec, uint8_t absolute)
{
    Track_t *videoTrack = NULL;
    Track_t *audioTrack = NULL;
    Track_t *current = NULL;
    seek_target_flag = 0;

    if (!absolute) 
    {
        ffmpeg_printf(10, "seeking %f sec\n", sec);
        if (sec == 0)
        {
            ffmpeg_err("sec = 0 ignoring\n");
            return cERR_CONTAINER_FFMPEG_ERR;
        }
        else
        {
            int64_t currPts = -1;
            int32_t ret = context->playback->Command(context, PLAYBACK_PTS, &currPts);
            if (ret != 0)
            {
                ffmpeg_err("fail to get current PTS\n");
                return cERR_CONTAINER_FFMPEG_ERR;
            }
            sec += currPts / 90000;
        }
    }
    
    ffmpeg_printf(10, "goto %d sec\n", sec);
    if (sec < 0)
    {
        sec = 0;
    }
    
    context->manager->video->Command(context, MANAGER_GET_TRACK, &videoTrack);
    context->manager->audio->Command(context, MANAGER_GET_TRACK, &audioTrack);

    if (videoTrack != NULL)
    {
        current = videoTrack;
    }
    else if (audioTrack != NULL)
    {
        current = audioTrack;
    }
    else
    {
        ffmpeg_err( "no track available to seek\n");
        return cERR_CONTAINER_FFMPEG_ERR;
    }

    if (sec < 0)
    {
        seek_target_flag |= AVSEEK_FLAG_BACKWARD;
    }

    getMutex(__FILE__, __FUNCTION__,__LINE__);

    if (!context->playback || !context->playback->isPlaying)
    {
        releaseMutex(__FILE__, __FUNCTION__,__LINE__);
        return cERR_CONTAINER_FFMPEG_NO_ERROR;
    }

    ffmpeg_printf(10, "iformat->flags %d\n", avContextTab[0]->iformat->flags);

    if (avContextTab[0]->iformat->flags & AVFMT_TS_DISCONT)
    {
        /* konfetti: for ts streams seeking frame per seconds does not work (why?).
        * I take this algo partly from ffplay.c.
        *
        * seeking per HTTP does still not work very good. forward seeks everytime
        * about 10 seconds, backward does not work.
        */

        off_t pos = avio_tell(avContextTab[0]->pb);

        ffmpeg_printf(10, "pos %lld %d\n", pos, avContextTab[0]->bit_rate);

        if (avContextTab[0]->bit_rate)
        {
            sec *= avContextTab[0]->bit_rate / 8;
            ffmpeg_printf(10, "bit_rate %d\n", avContextTab[0]->bit_rate);
        }
        else
        {
            sec *= 180000;
        }
        
        pos = sec;
        
        if (pos < 0)
        {
           pos = 0;
        }
        
        ffmpeg_printf(10, "1. seeking to position %lld bytes ->sec %d\n", pos, sec);

        seek_target_bytes = pos;
        do_seek_target_bytes = 1;

    } 
    else
    {
        seek_target_seconds = sec * AV_TIME_BASE;
        do_seek_target_seconds = 1;
    }

    releaseMutex(__FILE__, __FUNCTION__,__LINE__);
    return cERR_CONTAINER_FFMPEG_NO_ERROR;
}

static int32_t container_ffmpeg_get_length(Context_t *context, int64_t *length) 
{
    ffmpeg_printf(50, "\n");
    Track_t * videoTrack = NULL;
    Track_t * audioTrack = NULL;
    Track_t * current = NULL;

    if (length == NULL) 
    {
        ffmpeg_err( "null pointer passed\n");
        return cERR_CONTAINER_FFMPEG_ERR;
    }

    context->manager->video->Command(context, MANAGER_GET_TRACK, &videoTrack);
    context->manager->audio->Command(context, MANAGER_GET_TRACK, &audioTrack);

    if (videoTrack != NULL)
    {
        current = videoTrack;
    }
    else if (audioTrack != NULL)
    {
        current = audioTrack;
    }
    
    *length = 0;

    if (current != NULL) 
    {
        if (current->duration == 0)
        {
            return cERR_CONTAINER_FFMPEG_ERR;
        }
        else
        {
            *length = current->duration / 1000;
        }
    }
    else 
    {
        if (avContextTab[0] != NULL)
        {
            *length = avContextTab[0]->duration / 1000;
        } 
        else
        {
           ffmpeg_err( "no Track not context ->no problem :D\n");
           return cERR_CONTAINER_FFMPEG_ERR;
        }
    }

    return cERR_CONTAINER_FFMPEG_NO_ERROR;
}

static int32_t container_ffmpeg_switch_audio(Context_t *context, int32_t *arg)
{
    ffmpeg_printf(10, "track %d\n", *arg);
    
    /* Hellmaster1024: nothing to do here!*/
    int64_t sec = -5;
    context->playback->Command(context, PLAYBACK_SEEK, (void*)&sec);
    return cERR_CONTAINER_FFMPEG_NO_ERROR;
}

static int32_t container_ffmpeg_switch_subtitle(Context_t *context, int32_t *arg)
{
    ffmpeg_printf(10, "track %d\n", *arg);
    
    /* This is made to flush inside the buffer because 
     * subtitles frame was already read and ignored
     * we seek to force ffmpeg to read once again the same data
     * but now we will not ignore subtitle frame
     */
    int64_t sec = -5;
    context->playback->Command(context, PLAYBACK_SEEK, (void*)&sec);
    return cERR_CONTAINER_FFMPEG_NO_ERROR;
}

/* konfetti comment: I dont like the mechanism of overwriting
 * the pointer in infostring. This lead in most cases to
 * user errors, like it is in the current version (libeplayer2 <-->e2->servicemp3.cpp)
 * From e2 there is passed a tag=strdup here and we overwrite this
 * strdupped tag. This lead to dangling pointers which are never freed!
 * I do not free the string here because this is the wrong way. The mechanism
 * should be changed, or e2 should pass it in a different way...
 */
static int32_t container_ffmpeg_get_info(Context_t* context, char ** infoString)
{
    Track_t *videoTrack = NULL;
    Track_t *audioTrack = NULL;
    char     *meta = NULL;

    ffmpeg_printf(20, ">\n");

    if (avContextTab[0] != NULL)
    {
        if ((infoString == NULL) || (*infoString == NULL))
        {
            ffmpeg_err("infostring NULL\n");
            return cERR_CONTAINER_FFMPEG_ERR;
        }

        ffmpeg_printf(20, "%s\n", *infoString);

        context->manager->video->Command(context, MANAGER_GET_TRACK, &videoTrack);
        context->manager->audio->Command(context, MANAGER_GET_TRACK, &audioTrack);

        if ((meta = searchMeta(avContextTab[0]->metadata, *infoString)) == NULL)
        {
            if (audioTrack != NULL)
            {
                AVStream* stream = audioTrack->stream;
                meta = searchMeta(stream->metadata, *infoString);
            }

            if ((meta == NULL) && (videoTrack != NULL))
            {
                AVStream* stream = videoTrack->stream;
                meta = searchMeta(stream->metadata, *infoString);
            }
        }

        if (meta != NULL)
        {
            *infoString = strdup(meta);
        }
        else
        {
            ffmpeg_printf(1, "no metadata found for \"%s\"\n", *infoString);
            *infoString = strdup("not found");
        }
    } 
    else
    {
        ffmpeg_err("avContext NULL\n");
        return cERR_CONTAINER_FFMPEG_ERR;
    }

    return cERR_CONTAINER_FFMPEG_NO_ERROR;
}

static int32_t Command(void  *_context, ContainerCmd_t command, void *argument)
{
    Context_t  *context = (Context_t*) _context;
    int ret = cERR_CONTAINER_FFMPEG_NO_ERROR;

    ffmpeg_printf(50, "Command %d\n", command);

    if(command != CONTAINER_SET_BUFFER_SEEK_TIME && 
       command != CONTAINER_SET_BUFFER_SIZE && 
       command != CONTAINER_GET_BUFFER_SIZE && 
       command != CONTAINER_GET_BUFFER_STATUS &&
       command != CONTAINER_STOP_BUFFER && 
       command != CONTAINER_INIT && !avContextTab[0])
    {
        return cERR_CONTAINER_FFMPEG_ERR;
    }
    
    
    switch(command)
    {
    case CONTAINER_INIT:
    {
        PlayFiles_t *playFilesNames = (PlayFiles_t *)argument;
        ret = container_ffmpeg_init(context, playFilesNames);
        break;
    }
    case CONTAINER_PLAY:  
    {
        ret = container_ffmpeg_play(context);
        break;
    }
    case CONTAINER_STOP:
    {
        ret = container_ffmpeg_stop(context);
        break;
    }
    case CONTAINER_SEEK: 
    {
        ret = container_ffmpeg_seek(context, (int64_t)*((int64_t*)argument), 0);
        break;
    }
    case CONTAINER_SEEK_ABS: 
    {
        ret = container_ffmpeg_seek(context, (int64_t)*((int64_t*)argument), -1);
        break;
    }
    case CONTAINER_LENGTH: 
    {
        int64_t length = 0;
        ret = container_ffmpeg_get_length(context, &length);
        *((int64_t*)argument) = (int64_t)length;
        break;
    }
    case CONTAINER_SWITCH_AUDIO: 
    {
        ret = container_ffmpeg_switch_audio(context, (int32_t*) argument);
        break;
    }
    case CONTAINER_SWITCH_SUBTITLE: 
    {
        ret = container_ffmpeg_switch_subtitle(context, (int32_t*) argument);
        break;
    }
    case CONTAINER_INFO:
    {
        ret = container_ffmpeg_get_info(context, (char **)argument);
        break;
    }
    case CONTAINER_STATUS:
    {
        *((int32_t*)argument) = hasPlayThreadStarted;
        break;
    }
    case CONTAINER_LAST_PTS:
    {
        *((int64_t*)argument) = latestPts;
        break;
    }
    case CONTAINER_SET_BUFFER_SIZE:
    {
        ret = container_set_ffmpeg_buf_size((int32_t *) argument);
        break;
    }
    case CONTAINER_GET_BUFFER_SIZE:
    {
        int32_t size = 0;
        ret = container_get_ffmpeg_buf_size(&size);
        *((int32_t*)argument) = size;
        break;
    }   
    default:
        ffmpeg_err("ContainerCmd %d not supported!\n", command);
        ret = cERR_CONTAINER_FFMPEG_ERR;
        break;
    }

    ffmpeg_printf(50, "exiting with value %d\n", ret);
    return ret;
}

static char *FFMPEG_Capabilities[] = {"avi", "mkv", "mp4", "ts", "mov", "flv", "flac", "mp3", "mpg", "m2ts", "vob", "evo", "wmv","wma", "asf", "mp2", "m4v", "m4a", "fla", "divx", "dat", "mpeg", "trp", "mts", "vdr", "ogg", "wav", "wtv", "asx", "mvi", "png", "jpg", "ra", "ram", "rm", "3gp", "amr", NULL };

Container_t FFMPEGContainer = {
    "FFMPEG",
    &Command,
    FFMPEG_Capabilities
};
