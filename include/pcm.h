/*
 * pcm helper
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

#ifndef pcm_h_
#define pcm_h_

#include <stdint.h>

typedef struct pcmPrivateData_s
{
    uint8_t bResampling;
    
    /* Field taken direct from stream->codec */
    int32_t channels;
    int32_t bits_per_coded_sample;
    int32_t sample_rate;
    int32_t bit_rate;

    int32_t block_align;
    int32_t frame_size;

    int32_t codec_id;

    uint8_t *private_data;
    uint32_t private_size;
} pcmPrivateData_t;
#endif
