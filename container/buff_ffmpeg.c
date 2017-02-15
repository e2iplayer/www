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
 
#define FILLBUFSIZE 0
#define FILLBUFDIFF 1048576
#define FILLBUFPAKET 5120
#define FILLBUFSEEKTIME 3 //sec
#define TIMEOUT_MAX_ITERS 10

static int ffmpeg_buf_size = FILLBUFSIZE + FILLBUFDIFF;
static int ffmpeg_buf_seek_time = FILLBUFSEEKTIME;
static int(*ffmpeg_read_org)(void *opaque, uint8_t *buf, int buf_size) = NULL;
static int(*ffmpeg_real_read_org)(void *opaque, uint8_t *buf, int buf_size) = NULL;

static int64_t(*ffmpeg_seek_org)(void *opaque, int64_t offset, int whence) = NULL;
static unsigned char* ffmpeg_buf_read = NULL;
static unsigned char* ffmpeg_buf_write = NULL;
static unsigned char* ffmpeg_buf = NULL;
static pthread_t fillerThread;
static int hasfillerThreadStarted[10] = {0,0,0,0,0,0,0,0,0,0};
int hasfillerThreadStartedID = 0;
static pthread_mutex_t fillermutex;
static int ffmpeg_buf_valid_size = 0;
static int ffmpeg_do_seek_ret = 0;
static int ffmpeg_do_seek = 0;
static int ffmpeg_buf_stop = 0;

static Context_t *g_context = 0;
static int64_t playPts = -1;
static int32_t finishTimeout = 0;
static int8_t pauseTimeout = 0;
static int64_t maxInjectedPTS = INVALID_PTS_VALUE;

static int64_t update_max_injected_pts(int64_t pts)
{
    if(pts > 0 && pts != INVALID_PTS_VALUE)
    {
        if(maxInjectedPTS == INVALID_PTS_VALUE || pts > maxInjectedPTS)
        {
            maxInjectedPTS = pts;
        }
    }
    return maxInjectedPTS;
}

int64_t get_play_pts()
{
    return playPts;
}

void reset_finish_timeout()
{
    playPts = -1;
    finishTimeout = 0;
}

void set_pause_timeout(uint8_t pause)
{
    reset_finish_timeout();
    pauseTimeout = pause;
}

static int8_t is_finish_timeout()
{
    if (finishTimeout > TIMEOUT_MAX_ITERS)
    {
        return 1;
    }
    return 0;
}

static void update_finish_timeout()
{
    if(0 == pauseTimeout)
    {
        int64_t maxInjectedPts = update_max_injected_pts(-1);
        int64_t currPts = -1;
        int32_t ret = g_context->playback->Command(g_context, PLAYBACK_PTS, &currPts);
        finishTimeout += 1;
        
        if(maxInjectedPts < 0 || maxInjectedPts == INVALID_PTS_VALUE)
        {
            maxInjectedPts = 0;
        }
        
        //printf("ret[%d] playPts[%lld] currPts[%lld] maxInjectedPts[%lld]\n", ret, playPts, currPts, maxInjectedPts);
        
        /* On some STBs PTS readed from decoder is invalid after seek or at start 
         * this is the reason for additional validation when we what to close immediately
         */
        if( !progressive_playback && 0 == ret && currPts >= maxInjectedPts && 
            ((currPts - maxInjectedPts) / 90000) < 2 )
        {
            /* close immediately 
             */
            finishTimeout = TIMEOUT_MAX_ITERS + 1;
        }
        else if (0 == ret && (playPts != currPts && maxInjectedPts > currPts))
        {
            playPts = currPts;
            finishTimeout = 0;
        }
    }
}

static int32_t ffmpeg_read_wrapper_base(void *opaque, uint8_t *buf, int32_t buf_size, uint8_t type)
{
    int32_t len = 0;
    if(0 == PlaybackDieNow(0))
    {
        len = ffmpeg_real_read_org(opaque, buf, buf_size);
        while(len < buf_size && g_context && 0 == PlaybackDieNow(0))
        {
            if(type && len > 0)
            {
                break;
            }
            
            int32_t partLen = ffmpeg_real_read_org(opaque, buf+len, buf_size-len);
            if (partLen > 0)
            {
                len += partLen;
                finishTimeout = 0;
                continue;
            }
            else if (is_finish_timeout())
            {
                len = 0;
                break;
            }
            
            update_finish_timeout();
            
            usleep(100000);
            continue;
        }
    }
    //printf("len [%d] finishTimeout[%d]\n", len, finishTimeout);
    return len;
}

