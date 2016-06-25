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

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/mman.h>

#include "common.h"

#define DUMP_BOOL(x) 0 == x ? "false"  : "true"
#define IPTV_MAX_FILE_PATH 1024

extern int ffmpeg_av_dict_set(const char *key, const char *value, int flags);
extern void aac_software_decoder_set(int val);
extern void dts_software_decoder_set(int32_t val);
extern void stereo_software_decoder_set(int32_t val);
extern void insert_pcm_as_lpcm_set(int32_t val);
extern void pcm_resampling_set(int32_t val);
extern void wma_software_decoder_set(int32_t val);
extern void ac3_software_decoder_set(int32_t val);
extern void eac3_software_decoder_set(int32_t val);
extern void progressive_download_set(int32_t val);



extern OutputHandler_t         OutputHandler;
extern PlaybackHandler_t       PlaybackHandler;
extern ContainerHandler_t      ContainerHandler;
extern ManagerHandler_t        ManagerHandler;

static Context_t *g_player = NULL;

static void map_inter_file_path(char *filename)
{
    if(strstr(filename, "iptv://") == filename)
    {
        FILE *f = fopen(filename + 7, "r");
        if(NULL != f)
        {
            size_t num = fread(filename, 1, IPTV_MAX_FILE_PATH-1, f);
            fclose(f);
            if(num > 0 && filename[num-1] == '\n')
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
        fd_set read_fd;

        tv.tv_sec=1;
        tv.tv_usec=0;

        FD_ZERO(&read_fd);
        FD_SET(0,&read_fd);

        if(-1 == select(1, &read_fd, NULL, NULL, &tv))
        {
            return 0;
        }

        if(FD_ISSET(0,&read_fd))
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
                fprintf(stderr, "{\"%c_%c\": [", argvBuff[0], argvBuff[1]);
                for (i = 0; TrackList[i].Id >= 0; ++i) 
                {
                    if(0 < i)
                    {
                        fprintf(stderr, ", ");
                    }
                    fprintf(stderr, "{\"id\":%d,\"e\":\"%s\",\"n\":\"%s\"}", TrackList[i].Id , TrackList[i].Encoding, TrackList[i].Name);
                    free(TrackList[i].Encoding);
                    free(TrackList[i].Name);
                }
                fprintf(stderr, "]}\n");
                free(TrackList);
            }
            else
            {
                // not tracks 
                fprintf(stderr, "{\"%c_%c\": []}\n", argvBuff[0], argvBuff[1]);
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
                    fprintf(stderr, "{\"%c_%c\":{\"id\":%d,\"e\":\"%s\",\"n\":\"%s\"}}\n", argvBuff[0], argvBuff[1], track->Id , track->Encoding, track->Name);
                }
                else // video
                {
                    fprintf(stderr, "{\"%c_%c\":{\"id\":%d,\"e\":\"%s\",\"n\":\"%s\",\"w\":%d,\"h\":%d,\"f\":%u,\"p\":%d}}\n", argvBuff[0], argvBuff[1], track->Id , track->Encoding, track->Name, track->width, track->height, track->frame_rate, track->progressive);
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
                    fprintf(stderr, "{\"%c_%c\":{\"id\":%d,\"e\":\"%s\",\"n\":\"%s\"}}\n", argvBuff[0], argvBuff[1], -1, "", "");
                }
                else // video
                {
                    fprintf(stderr, "{\"%c_%c\":{\"id\":%d,\"e\":\"%s\",\"n\":\"%s\",\"w\":%d,\"h\":%d,\"f\":%u,\"p\":%d}}\n", argvBuff[0], argvBuff[1], -1, "", "", -1, -1, 0, -1);
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
                    fprintf(stderr, "{\"%c_%c\":{\"id\":%d,\"sts\":%d}}\n", argvBuff[0], 's', id, commandRetVal);
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

static int ParseParams(int argc,char* argv[], char *file, char *audioFile, int *pAudioTrackIdx, int *subtitleTrackIdx)
{   
    int ret = 0;
    int c;
    int digit_optind = 0;
    int aopt = 0, bopt = 0;
    char *copt = 0, *dopt = 0;
    while ( (c = getopt(argc, argv, "wae3dlsrix:u:c:h:o:p:t:9:")) != -1) 
    {
        switch (c) 
        {
        case 'a':
            printf("Software decoder will be used for AAC codec\n");
            aac_software_decoder_set(1);
            break;
        case 'e':
            printf("Software decoder will be used for EAC3 codec\n");
            eac3_software_decoder_set(1);
            break;
        case '3':
            printf("Software decoder will be used for AC3 codec\n");
            ac3_software_decoder_set(1);
            break;
        case 'd':
            printf("Software decoder will be used for DTS codec\n");
            dts_software_decoder_set(1);
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
            progressive_download_set(atoi(optarg));
            break;
        case 'p':
            SetNice(atoi(optarg));
            break;
        case 't':
            *pAudioTrackIdx = atoi(optarg);
            break;
        case '9':
            *subtitleTrackIdx = atoi(optarg);
            break;
        case 'x':
            strncpy(audioFile, optarg, IPTV_MAX_FILE_PATH-1);
            map_inter_file_path(audioFile);
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
        default:
            printf ("?? getopt returned character code 0%o ??\n", c);
            ret = -1;
        }
    }
    
    if (0 == ret && optind < argc) 
    {
        ret = 0;
        
        if(NULL == strstr(argv[optind], "://"))
        {
            strcpy(file, "file://");
        }
        strcat(file, argv[optind]);
        map_inter_file_path(file);
        printf("file: [%s]\n", file);
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
    char file[IPTV_MAX_FILE_PATH];
    memset(file, '\0', sizeof(file));
    
    char audioFile[IPTV_MAX_FILE_PATH];
    memset(audioFile, '\0', sizeof(audioFile));
    
    int audioTrackIdx = -1;
    int subtitleTrackIdx = -1;
    
    char argvBuff[256];
    memset(argvBuff, '\0', sizeof(argvBuff));
    int commandRetVal = -1;
    /* inform client that we can handle additional commands */
    fprintf(stderr, "{\"EPLAYER3_EXTENDED\":{\"version\":%d}}\n", 27);

    if (0 != ParseParams(argc, argv, file, audioFile, &audioTrackIdx, &subtitleTrackIdx))
    {
        printf("Usage: exteplayer3 filePath [-u user-agent] [-c cookies] [-h headers] [-p prio] [-a] [-d] [-w] [-l] [-s] [-i] [-t audioTrackId] [-9 subtitleTrackId] [-x separateAudioUri] plabackUri\n");
        printf("[-a] AAC software decoding\n");
        printf("[-e] EAC3 software decoding\n");
        printf("[-3] AC3 software decoding\n");
        printf("[-d] DTS software decoding\n");
        printf("[-w] WMA1, WMA2, WMA/PRO software decoding\n");
        printf("[-l] software decoder use LPCM for injection (otherwise wav PCM will be used)\n");
        printf("[-s] software decoding as stereo [downmix]\n");
        printf("[-i] play in infinity loop\n");
        printf("[-o 0|1] set progressive download\n");
        printf("[-p value] nice value\n");
        printf("[-t id] audio track ID switched on at start\n");
        printf("[-9 id] subtitle track ID switched on at start\n");
        printf("[-h headers] set custom HTTP headers \"Name: value\\r\\nName: value\\r\\n\"\n");
        printf("[-u user-agent] set custom http User-Agent header\n");
        printf("[-c cookies] set cookies - not working at now, please use -h instead\n");
        printf("[-x separateAudioUri]\n");
        
        exit(1);
    }
    
    g_player = malloc(sizeof(Context_t));
    if(NULL == g_player)
    {
        printf("g_player allocate error\n");
        exit(1);
    }

    g_player->playback    = &PlaybackHandler;
    g_player->output      = &OutputHandler;
    g_player->container   = &ContainerHandler;
    g_player->manager     = &ManagerHandler;

    SetBuffering();
    
    //Registrating output devices
    g_player->output->Command(g_player, OUTPUT_ADD, "audio");
    g_player->output->Command(g_player, OUTPUT_ADD, "video");
    g_player->output->Command(g_player, OUTPUT_ADD, "subtitle");

    g_player->manager->video->Command(g_player, MANAGER_REGISTER_UPDATED_TRACK_INFO, UpdateVideoTrack);
    g_player->playback->noprobe = 1;
    
    PlayFiles_t playbackFiles = {file, NULL};
    if('\0' != audioFile[0])
    {
        playbackFiles.szSecondFile = audioFile;
    }
    
    commandRetVal = g_player->playback->Command(g_player, PLAYBACK_OPEN, &playbackFiles);
    fprintf(stderr, "{\"PLAYBACK_OPEN\":{\"OutputName\":\"%s\", \"file\":\"%s\", \"sts\":%d}}\n", g_player->output->Name, file, commandRetVal);
    if(commandRetVal < 0)
    {
        if(NULL != g_player)
        {
            free(g_player);
        }
        return 10;
    }
    
    {
        commandRetVal = g_player->output->Command(g_player, OUTPUT_OPEN, NULL);
        fprintf(stderr, "{\"OUTPUT_OPEN\":{\"sts\":%d}}\n", commandRetVal);
        commandRetVal = g_player->playback->Command(g_player, PLAYBACK_PLAY, NULL);
        fprintf(stderr, "{\"PLAYBACK_PLAY\":{\"sts\":%d}}\n", commandRetVal);
        
        if (g_player->playback->isPlaying)
        {
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

        while(g_player->playback->isPlaying)
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
                fprintf(stderr, "{\"PLAYBACK_STOP\":{\"sts\":%d}}\n", commandRetVal);
                break;
            }
            case 'c':
            {
                commandRetVal = g_player->playback->Command(g_player, PLAYBACK_CONTINUE, NULL);
                fprintf(stderr, "{\"PLAYBACK_CONTINUE\":{\"sts\":%d}}\n", commandRetVal);
                break;
            }
            case 'p':
            {
                commandRetVal = g_player->playback->Command(g_player, PLAYBACK_PAUSE, NULL);
                fprintf(stderr, "{\"PLAYBACK_PAUSE\":{\"sts\":%d}}\n", commandRetVal);
                break;
            }
            case 'm':
            {
                int speed = 0;
                sscanf(argvBuff+1, "%d", &speed);

                commandRetVal = g_player->playback->Command(g_player, PLAYBACK_SLOWMOTION, &speed);
                fprintf(stderr, "{\"PLAYBACK_SLOWMOTION\":{\"speed\":%d, \"sts\":%d}}\n", speed, commandRetVal);
                break;
            }
            case 'o':
            {
                int flags = 0;
                if( 1 == sscanf(argvBuff+1, "%d", &flags) )
                {
                    progressive_download_set(flags);
                    fprintf(stderr, "{\"PROGRESSIVE_DOWNLOAD\":{\"flags\":%d, \"sts\":0}}\n", flags);
                }
                break;
            }
            case 'f':
            {
                int speed = 0;
                sscanf(argvBuff+1, "%d", &speed);

                commandRetVal = g_player->playback->Command(g_player, PLAYBACK_FASTFORWARD, &speed);
                fprintf(stderr, "{\"PLAYBACK_FASTFORWARD\":{\"speed\":%d, \"sts\":%d}}\n", speed, commandRetVal);
                break;
            }
            case 'b': 
            {
                int speed = 0;
                sscanf(argvBuff+1, "%d", &speed);

                commandRetVal = g_player->playback->Command(g_player, PLAYBACK_FASTBACKWARD, &speed);
                fprintf(stderr, "{\"PLAYBACK_FASTBACKWARD\":{\"speed\":%d, \"sts\":%d}}\n", speed, commandRetVal);
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
                    fprintf(stderr, "{\"PLAYBACK_LENGTH\":{\"length\":%lld, \"sts\":%d}}\n", length, commandRetVal);

                    lengthInt = (int32_t)length;
                    if(10 <= lengthInt || force)
                    {
                        sec = gotoPos;
                        if(!force && gotoPos >= lengthInt)
                        {
                            sec = lengthInt - 10;
                        }
                        
                        commandRetVal = g_player->playback->Command(g_player, PLAYBACK_SEEK_ABS, (void*)&sec);
                        fprintf(stderr, "{\"PLAYBACK_SEEK_ABS\":{\"sec\":%lld, \"sts\":%d}}\n", sec, commandRetVal);
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
                    fprintf(stderr, "{\"J\":{\"ms\":%lld}}\n", pts / 90, commandRetVal);
                }
                if(0 == commandRetVal || force)
                {                    
                    commandRetVal = g_player->playback->Command(g_player, PLAYBACK_LENGTH, (void*)&length);
                    fprintf(stderr, "{\"PLAYBACK_LENGTH\":{\"length\":%lld, \"sts\":%d}}\n", length, commandRetVal);
                    
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
                    fprintf(stderr, "{\"PLAYBACK_SEEK\":{\"sec\":%lld, \"sts\":%d}}\n", sec, commandRetVal);
                }
                break;
            }
            case 'l': 
            {
                int64_t length = 0;
                commandRetVal = g_player->playback->Command(g_player, PLAYBACK_LENGTH, (void*)&length);
                fprintf(stderr, "{\"PLAYBACK_LENGTH\":{\"length\":%lld, \"sts\":%d}}\n", length, commandRetVal);
                break;
            }
            case 'j': 
            {
                int64_t pts = 0;
                commandRetVal = g_player->playback->Command(g_player, PLAYBACK_PTS, &pts);
                if (0 == commandRetVal)
                {
                    fprintf(stderr, "{\"J\":{\"ms\":%lld}}\n", pts / 90, commandRetVal);
                }
                break;
            }
            case 'i':
            {
                PlaybackHandler_t *ptrP = g_player->playback;
                if(ptrP)
                {
                    fprintf(stderr, "{\"PLAYBACK_INFO\":{ \"isPlaying\":%s, \"isPaused\":%s, \"isForwarding\":%s, \"isSeeking\":%s, \"isCreationPhase\":%s,", \
                    DUMP_BOOL(ptrP->isPlaying), DUMP_BOOL(ptrP->isPaused), DUMP_BOOL(ptrP->isForwarding), DUMP_BOOL(ptrP->isSeeking), DUMP_BOOL(ptrP->isCreationPhase) );
                    fprintf(stderr, "\"BackWard\":%d, \"SlowMotion\":%d, \"Speed\":%d, \"AVSync\":%d,", ptrP->BackWard, ptrP->SlowMotion, ptrP->Speed, ptrP->AVSync);
                    fprintf(stderr, " \"isVideo\":%s, \"isAudio\":%s, \"isSubtitle\":%s, \"isDvbSubtitle\":%s, \"isTeletext\":%s, \"mayWriteToFramebuffer\":%s, \"abortRequested\":%s }}\n", \
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
                        fprintf(stderr, "{\"N\":{ \"isLoop\":%s }}\n", DUMP_BOOL(ptrP->isLoopMode));
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

    //printOutputCapabilities();

    exit(0);
}
