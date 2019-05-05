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
#include <dlfcn.h>
#include "plugins/png.h"
#include "debug.h"

static void *handle;
int (*SaveRGBAImage_handle)(const char *filename, const unsigned char *data, int width, int height);


int PNGPlugin_saveRGBAImage(const char *filename, const unsigned char *data, int width, int height)
{
    if (SaveRGBAImage_handle != NULL)
        return SaveRGBAImage_handle(filename, data, width, height);
    return -1;
}

int PNGPlugin_init(void)
{
    if (NULL != handle)
        return 0; /* Already initialized */
    else
    {
        handle = dlopen("exteplayer3png.so", RTLD_LAZY);
        if (handle)
        {
            char *error = NULL;

            dlerror();    /* Clear any existing error */

            *(void **) (&SaveRGBAImage_handle) = dlsym(handle, "SaveRGBAImage");

            if ((error = dlerror()) != NULL)
            {
                dlclose(handle);
                plugin_err("%s\n", error);
                return -2;
            }
            return 0;
        }
        plugin_err( "%s\n", dlerror());
    }

    return -1;
}

int PNGPlugin_term(void)
{
    if (handle)
    {
        dlclose(handle);
        handle = NULL;
        SaveRGBAImage_handle = NULL;
        return 0;
    }
    return -1;
}
