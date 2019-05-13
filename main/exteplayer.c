/*
 * eplayer3: command line playback using libeplayer3
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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sched.h>
#include <signal.h>
#include <inttypes.h>
#include <stdarg.h>

#include <sys/ioctl.h>
#include <sys/prctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>

#include <pthread.h>

#include "common.h"
#include "misc.h"

#include "debug.h"

#define DUMP_BOOL(x) 0 == x ? "false"  : "true"
#define IPTV_MAX_FILE_PATH 1024

extern int ffmpeg_av_dict_set(const char *key, const char *value, int flags);
extern void       aac_software_decoder_set(const int32_t val);
extern void  aac_latm_software_decoder_set(const int32_t val);
extern void       dts_software_decoder_set(const int32_t val);
extern void       wma_software_decoder_set(const int32_t val);
extern void       ac3_software_decoder_set(const int32_t val);
extern void      eac3_software_decoder_set(const int32_t val);
extern void       mp3_software_decoder_set(const int32_t val);
extern void       amr_software_decoder_set(const int32_t val);
extern void    vorbis_software_decoder_set(const int32_t val);
extern void      opus_software_decoder_set(const int32_t val);

extern void            rtmp_proto_impl_set(const int32_t val);
extern void        flv2mpeg4_converter_set(const int32_t val);
extern void        sel_program_id_set(const int32_t val);

extern void pcm_resampling_set(int32_t val);
extern void stereo_software_decoder_set(int32_t val);
extern void insert_pcm_as_lpcm_set(int32_t val);
extern void progressive_playback_set(int32_t val);

extern OutputHandler_t         OutputHandler;
extern PlaybackHandler_t       PlaybackHandler;
extern ContainerHandler_t      ContainerHandler;
extern ManagerHandler_t        ManagerHandler;

static Context_t *g_player = NULL;

static void TerminateAllSockets(void)
{
    int i;
    for(i=0; i<1024; ++i)
    {
        if( 0 == shutdown(i, SHUT_RDWR) )
        {
            /* yes, I know that this is not good practice and I know what this could cause 
             * but in this use case it can be accepted. 
             * We must close socket because without closing it recv will return 0 (after shutdown)
             * 0 is not correctly handled by external libraries
             */
            close(i);
        }
    }
}

static int g_pfd[2] = {-1, -1}; /* Used to wake terminate thread and kbhit */
static int isPlaybackStarted = 0;
static pthread_mutex_t playbackStartMtx;

static int32_t g_windows_width = 1280;
static int32_t g_windows_height = 720;
static char *g_graphic_sub_path;

const char* GetGraphicSubPath()
{
    return g_graphic_sub_path;
}

int32_t GetGraphicWindowWidth()
{
    return g_windows_width;
}

int32_t GetGraphicWindowHeight()
{
    return g_windows_height;
}

void E2iSendMsg(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
}

void E2iStartMsg(void)
{
    flockfile(stderr);
}

void E2iEndMsg(void)
{
    funlockfile(stderr);
}


static void TerminateWakeUp()
{
    int ret = write(g_pfd[1], "x", 1);
    if (ret != 1) {
        printf("TerminateWakeUp write return %d\n", ret);
    }
}

static void *TermThreadFun(void *arg)
{
    const char *socket_path = "/tmp/.exteplayerterm.socket";
    struct sockaddr_un addr;
    int fd = -1;
    int cl = -1;
    int nfds = 1;
    fd_set readfds;
    
    unlink(socket_path);
    if ( (fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) 
    {
        perror("TermThreadFun socket error");
        goto finish;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path)-1);

    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) 
    {
        perror("TermThreadFun bind error");
        goto finish;
    }

    if (listen(fd, 1) == -1) 
    {
        perror("TermThreadFun listen error");
        goto finish;
    }

    FD_ZERO(&readfds);
    FD_SET(g_pfd[0], &readfds);
    FD_SET(fd, &readfds);
    
    nfds = fd > g_pfd[0] ? fd + 1 : g_pfd[0] + 1;
    
    while (select(nfds, &readfds, NULL, NULL, NULL) == -1 
           && errno == EINTR)
    {
        /* Restart if interrupted by signal */
        continue;
    }
    
    if (FD_ISSET(fd, &readfds))
    {
        pthread_mutex_lock(&playbackStartMtx);
        PlaybackDieNow(1);
        if (isPlaybackStarted)
            TerminateAllSockets();
        else
            kill(getpid(), SIGINT);
        pthread_mutex_unlock(&playbackStartMtx);
    }

finish:
    close(cl);
    close(fd);
    pthread_exit(NULL);
    
}

