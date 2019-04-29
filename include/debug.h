#include <stdio.h>
#define log_error(fmt, x...) do { printf("[%s:%s] " fmt, __FILE__, __FUNCTION__, ## x); } while (0)
#define log_printf(maxlevel, level, fmt, x...) do { if (maxlevel >= level) printf("[%s:%s] " fmt, __FILE__, __FUNCTION__, ## x); } while (0)


/*******************************************
 * ffmpeg
 *******************************************/
#define FFMPEG_DEBUG_LEVEL 0
#define FFMPEG_SILENT

#if FFMPEG_DEBUG_LEVEL
#define ffmpeg_printf(...) log_printf(FFMPEG_DEBUG_LEVEL, __VA_ARGS__)
#else
#define ffmpeg_printf(...)
#endif

#ifndef FFMPEG_SILENT
#define ffmpeg_err(...) log_error(__VA_ARGS__)
#else
#define ffmpeg_err(...)
#endif

/*******************************************
 * container
 *******************************************/
#define CONTAINER_DEBUG_LEVEL 0
#define CONTAINER_SILENT

#if CONTAINER_DEBUG_LEVEL
#define container_printf(...) log_printf(CONTAINER_DEBUG_LEVEL, __VA_ARGS__)
#else
#define container_printf(...)
#endif

#ifndef CONTAINER_SILENT
#define container_err(...) log_error(__VA_ARGS__)
#else
#define container_err(...)
#endif

/*******************************************
 * latmenc
 *******************************************/
#define LATMENC_DEBUG_LEVEL 0
#define LATMENC_SILENT

#if LATMENC_DEBUG_LEVEL
#define latmenc_printf(...) log_printf(LATMENC_DEBUG_LEVEL, __VA_ARGS__)
#else
#define latmenc_printf(...)
#endif

#ifndef LATMENC_SILENT
#define latmenc_err(...) log_error(__VA_ARGS__)
#else
#define latmenc_err(...)
#endif

/*******************************************
 * audio_mgr
 *******************************************/
#define AUDIO_MGR_DEBUG_LEVEL 0
#define AUDIO_MGR_SILENT

#if AUDIO_MGR_DEBUG_LEVEL
#define audio_mgr_printf(...) log_printf(AUDIO_MGR_DEBUG_LEVEL, __VA_ARGS__)
#else
#define audio_mgr_printf(...)
#endif

#ifndef AUDIO_MGR_SILENT
#define audio_mgr_err(...) log_error(__VA_ARGS__)
#else
#define audio_mgr_err(...)
#endif

/*******************************************
 * subtitle_mgr
 *******************************************/
#define SUBTITLE_MGR_DEBUG_LEVEL 0
#define SUBTITLE_MGR_SILENT

#if SUBTITLE_MGR_DEBUG_LEVEL
#define subtitle_mgr_printf(...) log_printf(SUBTITLE_MGR_DEBUG_LEVEL, __VA_ARGS__)
#else
#define subtitle_mgr_printf(...)
#endif

#ifndef SUBTITLE_MGR_SILENT
#define subtitle_mgr_err(...) log_error(__VA_ARGS__)
#else
#define subtitle_mgr_err(...)
#endif

/*******************************************
 * video_mgr
 *******************************************/
#define VIDEO_MGR_DEBUG_LEVEL 0
#define VIDEO_MGR_SILENT

#if VIDEO_MGR_DEBUG_LEVEL
#define video_mgr_printf(...) log_printf(VIDEO_MGR_DEBUG_LEVEL, __VA_ARGS__)
#else
#define video_mgr_printf(...)
#endif

#ifndef VIDEO_MGR_SILENT
#define video_mgr_err(...) log_error(__VA_ARGS__)
#else
#define video_mgr_err(...)
#endif

/*******************************************
 * linuxdvb
 *******************************************/
#define LINUXDVB_DEBUG_LEVEL 0
#define LINUXDVB_SILENT

#if LINUXDVB_DEBUG_LEVEL
#define linuxdvb_printf(...) log_printf(LINUXDVB_DEBUG_LEVEL, __VA_ARGS__)
#else
#define linuxdvb_printf(...)
#endif

