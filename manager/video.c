/*
 * video manager handling.
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
#include <string.h>

#include "manager.h"
#include "common.h"
#include "debug.h"

/* ***************************** */
/* Makros/Constants              */
/* ***************************** */
#define TRACKWRAP 4
/* Error Constants */
#define cERR_VIDEO_MGR_NO_ERROR        0
#define cERR_VIDEO_MGR_ERROR          -1

/* ***************************** */
/* Types                         */
/* ***************************** */

/* ***************************** */
/* Varaibles                     */
/* ***************************** */
static Track_t *Tracks = NULL;
static int TrackCount = 0;
static int CurrentTrack = 0; //TRACK[0] as default.

static void (* updatedTrackInfoFnc)(void) = NULL;

/* ***************************** */
/* Prototypes                    */
/* ***************************** */

/* ***************************** */
/* Functions                     */
/* ***************************** */

static int ManagerAdd(Context_t  *context, Track_t track) {
    video_mgr_printf(10, "\n");

    if (Tracks == NULL) 
    {
        Tracks = malloc(sizeof(Track_t) * TRACKWRAP);
        int i;
        for (i = 0; i < TRACKWRAP; i++)
        {
            Tracks[i].Id = -1;
        }
    }

    if (Tracks == NULL)
    {
        video_mgr_err("malloc failed\n");
        return cERR_VIDEO_MGR_ERROR;
    }

    int i;
    for (i = 0; i < TRACKWRAP; i++) 
    {
        if (Tracks[i].Id == track.Id) 
        {
            Tracks[i].pending = 0;
            return cERR_VIDEO_MGR_NO_ERROR;
        }
    }

    if (TrackCount < TRACKWRAP)
    {
        copyTrack(&Tracks[TrackCount], &track);
        TrackCount++;
    } 
    else 
    {
        video_mgr_err("TrackCount out if range %d - %d\n", TrackCount, TRACKWRAP);
        return cERR_VIDEO_MGR_ERROR;
    }

    if (TrackCount > 0)
    {
        context->playback->isVideo = 1;
    }

    video_mgr_printf(10, "\n");
    return cERR_VIDEO_MGR_NO_ERROR;
}

static char ** ManagerList(Context_t  *context __attribute__((unused))) 
{
    int i = 0, j = 0;
    char ** tracklist = NULL;

    video_mgr_printf(10, "\n");

    if (Tracks != NULL) 
    {

        tracklist = malloc(sizeof(char *) * ((TrackCount*2) + 1));

        if (tracklist == NULL)
        {
            video_mgr_err("malloc failed\n");
            return NULL;
        }

        for (i = 0, j = 0; i < TrackCount; i++, j+=2) 
        {
            if (Tracks[i].pending)
            {
                continue;
            }
            size_t len = strlen(Tracks[i].Name) + 20;
            char tmp[len];
            snprintf(tmp, len, "%d %s\n", Tracks[i].Id, Tracks[i].Name);
            tracklist[j]    = strdup(tmp);
            tracklist[j+1]  = strdup(Tracks[i].Encoding);
        }
        tracklist[j] = NULL;
    }

    video_mgr_printf(10, "return %p (%d - %d)\n", tracklist, j, TrackCount);
    return tracklist;
}

static int ManagerDel(Context_t * context)
{
    int i = 0;

    video_mgr_printf(10, "\n");

    if(Tracks != NULL) 
    {
        for (i = 0; i < TrackCount; i++) 
        {
            freeTrack(&Tracks[i]);
        }
        free(Tracks);
        Tracks = NULL;
    } 
    else
    {
        video_mgr_err("nothing to delete!\n");
        return cERR_VIDEO_MGR_ERROR;
    }

    TrackCount = 0;
    CurrentTrack = 0;
    context->playback->isVideo = 0;

    video_mgr_printf(10, "return no error\n");
    return cERR_VIDEO_MGR_NO_ERROR;
}