static void map_inter_file_path(char *filename)
{
    if (0 == strncmp(filename, "iptv://", 7))
    {
        FILE *f = fopen(filename + 7, "r");
        if (NULL != f)
        {
            size_t num = fread(filename, 1, IPTV_MAX_FILE_PATH-1, f);
            fclose(f);
            if (num > 0 && filename[num-1] == '\n')
            {
                filename[num-1] = '\0';
            }
            else
            {
                filename[num] = '\0';
            }
        }
    }
}

static int kbhit(void)
{
    struct timeval tv;
    fd_set readfds;

    tv.tv_sec = 1;
    tv.tv_usec = 0;

    FD_ZERO(&readfds);
    FD_SET(0,&readfds);
    FD_SET(g_pfd[0], &readfds);

    if(-1 == select(g_pfd[0] + 1, &readfds, NULL, NULL, &tv))
    {
        return 0;
    }

    if(FD_ISSET(0, &readfds))
    {
        return 1;
    }

    return 0;
}

static void SetBuffering()
{
    static char buff[2048];
    memset( buff, '\0', sizeof(buff));
    if( setvbuf(stderr, buff, _IOLBF, sizeof(buff)) )
    {
        printf("SetBuffering: failed to change the buffer of stderr\n");
    }
    
    // make fgets not blocking 
    int flags = fcntl(stdin->_fileno, F_GETFL, 0); 
    fcntl(stdin->_fileno, F_SETFL, flags | O_NONBLOCK); 
}

static void SetNice(int prio)
{
#if 0
    setpriority(PRIO_PROCESS, 0, -8);
    
    int prio = sched_get_priority_max(SCHED_RR) / 2;
    struct sched_param param = {
        .sched_priority = prio
    };
    sched_setscheduler(0, SCHED_RR, &param);
#else
    int prevPrio = getpriority(PRIO_PROCESS, 0);
    if (-1 == setpriority(PRIO_PROCESS, 0, prio))
    {
        printf("setpriority - failed\n");
    }
#endif
}

static int HandleTracks(const Manager_t *ptrManager, const PlaybackCmd_t playbackSwitchCmd, const char *argvBuff)
{
    int commandRetVal = 0;
    
    if (NULL == ptrManager || NULL == argvBuff || 2 != strnlen(argvBuff, 2))
    {
        return -1;
    }
    
    switch (argvBuff[1]) 
    {
        case 'l': 
        {
            TrackDescription_t *TrackList = NULL;
            ptrManager->Command(g_player, MANAGER_LIST, &TrackList);
            if( NULL != TrackList) 
            {
                int i = 0;
                E2iStartMsg();
                E2iSendMsg("{\"%c_%c\": [", argvBuff[0], argvBuff[1]);
                for (i = 0; TrackList[i].Id >= 0; ++i) 
                {
                    if(0 < i)
                    {
                        E2iSendMsg(", ");
                    }
                    E2iSendMsg("{\"id\":%d,\"e\":\"%s\",\"n\":\"%s\"}", TrackList[i].Id , TrackList[i].Encoding, TrackList[i].Name);
                    free(TrackList[i].Encoding);
                    free(TrackList[i].Name);
                }
                E2iSendMsg("]}\n");
                E2iEndMsg();
                free(TrackList);
            }
            else
            {
                // not tracks 
                E2iSendMsg("{\"%c_%c\": []}\n", argvBuff[0], argvBuff[1]);
            }
            break;
        }
        case 'c': 
        {
            
            TrackDescription_t *track = NULL;
            ptrManager->Command(g_player, MANAGER_GET_TRACK_DESC, &track);
            if (NULL != track) 
            {
                if ('a' == argvBuff[0] || 's' == argvBuff[0])
                {
                    E2iSendMsg("{\"%c_%c\":{\"id\":%d,\"e\":\"%s\",\"n\":\"%s\"}}\n", argvBuff[0], argvBuff[1], track->Id , track->Encoding, track->Name);
                }
                else // video
                {
                    E2iSendMsg("{\"%c_%c\":{\"id\":%d,\"e\":\"%s\",\"n\":\"%s\",\"w\":%d,\"h\":%d,\"f\":%u,\"p\":%d,\"an\":%d,\"ad\":%d}}\n", \
                    argvBuff[0], argvBuff[1], track->Id , track->Encoding, track->Name, track->width, track->height, track->frame_rate, track->progressive, track->aspect_ratio_num, track->aspect_ratio_den);
                }
                free(track->Encoding);
                free(track->Name);
                free(track);
            }
            else
            {
                // no tracks
                if ('a' == argvBuff[0] || 's' == argvBuff[0])
                {
                    E2iSendMsg("{\"%c_%c\":{\"id\":%d,\"e\":\"%s\",\"n\":\"%s\"}}\n", argvBuff[0], argvBuff[1], -1, "", "");
                }
                else // video
                {
                    E2iSendMsg("{\"%c_%c\":{\"id\":%d,\"e\":\"%s\",\"n\":\"%s\",\"w\":%d,\"h\":%d,\"f\":%u,\"p\":%d}}\n", argvBuff[0], argvBuff[1], -1, "", "", -1, -1, 0, -1);
                }
            }
            break;
        }
        default: 
        {
            /* switch command available only for audio and subtitle tracks */
            if ('a' == argvBuff[0] || 's' == argvBuff[0])
            {
                int ok = 0;
                int id = -1;
                if ('i' == argvBuff[1])
                {
                    int idx = -1;
                    ok = sscanf(argvBuff+2, "%d", &idx);
                    if (idx >= 0)
                    {
                        TrackDescription_t *TrackList = NULL;
                        ptrManager->Command(g_player, MANAGER_LIST, &TrackList);
                        if( NULL != TrackList) 
                        {
                            int i = 0;
                            for (i = 0; TrackList[i].Id >= 0; ++i) 
                            {
                                if (idx == i)
                                {
                                    id = TrackList[i].Id;
                                }
                                free(TrackList[i].Encoding);
                                free(TrackList[i].Name);
                            }
                            free(TrackList);
                        }
                    }
                    else
                    {
                        id = idx;
                    }
                }
                else
                {
                    ok = sscanf(argvBuff+1, "%d", &id);
                }
                
                if(id >= 0 || (1 == ok && id == -1))
                {
                    commandRetVal = g_player->playback->Command(g_player, playbackSwitchCmd, (void*)&id);
                    E2iSendMsg("{\"%c_%c\":{\"id\":%d,\"sts\":%d}}\n", argvBuff[0], 's', id, commandRetVal);
                }
            }
            break;
        }
    }
    
    return commandRetVal;
}

