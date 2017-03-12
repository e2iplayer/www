/*
 * types
 *
 * Copyright (c) 2006 vixy project
 *
 * This file is part of VIXY FLV Converter.
 *
 * 'VIXY FLV Converter' is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * 'VIXY FLV Converter' is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef TYPE_H
#define TYPE_H

////////////////////////////////////////////////////////////////////////////////////////////////////////////////// 
typedef unsigned char	uint8;
typedef unsigned short	uint16;
typedef unsigned int	uint32;

typedef signed char		int8;
typedef signed short	int16;
typedef signed int		int32;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////// 
typedef struct _VLCDEC
{
	int bits;
	int value;
	
	int bits_ex;
	int value_ex;
	
} VLCDEC;

#endif // TYPE_H