static int32_t ffmpeg_read_wrapper(void *opaque, uint8_t *buf, int32_t buf_size)
{
    if(progressive_playback)
    {
        return ffmpeg_read_wrapper_base(opaque, buf, buf_size, 0);
    }
    else
    {
        /* at start it was progressive playback, but dwonload, finished
         */
        return ffmpeg_real_read_org(opaque, buf, buf_size);
    }
}

static int32_t ffmpeg_read_wrapper2(void *opaque, uint8_t *buf, int32_t buf_size)
{
    return ffmpeg_read_wrapper_base(opaque, buf, buf_size, 1);
}

//for buffered io
void getfillerMutex(const char *filename, const char *function, int line) 
{
    ffmpeg_printf(100, "::%d requesting mutex\n", line);

    pthread_mutex_lock(&fillermutex);

    ffmpeg_printf(100, "::%d received mutex\n", line);
}

void releasefillerMutex(const char *filename, const const char *function, int line) 
{
    pthread_mutex_unlock(&fillermutex);

    ffmpeg_printf(100, "::%d released mutex\n", line);
}
//for buffered io (end)encoding

static int32_t container_set_ffmpeg_buf_seek_time(int32_t* time)
{
    ffmpeg_buf_seek_time = (*time);
    return cERR_CONTAINER_FFMPEG_NO_ERROR;
}

static int32_t container_set_ffmpeg_buf_size(int32_t* size)
{
    if(ffmpeg_buf == NULL)
    {
        if(*size == 0)
        {
            ffmpeg_buf_size = 0;
        }
        else
        {
            ffmpeg_buf_size = (*size) + FILLBUFDIFF;
        }
    }
    
    ffmpeg_printf(10, "size=%d, buffer size=%d\n", (*size), ffmpeg_buf_size);
    return cERR_CONTAINER_FFMPEG_NO_ERROR;
}

static int32_t container_get_ffmpeg_buf_size(int32_t* size)
{
    *size = ffmpeg_buf_size - FILLBUFDIFF;
    return cERR_CONTAINER_FFMPEG_NO_ERROR;
}

static int32_t container_get_fillbufstatus(int32_t* size)
{
    int32_t rwdiff = 0;

    if(ffmpeg_buf != NULL && ffmpeg_buf_read != NULL && ffmpeg_buf_write != NULL)
    {
        if(ffmpeg_buf_read < ffmpeg_buf_write)
            rwdiff = ffmpeg_buf_write - ffmpeg_buf_read;
        if(ffmpeg_buf_read > ffmpeg_buf_write)
        {
            rwdiff = (ffmpeg_buf + ffmpeg_buf_size) - ffmpeg_buf_read;
            rwdiff += ffmpeg_buf_write - ffmpeg_buf;
        }

        *size = rwdiff;
    }

    return cERR_CONTAINER_FFMPEG_NO_ERROR;
}

static int32_t container_stop_buffer()
{
    ffmpeg_buf_stop = 1;
    return 0;
}

