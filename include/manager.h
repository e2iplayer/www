#ifndef MANAGER_H_
#define MANAGER_H_

#include <stdio.h>
#include <stdint.h>
 
typedef enum {
    MANAGER_ADD,
    MANAGER_LIST,
    MANAGER_GET,
    MANAGER_GETNAME,
    MANAGER_SET,
    MANAGER_GETENCODING,
    MANAGER_DEL,
    MANAGER_GET_TRACK,
    MANAGER_GET_TRACK_DESC,
    MANAGER_INIT_UPDATE,
    MANAGER_UPDATED_TRACK_INFO,
    MANAGER_REGISTER_UPDATED_TRACK_INFO,
    MANAGER_REF_LIST,
    MANAGER_REF_LIST_SIZE,
} ManagerCmd_t;

typedef enum {
    eTypeES,
    eTypePES
} eTrackTypeEplayer;

typedef struct Track_s {
    char *                Name;
    char *                Encoding;
    int32_t               Id;
    int32_t               AVIdx;

    /* new field for ffmpeg - add at the end so no problem
    * can occur with not changed srt saa container
    */
    char                  *language;

    /* length of track */
    int64_t               duration;
    uint32_t              frame_rate;
    uint32_t              TimeScale;
    int32_t               version;
    long long int         pts;
    long long int         dts;
    
    /* for later use: */
    eTrackTypeEplayer     type;
    int                   width;
    int                   height;
    int32_t               aspect_ratio_num;
    int32_t               aspect_ratio_den;

    /* stream from ffmpeg */
    void               *  stream;
    /* AVCodecContext  for steam */
    void               *  avCodecCtx;
    /* codec extra data (header or some other stuff) */
    void               *  extraData;
    int		              extraSize;

    uint8_t*              aacbuf;
    unsigned int          aacbuflen;
    int                   have_aacheader;

    /* If player2 or the elf do not support decoding of audio codec set this.
     * AVCodec is than used for softdecoding and stream will be injected as PCM */
    int                   inject_as_pcm;
    int                   inject_raw_pcm;

    int                   pending;
} Track_t;

typedef struct TrackDescription_s 
{
    int                   Id;
    char *                Name;
    char *                Encoding;
    unsigned int          frame_rate;
    int                   width;
    int                   height;
    int32_t               aspect_ratio_num;
    int32_t               aspect_ratio_den;
    int                   progressive;
    
} TrackDescription_t;

typedef struct Manager_s 
{
    char * Name;
    int (* Command) (/*Context_t*/void  *, ManagerCmd_t, void *);
    char ** Capabilities;

} Manager_t;

typedef struct ManagerHandler_s 
{
    char *Name;
    Manager_t *audio;
    Manager_t *video;
    Manager_t *subtitle;
} ManagerHandler_t;

void freeTrack(Track_t* track);
void copyTrack(Track_t* to, Track_t* from);

#endif
