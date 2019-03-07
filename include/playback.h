#ifndef PLAYBACK_H_
#define PLAYBACK_H_
#include <sys/types.h>
#include <stdint.h>
#include <stdbool.h>

typedef void( * PlaybackDieNowCallback )();
bool PlaybackDieNowRegisterCallback(PlaybackDieNowCallback callback);

typedef enum {PLAYBACK_OPEN, PLAYBACK_CLOSE, PLAYBACK_PLAY, PLAYBACK_STOP, PLAYBACK_PAUSE, PLAYBACK_CONTINUE, PLAYBACK_FLUSH, PLAYBACK_TERM, PLAYBACK_FASTFORWARD, PLAYBACK_SEEK, PLAYBACK_SEEK_ABS, PLAYBACK_PTS, PLAYBACK_LENGTH, PLAYBACK_SWITCH_AUDIO, PLAYBACK_SWITCH_SUBTITLE, PLAYBACK_INFO, PLAYBACK_SLOWMOTION, PLAYBACK_FASTBACKWARD, PLAYBACK_GET_FRAME_COUNT} PlaybackCmd_t;

typedef struct PlaybackHandler_s 
{
    char *Name;

    int32_t fd;

    uint8_t isFile;
    uint8_t isHttp;

    uint8_t isPlaying;
    uint8_t isPaused;
    uint8_t isForwarding;
    uint8_t isSeeking;
    uint8_t isCreationPhase;

    int32_t BackWard;
    int32_t SlowMotion;
    int32_t Speed;
    int32_t AVSync;

    uint8_t isVideo;
    uint8_t isAudio;
    uint8_t isSubtitle;
    uint8_t abortRequested;

    int32_t (* Command) (/*Context_t*/void  *, PlaybackCmd_t, void *);
    char * uri;
    off_t size;
    uint8_t noprobe; /* hack: only minimal probing in av_find_stream_info */
    uint8_t isLoopMode;
    uint8_t isTSLiveMode;

    void *stamp;
} PlaybackHandler_t;

#endif