//flag 0: start direct
//flag 1: from thread
static void ffmpeg_filler(Context_t *context, int32_t id, int32_t* inpause, int32_t flag)
{
    int32_t len = 0;
    int32_t rwdiff = ffmpeg_buf_size;
    uint8_t buf[FILLBUFPAKET];

    if(ffmpeg_read_org == NULL || ffmpeg_seek_org == NULL)
    {
        ffmpeg_err("ffmpeg_read_org or ffmpeg_seek_org is NULL\n");
        return;
    }

    while( (flag == 0 && avContextTab[0] != NULL && avContextTab[0]->pb != NULL && rwdiff > FILLBUFDIFF) || 
           (flag == 1 && hasfillerThreadStarted[id] == 1 && avContextTab[0] != NULL && avContextTab[0]->pb != NULL && rwdiff > FILLBUFDIFF) )
    {
         if( 0 == PlaybackDieNow(0))
         {
            break;
         }
         
         if(flag == 0 && ffmpeg_buf_stop == 1)
         {
             ffmpeg_buf_stop = 0;
             break;
         }

         getfillerMutex(__FILE__, __FUNCTION__,__LINE__);
         //do a seek
         if(ffmpeg_do_seek != 0)
         {
             ffmpeg_do_seek_ret = ffmpeg_seek_org(avContextTab[0]->pb->opaque, avContextTab[0]->pb->pos + ffmpeg_do_seek, SEEK_SET);
             if(ffmpeg_do_seek_ret >= 0)
             {
                 ffmpeg_buf_write = ffmpeg_buf;
                 ffmpeg_buf_read = ffmpeg_buf;
             }

             ffmpeg_do_seek = 0;
         }

         if(ffmpeg_buf_read == ffmpeg_buf_write)
         {
             ffmpeg_buf_valid_size = 0;
             rwdiff = ffmpeg_buf_size;
         }
         
         if(ffmpeg_buf_read < ffmpeg_buf_write)
         {
             rwdiff = (ffmpeg_buf + ffmpeg_buf_size) - ffmpeg_buf_write;
             rwdiff += ffmpeg_buf_read - ffmpeg_buf;
         }
         
         if(ffmpeg_buf_read > ffmpeg_buf_write)
         {
            rwdiff = ffmpeg_buf_read - ffmpeg_buf_write;
         }
         
         int32_t size = FILLBUFPAKET;
         if(rwdiff - FILLBUFDIFF < size)
         {
             size = (rwdiff - FILLBUFDIFF);
         }

         if(ffmpeg_buf_write + size > ffmpeg_buf + ffmpeg_buf_size)
         {
             size = (ffmpeg_buf + ffmpeg_buf_size) - ffmpeg_buf_write;
         }

         if(ffmpeg_buf_write == ffmpeg_buf + ffmpeg_buf_size)
         {
             ffmpeg_buf_write = ffmpeg_buf;
         }

         releasefillerMutex(__FILE__, __FUNCTION__,__LINE__);

         if(size > 0)
         {
             if(flag == 1 && hasfillerThreadStarted[id] == 2) break;
             len = ffmpeg_read_org(avContextTab[0]->pb->opaque, buf, size);
             if(flag == 1 && hasfillerThreadStarted[id] == 2) break;

             ffmpeg_printf(20, "buffer-status (free buffer=%d)\n", rwdiff - FILLBUFDIFF - len);

             getfillerMutex(__FILE__, __FUNCTION__,__LINE__);
             if(len > 0)
             { 
                 memcpy(ffmpeg_buf_write, buf, len);
                 ffmpeg_buf_write += len;
             }
             else
             {
                 releasefillerMutex(__FILE__, __FUNCTION__,__LINE__);
                 ffmpeg_err("read not ok ret=%d\n", len);
                 break;
             }
             releasefillerMutex(__FILE__, __FUNCTION__,__LINE__);
        }
        else
        {
            //on long pause the server close the connection, so we use seek to reconnect
            if(context != NULL && context->playback != NULL && inpause != NULL)
            {
                if((*inpause) == 0 && context->playback->isPaused)
                {
                    (*inpause) = 1;
                }
                else if((*inpause) == 1 && !context->playback->isPaused)
                {
                    int32_t buflen = 0;
                    (*inpause) = 0;

                    getfillerMutex(__FILE__, __FUNCTION__,__LINE__);
                    if(ffmpeg_buf_read < ffmpeg_buf_write)
                    {
                        buflen = ffmpeg_buf_write - ffmpeg_buf_read;
                    }
                    
                    if(ffmpeg_buf_read > ffmpeg_buf_write)
                    {
                        buflen = (ffmpeg_buf + ffmpeg_buf_size) - ffmpeg_buf_read;
                        buflen += ffmpeg_buf_write - ffmpeg_buf;
                    } 
                    ffmpeg_seek_org(avContextTab[0]->pb->opaque, avContextTab[0]->pb->pos + buflen, SEEK_SET);
                    releasefillerMutex(__FILE__, __FUNCTION__,__LINE__);
                }
            }
        }
    }
}

