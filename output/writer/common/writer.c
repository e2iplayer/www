/*
 * linuxdvb output/writer handling.
 *
 * konfetti 2010
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
#include <errno.h>

#include "misc.h"
#include "writer.h"
#include "debug.h"
#include "common.h"

/* ***************************** */
/* Makros/Constants              */
/* ***************************** */

/* ***************************** */
/* Types                         */
/* ***************************** */

/* ***************************** */
/* Varaibles                     */
/* ***************************** */

/* ***************************** */
/* Prototypes                    */
/* ***************************** */

/* ***************************** */
/*  Functions                    */
/* ***************************** */
void FlushPipe(int pipefd)
{
    char tmp;
    while(1 == read(pipefd, &tmp, 1));
}

ssize_t WriteExt(WriteV_t _call, int fd, void *data, size_t size)
{
    struct iovec iov[1];
    iov[0].iov_base = data;
    iov[0].iov_len = size;
    return _call(fd, iov, 1);
}