static void UpdateVideoTrack()
{
    HandleTracks(g_player->manager->video, (PlaybackCmd_t)-1, "vc");
}

static int ParseParams(int argc,char* argv[], PlayFiles_t *playbackFiles, int *pAudioTrackIdx, int *subtitleTrackIdx, uint32_t *linuxDvbBufferSizeMB)
{   
    int ret = 0;
    int c;
    int digit_optind = 0;
    int aopt = 0, bopt = 0;
    char *copt = 0, *dopt = 0;
    while ( (c = getopt(argc, argv, "G:W:H:A:V:U:we3dlsrimva:n:x:u:c:h:o:p:P:t:9:0:1:4:f:b:F:S:O:")) != -1) 
    {
        switch (c) 
        {
        case 'G':
            g_graphic_sub_path = optarg;
        case 'W':
        {
            int val = atoi(optarg);
            if (val) g_windows_width = val;
            break;
        }
        case 'H':
        {
            int val = atoi(optarg);
            if (val) g_windows_height = val;
            break;
        }
        case 'a':
        {
            int flag = atoi(optarg);
            printf("Software decoder will be used for AAC codec\n");
            aac_software_decoder_set(flag & 0x01);
            aac_latm_software_decoder_set(flag & 0x02);
            break;
        }
        case 'e':
            printf("Software decoder will be used for EAC3 codec\n");
            eac3_software_decoder_set(1);
            break;
        case 'A':
            printf("Software decoder will be used for AMR codec\n");
            amr_software_decoder_set(atoi(optarg));
            break;
        case 'V':
            printf("Software decoder will be used for VORBIS codec\n");
            vorbis_software_decoder_set(atoi(optarg));
            break;
        case 'U':
            printf("Software decoder will be used for OPUS codec\n");
            vorbis_software_decoder_set(atoi(optarg));
            break;
        case '3':
            printf("Software decoder will be used for AC3 codec\n");
            ac3_software_decoder_set(1);
            break;
        case 'd':
            printf("Software decoder will be used for DTS codec\n");
            dts_software_decoder_set(1);
            break;
        case 'm':
            printf("Software decoder will be used for MP3 codec\n");
            mp3_software_decoder_set(1);
            break;
        case 'w':
            printf("Software decoder will be used for WMA codec\n");
            wma_software_decoder_set(1);
            break;
        case 'l':
            printf("Audio software decoding as LPCM\n");
            insert_pcm_as_lpcm_set(1);
            break;
        case 's':
            printf("Software decoder will decode to stereo\n");
            stereo_software_decoder_set(1);
            break;
        case 'r':
            printf("Software decoder do not use PCM resampling\n");
            pcm_resampling_set(0);
            break;
        case 'o':
            printf("Set progressive download to %d\n", atoi(optarg));
            progressive_playback_set(atoi(optarg));
            break;
        case 'p':
            SetNice(atoi(optarg));
            break;
        case 'P':
            sel_program_id_set(atoi(optarg));
            break;
        case 't':
            *pAudioTrackIdx = atoi(optarg);
            break;
        case '9':
            *subtitleTrackIdx = atoi(optarg);
            break;
        case 'x':
            if (optarg[0] != '\0')
            {
                playbackFiles->szSecondFile = malloc(IPTV_MAX_FILE_PATH);
                playbackFiles->szSecondFile[0] = '\0';
                strncpy(playbackFiles->szSecondFile, optarg, IPTV_MAX_FILE_PATH-1);
                playbackFiles->szSecondFile[IPTV_MAX_FILE_PATH] = '\0';
                map_inter_file_path(playbackFiles->szSecondFile);
            }
            break;
        case 'h':
            ffmpeg_av_dict_set("headers", optarg, 0);
            break;
        case 'u':
            ffmpeg_av_dict_set("user-agent", optarg, 0);
            break;
        case 'c':
            printf("For now cookies should be set via headers option!\n");
            ffmpeg_av_dict_set("cookies", optarg, 0);
            break;
        case 'i':
            printf("Play in (infinity) loop.\n");
            PlaybackHandler.isLoopMode = 1;
            break;
        case 'v':
            printf("Use live TS stream mode.\n");
            PlaybackHandler.isTSLiveMode = 1;
            break;
        case 'n':
            printf("Force rtmp protocol implementation\n");
            rtmp_proto_impl_set(atoi(optarg));
            break;
        case '0':
            ffmpeg_av_dict_set("video_rep_index", optarg, 0);
            break;
        case '1':
            ffmpeg_av_dict_set("audio_rep_index", optarg, 0);
            break;
        case '4':
#ifdef HAVE_FLV2MPEG4_CONVERTER
            flv2mpeg4_converter_set(atoi(optarg));
#endif
            break;
        case 'f':
        {
            char *ffopt = strdup(optarg);
            char *ffval = strchr(ffopt, '=');
            if (ffval)
            {
                *ffval = '\0';
                ffval += 1;
                ffmpeg_av_dict_set(ffopt, ffval, 0);
            }
            free(ffopt);
            break;
        }
        case 'b':
            *linuxDvbBufferSizeMB = 1024 * 1024 * atoi(optarg);
            break;
        case 'S':
            playbackFiles->iFirstFileSize = (uint64_t) strtoull(optarg, (char **)NULL, 10);
            break;
        case 'O':
            playbackFiles->iFirstMoovAtomOffset = (uint64_t) strtoull(optarg, (char **)NULL, 10);
            break;
        case 'F':
            if (optarg[0] != '\0')
            {
                playbackFiles->szFirstMoovAtomFile = malloc(IPTV_MAX_FILE_PATH);
                playbackFiles->szFirstMoovAtomFile[0] = '\0';
                strncpy(playbackFiles->szFirstMoovAtomFile, optarg, IPTV_MAX_FILE_PATH-1);
                playbackFiles->szFirstMoovAtomFile[IPTV_MAX_FILE_PATH] = '\0';
                map_inter_file_path(playbackFiles->szFirstMoovAtomFile);
            }
            break;
        default:
            printf ("?? getopt returned character code 0%o ??\n", c);
            ret = -1;
        }
    }
    
    if (0 == ret && optind < argc) 
    {
        ret = 0;
        playbackFiles->szFirstFile = malloc(IPTV_MAX_FILE_PATH);
        playbackFiles->szFirstFile[0] = '\0';
        if(NULL == strstr(argv[optind], "://"))
        {
            strcpy(playbackFiles->szFirstFile, "file://");
        }
        strcat(playbackFiles->szFirstFile, argv[optind]);
        playbackFiles->szFirstFile[IPTV_MAX_FILE_PATH] = '\0';
        map_inter_file_path(playbackFiles->szFirstFile);
        printf("file: [%s]\n", playbackFiles->szFirstFile);
        ++optind;
    }
    else
    {
        ret = -1;
    }
    return ret;
}