static void ffmpeg_fillerTHREAD(Context_t *context)
{
    int32_t inpause = 0;
    int32_t id = hasfillerThreadStartedID;

    ffmpeg_printf(10, "Running ID=%d!\n", id);

    while(hasfillerThreadStarted[id] == 1)
    {
        ffmpeg_filler(context, id, &inpause, 1);
        usleep(10000);
    }

    hasfillerThreadStarted[id] = 0;

    ffmpeg_printf(10, "terminating ID=%d\n", id);
}

static int32_t ffmpeg_start_fillerTHREAD(Context_t *context)
{
    int32_t error;
    int32_t ret = 0, i = 0;
    pthread_attr_t attr;

    ffmpeg_printf(10, "\n");

    if (context && context->playback && context->playback->isPlaying)
    {
        ffmpeg_printf(10, "is Playing\n");
    }
    else
    {
        ffmpeg_printf(10, "is NOT Playing\n");
    }
    
    //get filler thread ID
    //if the thread hangs for long time, we use a new id
    for(i = 0; i < 10; i++)
    {
        if(hasfillerThreadStarted[i] == 0)
        {
            hasfillerThreadStartedID = i;
            break;
        }
    }

    if (hasfillerThreadStarted[hasfillerThreadStartedID] == 0)
    {
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

        hasfillerThreadStarted[hasfillerThreadStartedID] = 1;
        if((error = pthread_create(&fillerThread, &attr, (void *)&ffmpeg_fillerTHREAD, context)) != 0)
        {
            hasfillerThreadStarted[hasfillerThreadStartedID] = 0;
            ffmpeg_printf(10, "Error creating filler thread, error:%d:%s\n", error,strerror(error));

            ret = cERR_CONTAINER_FFMPEG_ERR;
        }
        else
        {
            ffmpeg_printf(10, "Created filler thread\n");
        }
    }
    else
    {
        ffmpeg_printf(10, "All filler thread ID's in use!\n");

        ret = cERR_CONTAINER_FFMPEG_ERR;
    }

    ffmpeg_printf(10, "exiting with value %d\n", ret);
    return ret;
}

static int32_t ffmpeg_read_real(void *opaque, uint8_t *buf, int32_t buf_size)
{
    int32_t len = buf_size;
    int32_t rwdiff = 0;

    if(buf_size > 0)
    {
        getfillerMutex(__FILE__, __FUNCTION__,__LINE__);

        if(ffmpeg_buf_read < ffmpeg_buf_write)
            rwdiff = ffmpeg_buf_write - ffmpeg_buf_read;
        if(ffmpeg_buf_read > ffmpeg_buf_write)
        {
            rwdiff = (ffmpeg_buf + ffmpeg_buf_size) - ffmpeg_buf_read;
            rwdiff += ffmpeg_buf_write - ffmpeg_buf;
        }
        rwdiff--;

        if(len > rwdiff)
        {
            len = rwdiff;
        }

        if (ffmpeg_buf_read + len > ffmpeg_buf + ffmpeg_buf_size)
        {
            len = (ffmpeg_buf + ffmpeg_buf_size) - ffmpeg_buf_read;
        }

        if(len > 0)
        {
            memcpy(buf, ffmpeg_buf_read, len);
            ffmpeg_buf_read += len;

            if(ffmpeg_buf_valid_size < FILLBUFDIFF)
            {
                if(ffmpeg_buf_valid_size + len > FILLBUFDIFF)
                {
                    ffmpeg_buf_valid_size = FILLBUFDIFF;
                }
                else
                {
                    ffmpeg_buf_valid_size += len;
                }
            }

            if(ffmpeg_buf_read == ffmpeg_buf + ffmpeg_buf_size)
            {
                ffmpeg_buf_read = ffmpeg_buf;
            }
        }
        else
        {
            len = 0;
        }
        releasefillerMutex(__FILE__, __FUNCTION__,__LINE__);
    }

    return len;
}

static int32_t ffmpeg_read(void *opaque, uint8_t *buf, int32_t buf_size)
{
    int32_t sumlen = 0;
    int32_t len = 0;
    int32_t count = 2000;

    while(sumlen < buf_size && (--count) > 0 && 0 == PlaybackDieNow(0))
    {
        len = ffmpeg_read_real(opaque, buf, buf_size - sumlen);
        sumlen += len;
        buf += len;
        if(len == 0)
        {
            usleep(10000);
        }
    }

    if(count == 0)
    {
        if(sumlen == 0)
        {
            ffmpeg_err( "Timeout waiting for buffered data (buf_size=%d sumlen=%d)!\n", buf_size, sumlen);
        }
        else
        {
            ffmpeg_err( "Timeout, not all buffered data availabel (buf_size=%d sumlen=%d)!\n", buf_size, sumlen);
        }
    }

    return sumlen;
}

