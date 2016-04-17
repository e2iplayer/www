/*
 * aac helper
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

#ifndef acc_123
#define acc_123

#define AAC_HEADER_LENGTH       7

static inline int aac_get_sample_rate_index (uint32_t sample_rate)
{
    if (96000 <= sample_rate)
        return 0;
    else if (88200 <= sample_rate)
        return 1;
    else if (64000 <= sample_rate)
        return 2;
    else if (48000 <= sample_rate)
        return 3;
    else if (44100 <= sample_rate)
        return 4;
    else if (32000 <= sample_rate)
        return 5;
    else if (24000 <= sample_rate)
        return 6;
    else if (22050 <= sample_rate)
        return 7;
    else if (16000 <= sample_rate)
        return 8;
    else if (12000 <= sample_rate)
        return 9;
    else if (11025 <= sample_rate)
        return 10;
    else if (8000 <= sample_rate)
        return 11;
    else if (7350 <= sample_rate)
        return 12;
    else
        return 13;
}

#endif