#ifndef LINUXDVB_SILENT
#define linuxdvb_err(...) log_error(__VA_ARGS__)
#else
#define linuxdvb_err(...)
#endif

/*******************************************
 * buff
 *******************************************/
#define BUFF_DEBUG_LEVEL 0
#define BUFF_SILENT

#if BUFF_DEBUG_LEVEL
#define buff_printf(...) log_printf(BUFF_DEBUG_LEVEL, __VA_ARGS__)
#else
#define buff_printf(...)
#endif

#ifndef BUFF_SILENT
#define buff_err(...) log_error(__VA_ARGS__)
#else
#define buff_err(...)
#endif

/*******************************************
 * output
 *******************************************/
#define OUTPUT_DEBUG_LEVEL 0
#define OUTPUT_SILENT

#if OUTPUT_DEBUG_LEVEL
#define output_printf(...) log_printf(OUTPUT_DEBUG_LEVEL, __VA_ARGS__)
#else
#define output_printf(...)
#endif

#ifndef OUTPUT_SILENT
#define output_err(...) log_error(__VA_ARGS__)
#else
#define output_err(...)
#endif

/*******************************************
 * subtitle
 *******************************************/
#define SUBTITLE_DEBUG_LEVEL 0
#define SUBTITLE_SILENT

#if SUBTITLE_DEBUG_LEVEL
#define subtitle_printf(...) log_printf(SUBTITLE_DEBUG_LEVEL, __VA_ARGS__)
#else
#define subtitle_printf(...)
#endif

#ifndef SUBTITLE_SILENT
#define subtitle_err(...) log_error(__VA_ARGS__)
#else
#define subtitle_err(...)
#endif

/*******************************************
 * writer
 *******************************************/
#define WRITER_DEBUG_LEVEL 0
#define WRITER_SILENT

#if WRITER_DEBUG_LEVEL
#define writer_printf(...) log_printf(WRITER_DEBUG_LEVEL, __VA_ARGS__)
#else
#define writer_printf(...)
#endif

#ifndef WRITER_SILENT
#define writer_err(...) log_error(__VA_ARGS__)
#else
#define writer_err(...)
#endif

/*******************************************
 * playback
 *******************************************/
#define PLAYBACK_DEBUG_LEVEL 0
#define PLAYBACK_SILENT

#if PLAYBACK_DEBUG_LEVEL
#define playback_printf(...) log_printf(PLAYBACK_DEBUG_LEVEL, __VA_ARGS__)
#else
#define playback_printf(...)
#endif

#ifndef PLAYBACK_SILENT
#define playback_err(...) log_error(__VA_ARGS__)
#else
#define playback_err(...)
#endif

/*******************************************
 * aac
 *******************************************/
#define AAC_DEBUG_LEVEL 0
#define AAC_SILENT

#if AAC_DEBUG_LEVEL
#define aac_printf(...) log_printf(AAC_DEBUG_LEVEL, __VA_ARGS__)
#else
#define aac_printf(...)
#endif

#ifndef AAC_SILENT
#define aac_err(...) log_error(__VA_ARGS__)
#else
#define aac_err(...)
#endif

/*******************************************
 * ac3
 *******************************************/
#define AC3_DEBUG_LEVEL 0
#define AC3_SILENT

#if AC3_DEBUG_LEVEL
#define ac3_printf(...) log_printf(AC3_DEBUG_LEVEL, __VA_ARGS__)
#else
#define ac3_printf(...)
#endif

#ifndef AC3_SILENT
#define ac3_err(...) log_error(__VA_ARGS__)
#else
#define ac3_err(...)
#endif

/*******************************************
 * amr
 *******************************************/
#define AMR_DEBUG_LEVEL 0
#define AMR_SILENT

#if AMR_DEBUG_LEVEL
#define amr_printf(...) log_printf(AMR_DEBUG_LEVEL, __VA_ARGS__)
#else
#define amr_printf(...)
#endif

#ifndef AMR_SILENT
#define amr_err(...) log_error(__VA_ARGS__)
#else
#define amr_err(...)
#endif

