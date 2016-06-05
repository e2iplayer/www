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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <memory.h>
#include <asm/types.h>
#include <errno.h>

#include "common.h"
#include "output.h"

/* ***************************** */
/* Makros/Constants              */
/* ***************************** */

//SULGE DEBUG ENABLED
//#define SAM_WITH_DEBUG
#ifdef SAM_WITH_DEBUG
#define SUBTITLE_DEBUG
#else
#define SUBTITLE_SILENT
#endif

#ifdef SUBTITLE_DEBUG

static short debug_level = 0;

#define subtitle_printf(level, fmt, x...) do { \
if (debug_level >= level) printf("[%s:%s] " fmt, __FILE__, __FUNCTION__, ## x); } while (0)
#else
#define subtitle_printf(level, fmt, x...)
#endif

#ifndef SUBTITLE_SILENT
#define subtitle_err(fmt, x...) do { printf("[%s:%s] " fmt, __FILE__, __FUNCTION__, ## x); } while (0)
#else
#define subtitle_err(fmt, x...)
#endif

/* Error Constants */
#define cERR_SUBTITLE_NO_ERROR         0
#define cERR_SUBTITLE_ERROR            -1

static const char FILENAME[] = __FILE__;

/*
Number, Style, Name,, MarginL, MarginR, MarginV, Effect,, Text

1038,0,tdk,,0000,0000,0000,,That's not good.
1037,0,tdk,,0000,0000,0000,,{\i1}Rack them up, rack them up,{\i0}\N{\i1}rack them up.{\i0} [90]
1036,0,tdk,,0000,0000,0000,,Okay, rack them up.
*/

#define PUFFERSIZE 20

/* ***************************** */
/* Types                         */
/* ***************************** */


/* ***************************** */
/* Varaibles                     */
/* ***************************** */

static pthread_mutex_t mutex;
static int isSubtitleOpened = 0;

/* ***************************** */
/* Prototypes                    */
/* ***************************** */

/* ***************************** */
/* MISC Functions                */
/* ***************************** */
static void getMutex(int line)
{
    subtitle_printf(100, "%d requesting mutex\n", line);

    pthread_mutex_lock(&mutex);

    subtitle_printf(100, "%d received mutex\n", line);
}

static void releaseMutex(int line) 
{
    pthread_mutex_unlock(&mutex);

    subtitle_printf(100, "%d released mutex\n", line);
}


/* ***************************** */
/* Functions                     */
/* ***************************** */

static int Write(void *_context, void *data)
{
    Context_t  *context = (Context_t *)_context;
    char *Encoding      = NULL;
    SubtitleOut_t *out  = NULL;
    
    subtitle_printf(10, "\n");

    if (data == NULL)
    {
        subtitle_err("null pointer passed\n");
        return cERR_SUBTITLE_ERROR;
    }

    out = (SubtitleOut_t*) data;
    
    context->manager->subtitle->Command(context, MANAGER_GETENCODING, &Encoding);

    if (Encoding == NULL)
    {
       subtitle_err("encoding unknown\n");
       return cERR_SUBTITLE_ERROR;
    }
    
    subtitle_printf(20, "Encoding:%s Text:%s Len:%d\n", Encoding, (const char*) out->data, out->len);

    if(!strncmp("S_TEXT/SUBRIP", Encoding, 13))
    {
        printf("[%u] [S_TEXT/SUBRIP] [%lld] -> [%lld]\n", out->trackId, out->pts / 90, out->pts / 90 + out->durationMS);
        printf("%s\n", (char *)out->data);
    }
    else
    {
        subtitle_err("unknown encoding %s\n", Encoding);
        return  cERR_SUBTITLE_ERROR;
    }

    subtitle_printf(10, "<\n");
    return cERR_SUBTITLE_NO_ERROR;
}

static int32_t subtitle_Open(Context_t *context __attribute__((unused))) 
{
    uint32_t i = 0 ;

    subtitle_printf(10, "\n");

    if (isSubtitleOpened == 1)
    {
        subtitle_err("already opened! ignoring\n");
        return cERR_SUBTITLE_ERROR;
    }

    getMutex(__LINE__);

    isSubtitleOpened = 1;

    releaseMutex(__LINE__);

    subtitle_printf(10, "<\n");
    return cERR_SUBTITLE_NO_ERROR;
}

static int32_t subtitle_Close(Context_t *context __attribute__((unused)))
{
    uint32_t i = 0 ;

    subtitle_printf(10, "\n");

    getMutex(__LINE__);

    isSubtitleOpened = 0;

    releaseMutex(__LINE__);

    subtitle_printf(10, "<\n");

    return cERR_SUBTITLE_NO_ERROR;
}

static int Command(void  *_context, OutputCmd_t command, void *argument) 
{
    Context_t  *context = (Context_t*) _context;
    int ret = cERR_SUBTITLE_NO_ERROR;

    subtitle_printf(50, "%d\n", command);

    switch(command) 
    {
    case OUTPUT_OPEN:
    {
        ret = subtitle_Open(context);
        break;
    }
    case OUTPUT_CLOSE:
    {
        ret = subtitle_Close(context);
        break;
    }
    case OUTPUT_PLAY: 
    {
        break;
    }
    case OUTPUT_STOP: 
    {
        break;
    }
    case OUTPUT_SWITCH:
    {
        subtitle_err("Subtitle Switch not implemented\n");
        ret = cERR_SUBTITLE_ERROR;
        break;
    }
    case OUTPUT_FLUSH:
    {
        subtitle_err("Subtitle Flush not implemented\n");
        ret = cERR_SUBTITLE_ERROR;
        break;
    }
    case OUTPUT_PAUSE: 
    {
        subtitle_err("Subtitle Pause not implemented\n");
        ret = cERR_SUBTITLE_ERROR;
        break;
    }
    case OUTPUT_CONTINUE:
    {
        subtitle_err("Subtitle Continue not implemented\n");
        ret = cERR_SUBTITLE_ERROR;
        break;
    }
    default:
        subtitle_err("OutputCmd %d not supported!\n", command);
        ret = cERR_SUBTITLE_ERROR;
        break;
    }

    subtitle_printf(50, "exiting with value %d\n", ret);
    return ret;
}


static char *SubtitleCapabilitis[] = { "subtitle", NULL };

Output_t SubtitleOutput = {
    "Subtitle",
    &Command,
    &Write,
    SubtitleCapabilitis
};