int main(int argc, char* argv[]) 
{
    pthread_t termThread;
    int isTermThreadStarted = 0;
    
    int audioTrackIdx = -1;
    int subtitleTrackIdx = -1;
    
    uint32_t linuxDvbBufferSizeMB = 0; 
    
    char argvBuff[256];
    memset(argvBuff, '\0', sizeof(argvBuff));
    int commandRetVal = -1;

    /* inform client that we can handle additional commands */
    E2iSendMsg("{\"EPLAYER3_EXTENDED\":{\"version\":%d}}\n", 68);

    PlayFiles_t playbackFiles;
    memset(&playbackFiles, 0x00, sizeof(playbackFiles));

    if (0 != ParseParams(argc, argv, &playbackFiles, &audioTrackIdx, &subtitleTrackIdx, &linuxDvbBufferSizeMB))
    {
        printf("Usage: exteplayer3 filePath [-u user-agent] [-c cookies] [-h headers] [-p prio] [-a] [-d] [-w] [-l] [-s] [-i] [-t audioTrackId] [-9 subtitleTrackId] [-x separateAudioUri] plabackUri\n");
        printf("[-b size] Linux DVB output buffer size in MB\n");
        printf("[-a 0|1|2|3] AAC software decoding - 1 bit - AAC ADTS, 2 - bit AAC LATM\n");
        printf("[-e] EAC3 software decoding\n");
        printf("[-3] AC3 software decoding\n");
        printf("[-d] DTS software decoding\n");
        printf("[-m] MP3 software decoding\n");
        printf("[-A 0|1] disable|enable AMR software decoding\n");
        printf("[-V 0|1] disable|enable VORBIS software decoding\n");
        printf("[-U 0|1] disable|enable AMR software decoding\n");
        printf("[-w] WMA2, WMA/PRO software decoding\n");
        printf("[-l] software decoder use LPCM for injection (otherwise wav PCM will be used)\n");
        printf("[-s] software decoding as stereo [downmix]\n");
#ifdef HAVE_FLV2MPEG4_CONVERTER
        printf("[-4 0|1] - disable/enable flv2mpeg4 converter\n");
#endif
        printf("[-i] play in infinity loop\n");
        printf("[-v] switch to live TS stream mode\n");
        printf("[-n 0|1|2] rtmp force protocol implementation auto(0) native/ffmpeg(1) or librtmp(2)\n");
        printf("[-o 0|1] set progressive download\n");
        printf("[-p value] nice value\n");
        printf("[-P value] select Program ID from multi-service stream\n");
        printf("[-t id] audio track ID switched on at start\n");
        printf("[-9 id] subtitle track ID switched on at start\n");
        printf("[-h headers] set custom HTTP headers \"Name: value\\r\\nName: value\\r\\n\"\n");
        printf("[-u user-agent] set custom http User-Agent header\n");
        printf("[-c cookies] set cookies - not working at now, please use -h instead\n");
        printf("[-x separateAudioUri]\n");
        printf("[-0 idx] video MPEG-DASH representation index\n");
        printf("[-1 idx] audio MPEG-DASH representation index\n");
        printf("[-f ffopt=ffval] any other ffmpeg option\n");
        printf("[-F path to additional file with moov atom data (used for mp4 playback in progressive download mode)\n");
        printf("[-O moov atom offset in the original file (used for mp4 playback in progressive download mode)\n");
        printf("[-S remote file size (used for mp4 playback in progressive download mode)\n");
        printf("[-G path (directory where graphic subtitles frames will be saved)\n");
        printf("[-W osd window width (width of the window used to scale graphic subtitle frame)\n");
        printf("[-H osd window height (height of the window used to scale graphic subtitle frame)\n");
        exit(1);
    }
    
    g_player = malloc(sizeof(Context_t));
    if(NULL == g_player)
    {
        printf("g_player allocate error\n");
        exit(1);
    }
    
    pthread_mutex_init(&playbackStartMtx, NULL);
    do 
    {
        int flags = 0;
        
        if (pipe(g_pfd) == -1)
            break;
        
        /* Make read and write ends of pipe nonblocking */
        if ((flags = fcntl(g_pfd[0], F_GETFL)) == -1)
            break;
        
        /* Make read end nonblocking */
        flags |= O_NONBLOCK;
        if (fcntl(g_pfd[0], F_SETFL, flags) == -1)
            break;
        
        if ((flags = fcntl(g_pfd[1], F_GETFL)) == -1)
            break;
        
        /* Make write end nonblocking */
        flags |= O_NONBLOCK;
        if (fcntl(g_pfd[1], F_SETFL, flags) == -1)
            break;
        
        if(0 == pthread_create(&termThread, NULL, TermThreadFun, NULL))
            isTermThreadStarted = 1;
    } while(0);
    
    g_player->playback    = &PlaybackHandler;
    g_player->output      = &OutputHandler;
    g_player->container   = &ContainerHandler;
    g_player->manager     = &ManagerHandler;

    // make sure to kill myself when parent dies
    prctl(PR_SET_PDEATHSIG, SIGKILL);

    SetBuffering();

    //Registrating output devices
    g_player->output->Command(g_player, OUTPUT_ADD, "audio");
    g_player->output->Command(g_player, OUTPUT_ADD, "video");
    g_player->output->Command(g_player, OUTPUT_ADD, "subtitle");
    
    //Set LINUX DVB additional write buffer size 
    if (linuxDvbBufferSizeMB)
        g_player->output->Command(g_player, OUTPUT_SET_BUFFER_SIZE, &linuxDvbBufferSizeMB);
    
    g_player->manager->video->Command(g_player, MANAGER_REGISTER_UPDATED_TRACK_INFO, UpdateVideoTrack);
    if (strncmp(playbackFiles.szFirstFile, "rtmp", 4) && strncmp(playbackFiles.szFirstFile, "ffrtmp", 4))
    {
        g_player->playback->noprobe = 1;
    }

    commandRetVal = g_player->playback->Command(g_player, PLAYBACK_OPEN, &playbackFiles);
    E2iSendMsg("{\"PLAYBACK_OPEN\":{\"OutputName\":\"%s\", \"file\":\"%s\", \"sts\":%d}}\n", g_player->output->Name, playbackFiles.szFirstFile, commandRetVal);
    if(commandRetVal < 0)
    {
        if(NULL != g_player)
        {
            free(g_player);
        }
        return 10;
    }
    
    {
        pthread_mutex_lock(&playbackStartMtx);
        isPlaybackStarted = 1;
        pthread_mutex_unlock(&playbackStartMtx);
        
        commandRetVal = g_player->output->Command(g_player, OUTPUT_OPEN, NULL);
        E2iSendMsg("{\"OUTPUT_OPEN\":{\"sts\":%d}}\n", commandRetVal);
        commandRetVal = g_player->playback->Command(g_player, PLAYBACK_PLAY, NULL);
        E2iSendMsg("{\"PLAYBACK_PLAY\":{\"sts\":%d}}\n", commandRetVal);
        
        if (g_player->playback->isPlaying)
        {
            PlaybackDieNowRegisterCallback(TerminateWakeUp);

            HandleTracks(g_player->manager->video, (PlaybackCmd_t)-1, "vc");
            HandleTracks(g_player->manager->audio, (PlaybackCmd_t)-1, "al");
            if (audioTrackIdx >= 0)
            {
                static char cmd[128] = ""; // static to not allocate on stack
                sprintf(cmd, "ai%d\n", audioTrackIdx);
                commandRetVal = HandleTracks(g_player->manager->audio, PLAYBACK_SWITCH_AUDIO, cmd);
            }
            HandleTracks(g_player->manager->audio, (PlaybackCmd_t)-1, "ac");
            
            
            HandleTracks(g_player->manager->subtitle, (PlaybackCmd_t)-1, "sl");
            if (subtitleTrackIdx >= 0)
            {
                static char cmd[128] = ""; // static to not allocate on stack
                sprintf(cmd, "si%d\n", subtitleTrackIdx);
                commandRetVal = HandleTracks(g_player->manager->subtitle, PLAYBACK_SWITCH_SUBTITLE, cmd);
            }
            HandleTracks(g_player->manager->subtitle, (PlaybackCmd_t)-1, "sc");
        }

        while(g_player->playback->isPlaying && 0 == PlaybackDieNow(0))
        {
            /* we made fgets non blocking */
            if( NULL == fgets(argvBuff, sizeof(argvBuff)-1 , stdin) )
            {
                /* wait for data - max 1s */
                kbhit();
                continue;
            }

            if(0 == argvBuff[0])
            {
                continue;
            }
            
            switch(argvBuff[0])
            {
            case 'v':
            {
                HandleTracks(g_player->manager->video, (PlaybackCmd_t)-1, argvBuff);
            break;
            }
            case 'a': 
            {
                HandleTracks(g_player->manager->audio, PLAYBACK_SWITCH_AUDIO, argvBuff);
            break;
            }
            case 's': 
            {
                HandleTracks(g_player->manager->subtitle, PLAYBACK_SWITCH_SUBTITLE, argvBuff);
            break;
            }
            case 'q':
            {
                commandRetVal = g_player->playback->Command(g_player, PLAYBACK_STOP, NULL);
                E2iSendMsg("{\"PLAYBACK_STOP\":{\"sts\":%d}}\n", commandRetVal);
                break;
            }
            case 'c':
            {
                commandRetVal = g_player->playback->Command(g_player, PLAYBACK_CONTINUE, NULL);
                E2iSendMsg("{\"PLAYBACK_CONTINUE\":{\"sts\":%d}}\n", commandRetVal);
                break;
            }
            case 'p':
            {
                commandRetVal = g_player->playback->Command(g_player, PLAYBACK_PAUSE, NULL);
                E2iSendMsg("{\"PLAYBACK_PAUSE\":{\"sts\":%d}}\n", commandRetVal);
                break;
            }
            case 'm':
            {
                int speed = 0;
                sscanf(argvBuff+1, "%d", &speed);

                commandRetVal = g_player->playback->Command(g_player, PLAYBACK_SLOWMOTION, &speed);
                E2iSendMsg("{\"PLAYBACK_SLOWMOTION\":{\"speed\":%d, \"sts\":%d}}\n", speed, commandRetVal);
                break;
            }
            case 'o':
            {
                int flags = 0;
                if( 1 == sscanf(argvBuff+1, "%d", &flags) )
                {
                    progressive_playback_set(flags);
                    E2iSendMsg("{\"PROGRESSIVE_DOWNLOAD\":{\"flags\":%d, \"sts\":0}}\n", flags);
                }
                break;
            }
            case 'f':
            {
                int speed = 0;
                sscanf(argvBuff+1, "%d", &speed);

                commandRetVal = g_player->playback->Command(g_player, PLAYBACK_FASTFORWARD, &speed);
                E2iSendMsg("{\"PLAYBACK_FASTFORWARD\":{\"speed\":%d, \"sts\":%d}}\n", speed, commandRetVal);
                break;
            }
            case 'b': 
            {
                int speed = 0;
                sscanf(argvBuff+1, "%d", &speed);

                commandRetVal = g_player->playback->Command(g_player, PLAYBACK_FASTBACKWARD, &speed);
                E2iSendMsg("{\"PLAYBACK_FASTBACKWARD\":{\"speed\":%d, \"sts\":%d}}\n", speed, commandRetVal);
                break;
            }
            case 'g':
            {
                int32_t gotoPos = 0;
                int64_t length = 0;
                int32_t lengthInt = 0;
                int64_t sec = 0;
                int8_t force = ('f' == argvBuff[1]) ? 1 : 0; // f - force, c - check
                
                sscanf(argvBuff+2, "%d", &gotoPos);
                if(0 <= gotoPos || force)
                {
                    commandRetVal = g_player->playback->Command(g_player, PLAYBACK_LENGTH, (void*)&length);
                    E2iSendMsg("{\"PLAYBACK_LENGTH\":{\"length\":%"PRId64", \"sts\":%d}}\n", length, commandRetVal);

                    lengthInt = (int32_t)length;
                    if(10 <= lengthInt || force)
                    {
                        sec = gotoPos;
                        if(!force && gotoPos >= lengthInt)
                        {
                            sec = lengthInt - 10;
                        }
                        
                        commandRetVal = g_player->playback->Command(g_player, PLAYBACK_SEEK_ABS, (void*)&sec);
                        E2iSendMsg("{\"PLAYBACK_SEEK_ABS\":{\"sec\":%"PRId64", \"sts\":%d}}\n", sec, commandRetVal);
                    }
                }
                break;
            }
            case 'k': 
            {
                int32_t seek = 0;
                int64_t length = 0;
                int32_t lengthInt = 0;
                int64_t sec = 0;
                int64_t pts = 0;
                int32_t CurrentSec = 0;
                int8_t force = ('f' == argvBuff[1]) ? 1 : 0; // f - force, c - check
                
                sscanf(argvBuff+2, "%d", &seek);
                
                commandRetVal = g_player->playback->Command(g_player, PLAYBACK_PTS, &pts);
                CurrentSec = (int32_t)(pts / 90000);
                if (0 == commandRetVal)
                {
                    E2iSendMsg("{\"J\":{\"ms\":%"PRId64"}}\n", pts / 90);
                }
                if(0 == commandRetVal || force)
                {                    
                    commandRetVal = g_player->playback->Command(g_player, PLAYBACK_LENGTH, (void*)&length);
                    E2iSendMsg("{\"PLAYBACK_LENGTH\":{\"length\":%"PRId64", \"sts\":%d}}\n", length, commandRetVal);
                    
                    lengthInt = (int32_t)length;
                    if(10 <= lengthInt || force )
                    {
                        int32_t ergSec = CurrentSec + seek;
                        if(!force && 0 > ergSec)
                        {
                            sec = CurrentSec * -1; // jump to start position
                        }
                        else if(!force && ergSec >= lengthInt)
                        {
                            sec = (lengthInt - CurrentSec) - 5;
                            if(0 < sec)
                            {
                                sec = 0; // no jump we are at the end
                            }
                        }
                        else
                        {
                            sec = seek;
                        }
                    }
                    commandRetVal = g_player->playback->Command(g_player, PLAYBACK_SEEK, (void*)&sec);
                    E2iSendMsg("{\"PLAYBACK_SEEK\":{\"sec\":%"PRId64", \"sts\":%d}}\n", sec, commandRetVal);
                }
                break;
            }
            case 'l': 
            {
                int64_t length = 0;
                commandRetVal = g_player->playback->Command(g_player, PLAYBACK_LENGTH, (void*)&length);
                E2iSendMsg("{\"PLAYBACK_LENGTH\":{\"length\":%"PRId64", \"sts\":%d}}\n", length, commandRetVal);
                break;
            }
            case 'j': 
            {
                int64_t pts = 0;
                commandRetVal = g_player->playback->Command(g_player, PLAYBACK_PTS, &pts);
                if (0 == commandRetVal)
                {
                    int64_t lastPts = 0;
                    commandRetVal = 1;
                    if (g_player->container && g_player->container->selectedContainer)
                    {
                        commandRetVal = g_player->container->selectedContainer->Command(g_player->container, CONTAINER_LAST_PTS, &lastPts);
                    }
                    
                    if (0 == commandRetVal && lastPts != INVALID_PTS_VALUE)
                    {
                        E2iSendMsg("{\"J\":{\"ms\":%"PRId64",\"lms\":%"PRId64"}}\n", pts / 90, lastPts / 90);
                    }
                    else
                    {
                        E2iSendMsg("{\"J\":{\"ms\":%"PRId64"}}\n", pts / 90);
                    }
                }
                break;
            }
            case 'i':
            {
                PlaybackHandler_t *ptrP = g_player->playback;
                if(ptrP)
                {
                    E2iSendMsg("{\"PLAYBACK_INFO\":{ \"isPlaying\":%s, \"isPaused\":%s, \"isForwarding\":%s, \"isSeeking\":%s, \"isCreationPhase\":%s,", \
                    DUMP_BOOL(ptrP->isPlaying), DUMP_BOOL(ptrP->isPaused), DUMP_BOOL(ptrP->isForwarding), DUMP_BOOL(ptrP->isSeeking), DUMP_BOOL(ptrP->isCreationPhase) );
                    E2iSendMsg("\"BackWard\":%d, \"SlowMotion\":%d, \"Speed\":%d, \"AVSync\":%d,", ptrP->BackWard, ptrP->SlowMotion, ptrP->Speed, ptrP->AVSync);
                    E2iSendMsg(" \"isVideo\":%s, \"isAudio\":%s, \"isSubtitle\":%s, \"isDvbSubtitle\":%s, \"isTeletext\":%s, \"mayWriteToFramebuffer\":%s, \"abortRequested\":%s }}\n", \
                    DUMP_BOOL(ptrP->isVideo), DUMP_BOOL(ptrP->isAudio), DUMP_BOOL(0), DUMP_BOOL(0), DUMP_BOOL(0), DUMP_BOOL(0), DUMP_BOOL(ptrP->abortRequested) );
                }
                
                break;
            }
            case 'n':
            {
                uint8_t loop = 0;
                if( '1' == argvBuff[1] || '0' == argvBuff[1] )
                {
                    PlaybackHandler_t *ptrP = g_player->playback;
                    if(ptrP)
                    {
                        ptrP->isLoopMode = '1' == argvBuff[1] ? 1 : 0;
                        E2iSendMsg("{\"N\":{ \"isLoop\":%s }}\n", DUMP_BOOL(ptrP->isLoopMode));
                    }
                }
                break;
            }
            
            default: 
            {
                break;
            }
            }
        }

        g_player->output->Command(g_player, OUTPUT_CLOSE, NULL);
    }
    
    if(NULL != g_player)
    {
        free(g_player);
    }
    
    if (isTermThreadStarted && 1 == write(g_pfd[1], "x", 1))
    {
        pthread_join(termThread, NULL);
    }
    
    pthread_mutex_destroy(&playbackStartMtx);
    
    close(g_pfd[0]);
    close(g_pfd[1]);
    
    exit(0);
}