static int Command(void  *_context, ManagerCmd_t command, void * argument) {
    Context_t  *context = (Context_t*) _context;
    int ret = cERR_VIDEO_MGR_NO_ERROR;

    video_mgr_printf(10, "\n");

    switch(command) 
    {
    case MANAGER_ADD:
    {
        Track_t * track = argument;
        ret = ManagerAdd(context, *track);
        break;
    }
    case MANAGER_LIST: 
    {
        container_ffmpeg_update_tracks(context, context->playback->uri, 0);
        *((char***)argument) = (char **)ManagerList(context);
        break;
    }
    case MANAGER_GET: 
    {
        if ((TrackCount > 0) && (CurrentTrack >=0))
        {
            *((int*)argument) = (int)Tracks[CurrentTrack].Id;
        }
        else
        {
            *((int*)argument) = (int)-1;
        }
        break;
    }
    case MANAGER_GET_TRACK_DESC: 
    {
        if ((TrackCount > 0) && (CurrentTrack >=0))
        {
            TrackDescription_t *track =  malloc(sizeof(TrackDescription_t));
            *((TrackDescription_t**)argument) = track;
            if (track)
            {
                memset(track, 0, sizeof(TrackDescription_t));
                track->Id                   = Tracks[CurrentTrack].Id;
                track->Name                 = strdup(Tracks[CurrentTrack].Name);
                track->Encoding             = strdup(Tracks[CurrentTrack].Encoding);
                track->frame_rate           = Tracks[CurrentTrack].frame_rate;
                track->width                = Tracks[CurrentTrack].width;
                track->height               = Tracks[CurrentTrack].height;
                track->aspect_ratio_num     = Tracks[CurrentTrack].aspect_ratio_num;
                track->aspect_ratio_den     = Tracks[CurrentTrack].aspect_ratio_den;
                context->output->video->Command(context, OUTPUT_GET_PROGRESSIVE, &(track->progressive));
            }
        }
        else
        {
            *((TrackDescription_t**)argument) = NULL;
        }
    break;
    }
    case MANAGER_GET_TRACK: 
    {
        video_mgr_printf(20, "MANAGER_GET_TRACK\n");

        if ((TrackCount > 0) && (CurrentTrack >=0))
        {
            *((Track_t**)argument) = (Track_t*) &Tracks[CurrentTrack];
        }
        else
        {
            *((Track_t**)argument) = NULL;
        }
        break;
    }
    case MANAGER_GETENCODING:
    {
        if ((TrackCount > 0) && (CurrentTrack >=0))
        {
            *((char**)argument) = (char *)strdup(Tracks[CurrentTrack].Encoding);
        }
        else
        {
            *((char**)argument) = (char *)strdup("");
        }
        break;
    }
    case MANAGER_GETNAME:
    {
        if ((TrackCount > 0) && (CurrentTrack >=0))
        {
            *((char**)argument) = (char *)strdup(Tracks[CurrentTrack].Name);
        }
        else
        {
            *((char**)argument) = (char *)strdup("");
        }
        break;
    }
    case MANAGER_SET:
    {
        int i;
        for (i = 0; i < TrackCount; i++)
        {
            if (Tracks[i].Id == *((int*)argument))
            {
                CurrentTrack = i;
                break;
            }
        }

        if (i == TrackCount)
        {
            video_mgr_err("track id %d unknown\n", *((int*)argument));
            ret = cERR_VIDEO_MGR_ERROR;
        }
        break;
    }
    case MANAGER_DEL:
    {
        ret = ManagerDel(context);
        break;
    }
    case MANAGER_INIT_UPDATE:
    {
        int i;
        for (i = 0; i < TrackCount; i++)
        {
            Tracks[i].pending = 1;
        }
        break;
    }
    case MANAGER_UPDATED_TRACK_INFO:
    {
        if (updatedTrackInfoFnc != NULL)
            updatedTrackInfoFnc();
        break;
    }
    case MANAGER_REGISTER_UPDATED_TRACK_INFO:
    {
        updatedTrackInfoFnc = (void (* )(void))argument;
        break;
    }
    default:
        video_mgr_err("ContainerCmd %d not supported!\n", command);
        ret = cERR_VIDEO_MGR_ERROR;
        break;
    }

    video_mgr_printf(10, "returning %d\n", ret);
    return ret;
}


struct Manager_s VideoManager = {
    "Video",
    &Command,
    NULL
};
