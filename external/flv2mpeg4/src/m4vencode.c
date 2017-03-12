/*
 * MPEG4 Bitstream/VLC Encoder
 *
 * Copyright (c) 2006 vixy project
 *
 * This file contains the code that based on FFmpeg (http://ffmpeg.mplayerhq.hu/)
 * See original copyright notice in /FFMPEG_CREDITS and /FFMPEG_IMPORTS
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

#include "m4vencode.h"
#include "m4vencode_tables.h"
#include "bitwriter.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////// 

// same as H.263
static const uint32 vlce_intra_MCBPC_code[9] = { 1, 1, 2, 3, 1, 1, 2, 3, 1 };
static const uint32 vlce_intra_MCBPC_bits[9] = { 1, 3, 3, 3, 4, 6, 6, 6, 9 };
static const uint32 vlce_cbpy_code[16] = { 3, 5, 4, 9, 3, 7, 2, 11, 2, 3, 5, 10, 4, 8, 6, 3};
static const uint32 vlce_cbpy_bits[16] = { 4, 5, 5, 4, 5, 4, 6,  4, 5, 6, 4,  4, 4, 4, 4, 2};

// same as H.263
static const uint32 vlce_inter_MCBPC_code[28] = { 
    1, 3, 2, 5, 
    3, 4, 3, 3, 
    3, 7, 6, 5,
    4, 4, 3, 2,
    2, 5, 4, 5,
    1, 0, 0, 0, /* Stuffing */
    2, 12, 14, 15,
};