/*******************************************
 * divx
 *******************************************/
#define DIVX_DEBUG_LEVEL 0
#define DIVX_SILENT

#if DIVX_DEBUG_LEVEL
#define divx_printf(...) log_printf(DIVX_DEBUG_LEVEL, __VA_ARGS__)
#else
#define divx_printf(...)
#endif

#ifndef DIVX_SILENT
#define divx_err(...) log_error(__VA_ARGS__)
#else
#define divx_err(...)
#endif

/*******************************************
 * dts
 *******************************************/
#define DTS_DEBUG_LEVEL 0
#define DTS_SILENT

#if DTS_DEBUG_LEVEL
#define dts_printf(...) log_printf(DTS_DEBUG_LEVEL, __VA_ARGS__)
#else
#define dts_printf(...)
#endif

#ifndef DTS_SILENT
#define dts_err(...) log_error(__VA_ARGS__)
#else
#define dts_err(...)
#endif

/*******************************************
 * h263
 *******************************************/
#define H263_DEBUG_LEVEL 0
#define H263_SILENT

#if H263_DEBUG_LEVEL
#define h263_printf(...) log_printf(H263_DEBUG_LEVEL, __VA_ARGS__)
#else
#define h263_printf(...)
#endif

#ifndef H263_SILENT
#define h263_err(...) log_error(__VA_ARGS__)
#else
#define h263_err(...)
#endif

/*******************************************
 * h264
 *******************************************/
#define H264_DEBUG_LEVEL 0
#define H264_SILENT

#if H264_DEBUG_LEVEL
#define h264_printf(...) log_printf(H264_DEBUG_LEVEL, __VA_ARGS__)
#else
#define h264_printf(...)
#endif

#ifndef H264_SILENT
#define h264_err(...) log_error(__VA_ARGS__)
#else
#define h264_err(...)
#endif

/*******************************************
 * h265
 *******************************************/
#define H265_DEBUG_LEVEL 0
#define H265_SILENT

#if H265_DEBUG_LEVEL
#define h265_printf(...) log_printf(H265_DEBUG_LEVEL, __VA_ARGS__)
#else
#define h265_printf(...)
#endif

#ifndef H265_SILENT
#define h265_err(...) log_error(__VA_ARGS__)
#else
#define h265_err(...)
#endif

/*******************************************
 * lpcm
 *******************************************/
#define LPCM_DEBUG_LEVEL 0
#define LPCM_SILENT

#if LPCM_DEBUG_LEVEL
#define lpcm_printf(...) log_printf(LPCM_DEBUG_LEVEL, __VA_ARGS__)
#else
#define lpcm_printf(...)
#endif

#ifndef LPCM_SILENT
#define lpcm_err(...) log_error(__VA_ARGS__)
#else
#define lpcm_err(...)
#endif

/*******************************************
 * mp3
 *******************************************/
#define MP3_DEBUG_LEVEL 0
#define MP3_SILENT

#if MP3_DEBUG_LEVEL
#define mp3_printf(...) log_printf(MP3_DEBUG_LEVEL, __VA_ARGS__)
#else
#define mp3_printf(...)
#endif

#ifndef MP3_SILENT
#define mp3_err(...) log_error(__VA_ARGS__)
#else
#define mp3_err(...)
#endif

/*******************************************
 * mpeg2
 *******************************************/
#define MPEG2_DEBUG_LEVEL 0
#define MPEG2_SILENT

#if MPEG2_DEBUG_LEVEL
#define mpeg2_printf(...) log_printf(MPEG2_DEBUG_LEVEL, __VA_ARGS__)
#else
#define mpeg2_printf(...)
#endif

#ifndef MPEG2_SILENT
#define mpeg2_err(...) log_error(__VA_ARGS__)
#else
#define mpeg2_err(...)
#endif

/*******************************************
 * mpeg4
 *******************************************/
#define MPEG4_DEBUG_LEVEL 0
#define MPEG4_SILENT

#if MPEG4_DEBUG_LEVEL
#define mpeg4_printf(...) log_printf(MPEG4_DEBUG_LEVEL, __VA_ARGS__)
#else
#define mpeg4_printf(...)
#endif

