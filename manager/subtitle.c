/*
 * subtitle manager handling.
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
#include <assert.h>

#include "manager.h"
#include "common.h"
#include "debug.h"

/* ***************************** */
/* Makros/Constants              */
/* ***************************** */
#define TRACKWRAP 10

/* Error Constants */
#define cERR_SUBTITLE_MGR_NO_ERROR        0
#define cERR_SUBTITLE_MGR_ERROR          -1

static const char FILENAME[] = __FILE__;

/* ***************************** */
/* Types                         */
/* ***************************** */

/* ***************************** */
/* Varaibles                     */
/* ***************************** */

static Track_t * Tracks = NULL;
static int TrackSlotCount = 0;
static int TrackCount = 0;
static int CurrentTrack = -1; //no as default.

/* ***************************** */
/* Prototypes                    */
/* ***************************** */

/* ***************************** */
/* Functions                     */
/* ***************************** */

static int ManagerAdd(Context_t *context, Track_t track) 
{
    uint32_t i = 0;
    subtitle_mgr_printf(10, "%s::%s %s %s %d\n", FILENAME, __FUNCTION__, track.Name, track.Encoding, track.Id);

    if (TrackCount == TrackSlotCount)
    {
        static Track_t *t;
        t = realloc(Tracks, (TrackSlotCount + TRACKWRAP) * sizeof(Track_t));
        if (t)
        {
            Tracks = t;
            TrackSlotCount += TRACKWRAP;
            for (i = TrackCount; i < TrackSlotCount; ++i)
            {
                Tracks[i].Id = -1;
            }
        }
        else
        {
            subtitle_mgr_err("%s:%s realloc failed\n", FILENAME, __FUNCTION__);
            return cERR_SUBTITLE_MGR_ERROR;
        }
    }

    if (Tracks == NULL)
    {
        subtitle_mgr_err("%s:%s malloc failed\n", FILENAME, __FUNCTION__);
        return cERR_SUBTITLE_MGR_ERROR;
    }

    
    for (i = 0; i < TrackSlotCount; ++i) 
    {
        if (Tracks[i].Id == track.Id)
        {
            Tracks[i].pending = 0;
            return cERR_SUBTITLE_MGR_NO_ERROR;
        }
    }

    if (TrackCount < TrackSlotCount) 
    {
        copyTrack(&Tracks[TrackCount], &track);
        TrackCount++;
    } 
    else
    {
        subtitle_mgr_err("%s:%s TrackCount out if range %d - %d\n", FILENAME, __FUNCTION__, TrackCount, TrackSlotCount);
        return cERR_SUBTITLE_MGR_ERROR;
    }

    if (TrackCount > 0)
    {
        context->playback->isSubtitle = 1;
    }

    subtitle_mgr_printf(10, "%s::%s\n", FILENAME, __FUNCTION__);

    return cERR_SUBTITLE_MGR_NO_ERROR;
}

static TrackDescription_t* ManagerList(Context_t  *context __attribute__((unused))) 
{
    int i = 0;
    TrackDescription_t *tracklist = NULL;

    subtitle_mgr_printf(10, "%s::%s\n", FILENAME, __FUNCTION__);
    if (Tracks != NULL) 
    {
        tracklist = malloc(sizeof(TrackDescription_t) *((TrackCount) + 1));
        if (tracklist == NULL)
        {
            subtitle_mgr_err("%s:%s malloc failed\n", FILENAME, __FUNCTION__);
            return NULL;
        }
        
        int j = 0;
        for (i = 0; i < TrackCount; ++i) 
        {
            if (Tracks[i].pending || Tracks[i].Id < 0)
            {
                continue;
            }
            
            tracklist[j].Id = Tracks[i].Id;
            tracklist[j].Name = strdup(Tracks[i].Name);
            tracklist[j].Encoding = strdup(Tracks[i].Encoding);
            ++j;
        }
        tracklist[j].Id = -1;
    }

    return tracklist;
}

static int32_t ManagerDel(Context_t * context, int32_t onlycurrent)
{
    uint32_t i = 0;
    subtitle_mgr_printf(10, "%s::%s\n", FILENAME, __FUNCTION__);
    if(onlycurrent == 0)
    {
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
            subtitle_mgr_err("%s::%s nothing to delete!\n", FILENAME, __FUNCTION__);
            return cERR_SUBTITLE_MGR_ERROR;
        }
        TrackCount = 0;
        TrackSlotCount = 0;
        context->playback->isSubtitle = 0;
    }
    
    CurrentTrack = -1;
    subtitle_mgr_printf(10, "%s::%s return no error\n", FILENAME, __FUNCTION__);
    return cERR_SUBTITLE_MGR_NO_ERROR;
}

static int32_t Command(void  *_context, ManagerCmd_t command, void *argument)
{
    Context_t  *context = (Context_t*) _context;
    int32_t ret = cERR_SUBTITLE_MGR_NO_ERROR;

    subtitle_mgr_printf(50, "%s::%s %d\n", FILENAME, __FUNCTION__, command);

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
        if (TrackCount > 0 && CurrentTrack >= 0)
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
                track->Id       = Tracks[CurrentTrack].Id;
                track->Name     = strdup(Tracks[CurrentTrack].Name);
                track->Encoding = strdup(Tracks[CurrentTrack].Encoding);
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
        if (TrackCount > 0 && CurrentTrack >= 0)
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
        if (TrackCount > 0 && CurrentTrack >= 0)
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
        uint32_t i = 0;
        int32_t requestedTrackId = *((int*)argument);
        
        subtitle_mgr_printf(20, "%s::%s MANAGER_SET id=%d\n", FILENAME, __FUNCTION__, *((int*)argument));
        if (requestedTrackId == -1)
        {
            // track id -1 mean disable subtitle 
            CurrentTrack = -1;
            break;
        }

        for (i = 0; i < TrackCount; ++i)
        {
            if (Tracks[i].Id == requestedTrackId) 
            {
                CurrentTrack = i;
                break;
            }
        }
        
        if (i == TrackCount) 
        {
            subtitle_mgr_err("%s::%s track id %d unknown\n", FILENAME, __FUNCTION__, *((int*)argument));
            ret = cERR_SUBTITLE_MGR_ERROR;
        }
        break;
    }
    case MANAGER_DEL:
    {
        if(argument == NULL)
        {
            ret = ManagerDel(context, 0);
        }
        else
        {
            ret = ManagerDel(context, *((int*)argument));
        }
        break;
    }
    case MANAGER_INIT_UPDATE:
    {
        uint32_t i;
        for (i = 0; i < TrackCount; i++)
        {
            Tracks[i].pending = 1;
        }
        break;
    }
    default:
        subtitle_mgr_err("%s:%s: ConatinerCmd not supported!", FILENAME, __FUNCTION__);
        ret = cERR_SUBTITLE_MGR_ERROR;
        break;
    }

    subtitle_mgr_printf(50, "%s:%s: returning %d\n", FILENAME, __FUNCTION__,ret);
    return ret;
}


Manager_t SubtitleManager = {
    "Subtitle",
    &Command,
    NULL
};