// same as H.263
static const uint32 vlce_inter_MCBPC_bits[28] = { 
    1, 4, 4, 6, /* inter  */
    5, 8, 8, 7, /* intra  */
    3, 7, 7, 9, /* interQ */
    6, 9, 9, 9, /* intraQ */
    3, 7, 7, 8, /* inter4 */
    9, 0, 0, 0, /* Stuffing */
    11, 13, 13, 13,/* inter4Q*/
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////// 
static void __inline encode_DC(BW* p, int level, int n)
{
    if(level<-255 || level>255) printf("dc overflow\n");

#if 1
	level += 256;
	if (n < 4)
	{
		put_bits(p, uni_DCtab_lum_len[level], uni_DCtab_lum_bits[level]);
	}
	else
	{
		put_bits(p, uni_DCtab_chrom_len[level], uni_DCtab_chrom_bits[level]);
	}
#else

	int size, v;
    /* find number of bits */
    size = 0;
    v = abs(level);
    while (v) {
        v >>= 1;
        size++;
    }

    if (n < 4) {
        /* luminance */
        put_bits(p, DCtab_lum[size][1], DCtab_lum[size][0]);
    } else {
        /* chrominance */
        put_bits(p, DCtab_chrom[size][1], DCtab_chrom[size][0]);
    }

    /* encode remaining bits */
    if (size > 0) {
        if (level < 0)
            level = (-level) ^ ((1 << size) - 1);
        put_bits(p, size, level);
        if (size > 8)
            put_bits(p, 1, 1);
    }

#endif

}

static void __inline encode_escape_3(BW* p, int last, int run, int level)
{
#if 0
	put_bits(p, 
		7+2+1+6+1+12+1, //30bit
		(3<<23)+(3<<21)+(last<<20)+(run<<14)+(1<<13)+(((level-64)&0xfff)<<1)+1);
#else
	put_bits(p, 7, 3); // escape
	put_bits(p, 2, 3); // escape3
	put_bits(p, 1, last);
	put_bits(p, 6, run);
	put_bits(p, 1, 1); // marker
	put_bits(p, 12, ((level-64)&0xfff));
	put_bits(p, 1, 1); // marker
#endif
}

#define UNI_MPEG4_ENC_INDEX(last, run, level) ((last)*128*64 + (run)*128 + (level))

static void __inline encode_AC(BW* p, M4V_BLOCK* block, int intra)
{
	int i = intra;
	int last_index = block->last_index;
	int last_non_zero = i - 1;
	
	const uint8*  scan_table = zig_zag_scan; // !!!

#if 1
	const uint8*  len_tab;
	const uint32* bits_tab;

	if (intra)
	{
		len_tab  = uni_mpeg4_intra_rl_len;
		bits_tab = uni_mpeg4_intra_rl_bits;
	}
	else
	{
		len_tab  = uni_mpeg4_inter_rl_len;
		bits_tab = uni_mpeg4_inter_rl_bits;
	}
	
	for (; i < last_index; i++)
	{
		int level = block->block[scan_table[i]];
		if (level)
		{
			int run = i - last_non_zero - 1;
			level += 64;
			if ((level & (~127)) == 0)
			{
				const int index = UNI_MPEG4_ENC_INDEX(0, run, level);
				put_bits(p, len_tab[index], bits_tab[index]);
			}
			else
			{
				encode_escape_3(p, 0, run, level);
			}
			
			last_non_zero = i;
		}
	}
	
	{
		int level = block->block[scan_table[i]];
		int run = i - last_non_zero - 1;
		level += 64;
		if ((level & (~127)) == 0)
		{
			const int index = UNI_MPEG4_ENC_INDEX(1, run, level);
			put_bits(p, len_tab[index], bits_tab[index]);
		}
		else
		{
			encode_escape_3(p, 1, run, level);
		}
	}
#else
	const RL* rl;
	int last, sign, code;
		
	if (intra)
	{
		rl = &rl_intra;
	}
	else
	{
		rl = &rl_inter;
	}

    for (; i <= last_index; i++) 
	{
        const int slevel = block->block[scan_table[i]];
        if (slevel) 
		{
            int level;
            int run = i - last_non_zero - 1;
            last = (i == last_index);
            sign = 0;
            level = slevel;
            if (level < 0) 
			{
                sign = 1;
                level = -level;
            }
            
			code = get_rl_index(rl, last, run, level);
            put_bits(p, rl->table_vlc[code][1], rl->table_vlc[code][0]);
            if (code == rl->n) 
			{
                int level1, run1;
                level1 = level - rl->max_level[run][last];
                if (level1 < 1) 
                    goto esc2;
                
				code = get_rl_index(rl, last, run, level1);
                if (code == rl->n) 
				{
                esc2:
                    put_bits(p, 1, 1);
                    if (level > 64)
                        goto esc3;
                    run1 = run - rl->max_run[level][last] - 1;
                    if (run1 < 0)
                        goto esc3;
                    code = get_rl_index(rl, last, run1, level);
                    if (code == rl->n) 
					{
                    esc3:
                        /* third escape */
                        put_bits(p, 1, 1);
                        put_bits(p, 1, last);
                        put_bits(p, 6, run);
                        put_bits(p, 1, 1);
                        put_bits(p, 12, slevel & 0xfff);
                        put_bits(p, 1, 1);
                    } 
					else 
					{
                        /* second escape */
                        put_bits(p, 1, 0);
                        put_bits(p, rl->table_vlc[code][1], rl->table_vlc[code][0]);
                        put_bits(p, 1, sign);
                    }
                } 
				else 
				{
                    /* first escape */
                    put_bits(p, 1, 0);
                    put_bits(p, rl->table_vlc[code][1], rl->table_vlc[code][0]);
                    put_bits(p, 1, sign);
                }
            } 
			else 
			{
                put_bits(p, 1, sign);
            }
            last_non_zero = i;
        }
    }

#endif	
	
	
}

static void __inline encode_intra_block(BW* bw, M4V_BLOCK* block, int n)
{
	encode_DC(bw, block->block[0], n);
	encode_AC(bw, block, 1);
}

static void __inline encode_inter_block(BW* bw, M4V_BLOCK* block)
{
	encode_AC(bw, block, 0);
}

// same as H.263
static void __inline encode_intra_I_MCBPC(BW* bw, int cbpc)
{
	put_bits(bw, vlce_intra_MCBPC_bits[cbpc], vlce_intra_MCBPC_code[cbpc]);
}

// same as H.263
static void __inline encode_intra_P_MCBPC(BW* bw, int cbpc)
{
	put_bits(bw, vlce_inter_MCBPC_bits[cbpc+4], vlce_inter_MCBPC_code[cbpc+4]);
}

// same as H.263
static void __inline encode_inter_16x16_MCBPC(BW* bw, int cbpc)
{
	put_bits(bw, vlce_inter_MCBPC_bits[cbpc], vlce_inter_MCBPC_code[cbpc]);
}

// same as H.263
static void __inline encode_inter_8x8_MCBPC(BW* bw, int cbpc)
{
	put_bits(bw, vlce_inter_MCBPC_bits[cbpc+16], vlce_inter_MCBPC_code[cbpc+16]);
}


// same as H.263
static void __inline encode_cbpy(BW* bw, int cbpy)
{
	put_bits(bw, vlce_cbpy_bits[cbpy], vlce_cbpy_code[cbpy]);
}

// same as H.263
static void __inline encode_dquant(BW* bw, int dquant)
{
	const uint32 dquant_code[5]= {1, 0, -1, 2, 3};
	if (dquant)
	{
		put_bits(bw, 2, dquant_code[dquant + 2]);
	}
}

// same as FLV
static void __inline encode_motion(BW* bw, VLCDEC* mv_x, VLCDEC* mv_y)
{
	put_vlcdec(bw, mv_x);
	if (mv_x->bits_ex)
	{
		put_bits(bw, 1, mv_x->value_ex & 1);
		if (mv_x->bits_ex > 1)
		{
			put_bits(bw, mv_x->bits_ex-1, mv_x->value_ex >> 1);
		}

	}
	put_vlcdec(bw, mv_y);
	if (mv_y->bits_ex)
	{
		put_bits(bw, 1, mv_y->value_ex & 1);
		if (mv_y->bits_ex > 1)
		{
			put_bits(bw, mv_y->bits_ex-1, mv_y->value_ex >> 1);
		}
	}
}

// same as FLV
static void __inline encode_mb_inter_internal(BW* bw, M4V_MICROBLOCK* mb)
{
	int cbp=0, cbpc, cbpy;
	int i;

	for (i = 0; i < 6; i++)
	{
		if (mb->block[i].last_index >= 0)
		{
			cbp |= 1 << (5 - i);
		}
	}

	cbpc = cbp & 3;
	cbpy = cbp >> 2;
	cbpy ^= 0xF;
	
	if (mb->dquant) cbpc += 8;

	switch (mb->mv_type)
	{
	case MV_TYPE_16X16:
		encode_inter_16x16_MCBPC(bw, cbpc);
		encode_cbpy(bw, cbpy);
		encode_dquant(bw, mb->dquant);
		encode_motion(bw, &mb->mv_x[0], &mb->mv_y[0]);
		break;
	case MV_TYPE_8X8:
		encode_inter_8x8_MCBPC(bw, cbpc);
		encode_cbpy(bw, cbpy);
		encode_dquant(bw, mb->dquant);
		for (i = 0; i < 4; i++)
		{
			encode_motion(bw, &mb->mv_x[i], &mb->mv_y[i]);
		}
		break;
	}

	for (i = 0; i < 6; i++)
	{
		encode_inter_block(bw, &mb->block[i]);
	}
}

static void __inline encode_mb_intra_internal(BW* bw, M4V_MICROBLOCK* mb, int iframe)
{
	int cbp=0, cbpc, cbpy;
	int i;

	for (i = 0; i < 6; i++)
	{
		if (mb->block[i].last_index >= 1)
		{
			cbp |= 1 << (5 - i);
		}
	}

	cbpc = cbp & 3;
	if (iframe)
	{
		if (mb->dquant) cbpc += 4;
		encode_intra_I_MCBPC(bw, cbpc);
	}
	else
	{
		if (mb->dquant) cbpc += 8;
		encode_intra_P_MCBPC(bw, cbpc);
	}

	put_bits(bw, 1, 0); // AC Prediction = no

	cbpy = cbp >> 2;

	encode_cbpy(bw, cbpy);
	encode_dquant(bw, mb->dquant);
	

	for (i = 0; i < 6; i++)
	{
		encode_intra_block(bw, &mb->block[i], i);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////// 
static int __inline encode_vo_header(BW* p)
{
	put_bits(p, 16, 0);
	put_bits(p, 16, VOS_STARTCODE);

	put_bits(p, 8, 1); // *** profile_and_level_indidation
	
	put_bits(p, 16, 0);
	put_bits(p, 16, VISUAL_OBJECT_STARTCODE);

	put_bits(p, 1, 1);
	put_bits(p, 4, 1); // vo_vel_id
	put_bits(p, 3, 1); // priority
	put_bits(p, 4, 1); // visual_object_type = video object
	put_bits(p, 1, 0); // video signal type = no clue
	
	m4v_stuffing(p);

	return 0;
}

static int __inline encode_vol_header(BW* p, M4V_VOL* vol)
{
	const int vo_number = 0;
	const int vol_number = 0;

	put_bits(p, 16, 0);
	put_bits(p, 16, 0x100 + vo_number);

	put_bits(p, 16, 0);
	put_bits(p, 16, 0x120 + vol_number);

	put_bits(p, 1, 0); // random_accessible_vol
	put_bits(p, 8, 1); // video_object_type_indication = Simple Object Type
	
	put_bits(p, 1, 0); //is_object_layer_identifier

	put_bits(p, 4, 1); // *** aspect_ratio_info = 1(1:1)
	
	put_bits(p, 1, 0); //vol_control_parameters
	
	put_bits(p, 2, 0); // shape_type
	put_bits(p, 1, 1); // marker
	
	if (vol->time_bits != 5) return -1; // for vop_time_increment_resolution = 30
	put_bits(p, 16, 30); // *** vop_time_increment_resolution = 30
	
	put_bits(p, 1, 1); // marker
	put_bits(p, 1, 0); // fixed vop rate = no
	put_bits(p, 1, 1); // marker
	put_bits(p, 13, vol->width); // width
	put_bits(p, 1, 1); // marker
	put_bits(p, 13, vol->height); // height
	put_bits(p, 1, 1); // marker
	put_bits(p, 1, 0); // progressive = false
	put_bits(p, 1, 1); // obmc disable = true
	put_bits(p, 1, 0); // sprite = disable
	put_bits(p, 1, 0); // not8bit = false
	put_bits(p, 1, 0); // quant type = H.263

	put_bits(p, 1, 1); // complexity estimaition disable = true
	put_bits(p, 1, 1); // resync marker disable = true
	put_bits(p, 1, 0); // data pertitioning = false
	put_bits(p, 1, 0); // scalability = false
	
	m4v_stuffing(p);
	return 0;
}

static int __inline encode_vop_header(BW* p, M4V_VOP* vop, int time_bits, int vop_not_coded)
{
//	static int time_old = 0;

	int time_incr = vop->icount;
	if (vop->time != 0)
		time_incr = 0;

	put_bits(p, 16, 0);
	put_bits(p, 16, VOP_STARTCODE);

	put_bits(p, 2, vop->picture_type);
	
	
//	printf("not_code:%d vop_time: %d\n", vop_not_coded, vop->time);

//	printf("pic:%d icount:%d vop_time: %d\n", vop->picture_type, time_incr, vop->time);

/*	if (time_old > vop->time)
	{
		put_bits(p, 1, 1);
	}

	time_old = vop->time;
*/

	// !!!!!
	while (time_incr--)
		put_bits(p, 1, 1);
	put_bits(p, 1, 0);
		
	put_bits(p, 1, 1); // marker
	put_bits(p, time_bits, vop->time); // time_increment
	put_bits(p, 1, 1); // marker
	
	if (vop_not_coded)
	{
		put_bits(p, 1, 0); // vop coded
		return 0;
	}
	
	put_bits(p, 1, 1); // vop coded
	
	if (vop->picture_type == M4V_P_TYPE)
	{
		put_bits(p, 1, vop->rounding_type); // rounding type
	}
	
	put_bits(p, 3, 0); // intra dc VLC threashold
	
	put_bits(p, 5, vop->qscale); // qscale
	
	if (vop->picture_type != M4V_I_TYPE)
	{
		put_bits(p, 3, vop->f_code);
	}
	
	if (vop->picture_type == M4V_B_TYPE)
	{
		put_bits(p, 3, vop->b_code);
	}
	
	return 0;
}

static __inline int encode_gop_header(BW* bw, uint32 time_ms)
{
	int sec, min, hour;
	
	sec = time_ms / 1000;
	min = sec / 60; sec %= 60;
	hour = min / 60; min %= 60;
	hour %= 24;

	put_bits(bw, 16, 0);
	put_bits(bw, 16, GOP_STARTCODE);

	put_bits(bw, 5, hour);
	put_bits(bw, 6, min);
	put_bits(bw, 1, 1);
	put_bits(bw, 6, sec);
	
	put_bits(bw, 1, 0); // closed_gop == NO
	put_bits(bw, 1, 0); // broken link == NO
	
	printf("GOP %02d:%02d:%02d\n", hour, min, sec);
	
	m4v_stuffing(bw);
	return 0;
}

static int __inline encode_user_header(BW* p)
{
	put_bits(p, 16, 0);
	put_bits(p, 16, USERDATA_STARTCODE);

	put_bits(p, 8, 'v');
	put_bits(p, 8, 'i');
	put_bits(p, 8, 'x');
	put_bits(p, 8, 'y');
	put_bits(p, 8, '.');
	put_bits(p, 8, 'n');
	put_bits(p, 8, 'e');
	put_bits(p, 8, 't');
	
	m4v_stuffing(p);
	return 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////// 
void m4v_encode_m4v_header(BW* bw, M4V_VOL* vol, uint32 time)
{
	encode_vo_header(bw);
	encode_vol_header(bw, vol);
//	encode_gop_header(bw, time);
	encode_user_header(bw);
}

void m4v_encode_vop_header(BW* bw, M4V_VOP* vop, int time_bits, int vop_not_coded)
{
	encode_vop_header(bw, vop, time_bits, vop_not_coded);
}

void m4v_encode_I_mb(BW* bw, M4V_MICROBLOCK* mb)
{
	encode_mb_intra_internal(bw, mb, 1);
}

// same as FLV
void m4v_encode_P_mb(BW* bw, M4V_MICROBLOCK* mb)
{
	if (mb->skip)
	{
		put_bits(bw, 1, 1); // not coded
		return;
	}
	else
	{
		put_bits(bw, 1, 0); // coded
	}
		
	if (mb->intra)
	{
		encode_mb_intra_internal(bw, mb, 0);
	}
	else
	{
		encode_mb_inter_internal(bw, mb);
	}
}

int m4v_encode_I_dcpred(M4V_MICROBLOCK* mb, M4V_DCPRED* dcpred, int mb_x, int mb_y)
{
	int n;
	if (mb->intra)
	{
		dcpred_set_qscale(dcpred, mb->qscale);
		dcpred_set_pos(dcpred, mb_x, mb_y);
		
		for (n = 0; n < 6; n ++)
		{
			int level = dcpred_for_enc(dcpred, n, mb->block[n].block[0]);
			mb->block[n].block[0] = level;
		}
	}		
	return 0;
}