#ifndef MPEG4_SILENT
#define mpeg4_err(...) log_error(__VA_ARGS__)
#else
#define mpeg4_err(...)
#endif

/*******************************************
 * pcm
 *******************************************/
#define PCM_DEBUG_LEVEL 0
#define PCM_SILENT

#if PCM_DEBUG_LEVEL
#define pcm_printf(...) log_printf(PCM_DEBUG_LEVEL, __VA_ARGS__)
#else
#define pcm_printf(...)
#endif

#ifndef PCM_SILENT
#define pcm_err(...) log_error(__VA_ARGS__)
#else
#define pcm_err(...)
#endif

/*******************************************
 * vc1
 *******************************************/
#define VC1_DEBUG_LEVEL 0
#define VC1_SILENT

#if VC1_DEBUG_LEVEL
#define vc1_printf(...) log_printf(VC1_DEBUG_LEVEL, __VA_ARGS__)
#else
#define vc1_printf(...)
#endif

#ifndef VC1_SILENT
#define vc1_err(...) log_error(__VA_ARGS__)
#else
#define vc1_err(...)
#endif

/*******************************************
 * vp
 *******************************************/
#define VP_DEBUG_LEVEL 0
#define VP_SILENT

#if VP_DEBUG_LEVEL
#define vp_printf(...) log_printf(VP_DEBUG_LEVEL, __VA_ARGS__)
#else
#define vp_printf(...)
#endif

#ifndef VP_SILENT
#define vp_err(...) log_error(__VA_ARGS__)
#else
#define vp_err(...)
#endif

/*******************************************
 * wma
 *******************************************/
#define WMA_DEBUG_LEVEL 0
#define WMA_SILENT

#if WMA_DEBUG_LEVEL
#define wma_printf(...) log_printf(WMA_DEBUG_LEVEL, __VA_ARGS__)
#else
#define wma_printf(...)
#endif

#ifndef WMA_SILENT
#define wma_err(...) log_error(__VA_ARGS__)
#else
#define wma_err(...)
#endif

/*******************************************
 * wmv
 *******************************************/
#define WMV_DEBUG_LEVEL 0
#define WMV_SILENT

#if WMV_DEBUG_LEVEL
#define wmv_printf(...) log_printf(WMV_DEBUG_LEVEL, __VA_ARGS__)
#else
#define wmv_printf(...)
#endif

#ifndef WMV_SILENT
#define wmv_err(...) log_error(__VA_ARGS__)
#else
#define wmv_err(...)
#endif

/*******************************************
 * mjpeg
 *******************************************/
#define MJPEG_DEBUG_LEVEL 0
#define MJPEG_SILENT

#if MJPEG_DEBUG_LEVEL
#define mjpeg_printf(...) log_printf(MJPEG_DEBUG_LEVEL, __VA_ARGS__)
#else
#define mjpeg_printf(...)
#endif

#ifndef MJPEG_SILENT
#define mjpeg_err(...) log_error(__VA_ARGS__)
#else
#define mjpeg_err(...)
#endif

/*******************************************
 * bcma
 *******************************************/
#define BCMA_DEBUG_LEVEL 0
#define BCMA_SILENT

#if BCMA_DEBUG_LEVEL
#define bcma_printf(...) log_printf(BCMA_DEBUG_LEVEL, __VA_ARGS__)
#else
#define bcma_printf(...)
#endif

#ifndef BCMA_SILENT
#define bcma_err(...) log_error(__VA_ARGS__)
#else
#define bcma_err(...)
#endif

/*******************************************
 * plugin
 *******************************************/
#define PLUGIN_DEBUG_LEVEL 0
#define PLUGIN_SILENT

#if PLUGIN_DEBUG_LEVEL
#define plugin_printf(...) log_printf(PLUGIN_DEBUG_LEVEL, __VA_ARGS__)
#else
#define plugin_printf(...)
#endif

#ifndef PLUGIN_SILENT
#define plugin_err(...) log_error(__VA_ARGS__)
#else
#define plugin_err(...)
#endif