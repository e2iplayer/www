/*
 * simple math operations
 * Copyright (c) 2001, 2002 Fabrice Bellard
 * Copyright (c) 2006 Michael Niedermayer <michaelni@gmx.at> et al
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */
#ifndef AVCODEC_MATHOPS_H
#define AVCODEC_MATHOPS_H

#include <stdint.h>
#include <libavutil/common.h>

#ifndef NEG_USR32
#   define NEG_USR32(a,s) (((uint32_t)(a))>>(32-(s)))
#endif

#ifndef NEG_SSR32
#   define NEG_SSR32(a,s) ((( int32_t)(a))>>(32-(s)))
#endif

#ifndef sign_extend
static inline av_const int sign_extend(int val, unsigned bits)
{
    unsigned shift = 8 * sizeof(int) - bits;
    union { unsigned u; int s; } v = { (unsigned) val << shift };
    return v.s >> shift;
}
#endif

#ifndef zero_extend
static inline av_const unsigned zero_extend(unsigned val, unsigned bits)
{
    return (val << ((8 * sizeof(int)) - bits)) >> ((8 * sizeof(int)) - bits);
}
#endif

#endif /* AVCODEC_MATHOPS_H */