static int64_t ffmpeg_seek(void *opaque, int64_t offset, int32_t whence)
{
    int64_t diff;
    int32_t rwdiff = 0;
    whence &= ~AVSEEK_FORCE;

    if(whence != SEEK_CUR && whence != SEEK_SET)
    {
        return AVERROR(EINVAL);
    }

    if(whence == SEEK_CUR)
    {
        diff = offset;
    }
    else
    {
        diff = offset - avContextTab[0]->pb->pos;
    }

    if(diff == 0)
    {
        return avContextTab[0]->pb->pos;
    }

    getfillerMutex(__FILE__, __FUNCTION__,__LINE__);

    if(ffmpeg_buf_read < ffmpeg_buf_write)
    {
        rwdiff = ffmpeg_buf_write - ffmpeg_buf_read;
    }
    
    if(ffmpeg_buf_read > ffmpeg_buf_write)
    {
        rwdiff = (ffmpeg_buf + ffmpeg_buf_size) - ffmpeg_buf_read;
        rwdiff += ffmpeg_buf_write - ffmpeg_buf;
    }

    if(diff > 0 && diff < rwdiff)
    {
        /* can do the seek inside the buffer */
        ffmpeg_printf(20, "buffer-seek diff=%lld\n", diff);
        if(diff > (ffmpeg_buf + ffmpeg_buf_size) - ffmpeg_buf_read)
        {
            ffmpeg_buf_read = ffmpeg_buf + (diff - ((ffmpeg_buf + ffmpeg_buf_size) - ffmpeg_buf_read));
        }
        else
        {
            ffmpeg_buf_read = ffmpeg_buf_read + diff;
        }
    }
    else if(diff < 0 && diff * -1 < ffmpeg_buf_valid_size)
    {
        /* can do the seek inside the buffer */
        ffmpeg_printf(20, "buffer-seek diff=%lld\n", diff);
        int32_t tmpdiff = diff * -1;
        if(tmpdiff > ffmpeg_buf_read - ffmpeg_buf)
        {
            ffmpeg_buf_read = (ffmpeg_buf + ffmpeg_buf_size) - (tmpdiff - (ffmpeg_buf_read - ffmpeg_buf));
        }
        else
        {
            ffmpeg_buf_read = ffmpeg_buf_read - tmpdiff;
        }
    }
    else
    {
        releasefillerMutex(__FILE__, __FUNCTION__,__LINE__);
        ffmpeg_printf(20, "real-seek diff=%lld\n", diff);

        ffmpeg_do_seek_ret = 0;
        ffmpeg_do_seek = diff;
        while(ffmpeg_do_seek != 0)
        {
            usleep(100000);
        }

        ffmpeg_do_seek = 0;
        if(ffmpeg_do_seek_ret < 0)
        {
            ffmpeg_err("seek not ok ret=%d\n", ffmpeg_do_seek_ret);
            return ffmpeg_do_seek_ret;
        }

        //fill buffer
        int32_t count = ffmpeg_buf_seek_time * 10;
        int32_t size = 0;

        container_get_fillbufstatus(&size);
        while(size < ffmpeg_buf_size - FILLBUFDIFF && (--count) > 0)
        {
            usleep(100000);
            container_get_fillbufstatus(&size);
        }

        return avContextTab[0]->pb->pos + diff;
    }

    releasefillerMutex(__FILE__, __FUNCTION__,__LINE__);
    return avContextTab[0]->pb->pos + diff;
}

static void ffmpeg_buf_free()
{
    ffmpeg_read_org = NULL;
    ffmpeg_seek_org = NULL;
    ffmpeg_buf_read = NULL;
    ffmpeg_buf_write = NULL;
    free(ffmpeg_buf);
    ffmpeg_buf = NULL;
    ffmpeg_buf_valid_size = 0;
    ffmpeg_do_seek_ret = 0;
    ffmpeg_do_seek = 0;
    ffmpeg_buf_stop = 0;
    hasfillerThreadStartedID = 0;
}
