/*****************************************************************************
 * bs.h :
 *****************************************************************************
 * Copyright (C) 2003-2008 x264 project
 *
 * Authors: Loren Merritt <lorenm@u.washington.edu>
 *          Jason Garrett-Glaser <darkshikari@gmail.com>
 *          Laurent Aimar <fenrir@via.ecp.fr>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111, USA.
 *****************************************************************************/

#ifndef X264_BS_H
#define X264_BS_H

/* some architectures cannot perform unaligned memory accesses at all. In such a 
   case the memory access has to be broken down in individual byte accesses */
#if defined(__sparc__) || defined(__sparc) || defined(sparc) || defined(__SPARC__)
#define ALIGNMENT_32BIT_MANDATORY 1
#endif

typedef struct
{
    uint8_t i_bits;
    uint8_t i_size;
} vlc_t;

typedef struct bs_s
{
    uint8_t *p_start;
    uint8_t *p;
    uint8_t *p_end;

    intptr_t cur_bits;
    int     i_left;    /* i_count number of available bits */
    int     i_bits_encoded; /* RD only */
} bs_t;

extern const vlc_t x264_coeff_token[5][17*4];
extern const vlc_t x264_total_zeros[15][16];
extern const vlc_t x264_total_zeros_dc[3][4];
extern const vlc_t x264_run_before[7][15];

static inline void bs_init( bs_t *s, void *p_data, int i_data )
{
    int offset = ((intptr_t)p_data & (WORD_SIZE-1));
    s->p       = s->p_start = (uint8_t*)p_data - offset;
    s->p_end   = (uint8_t*)p_data + i_data;
    s->i_left  = offset ? 8*offset : (WORD_SIZE*8);
    s->cur_bits = endian_fix( *(intptr_t*)s->p );
}
static inline int bs_pos( bs_t *s )
{
    return( 8 * (s->p - s->p_start) + (WORD_SIZE*8) - s->i_left );
}

/* Write the rest of cur_bits to the bitstream; results in a bitstream no longer 32/64-bit aligned. */
static inline void bs_flush( bs_t *s )
{
#ifdef ALIGNMENT_32BIT_MANDATORY
/* special case for all 32 bit architectures that require word aligned memory access. */
    int value = endian_fix( s->cur_bits << s->i_left );
    s->p[0] = (value >> 24) & 0xff;
    s->p[1] = (value >> 16) & 0xff;
    s->p[2] = (value >>  8) & 0xff;
    s->p[3] =  value        & 0xff;
#else
    *(intptr_t*)s->p = endian_fix( s->cur_bits << s->i_left );
#endif
    s->p += WORD_SIZE - s->i_left / 8;
    s->i_left = WORD_SIZE*8;
}

static inline void bs_write( bs_t *s, int i_count, uint32_t i_bits )
{
    if( WORD_SIZE == 8 )
    {
        s->cur_bits = (s->cur_bits << i_count) | i_bits;
        s->i_left -= i_count;
        if( s->i_left <= 32 )
        {
#ifdef WORDS_BIGENDIAN
            *(uint32_t*)s->p = s->cur_bits >> (32 - s->i_left);
#else
            *(uint32_t*)s->p = endian_fix( s->cur_bits << s->i_left );
#endif
            s->i_left += 32;
            s->p += 4;
        }
    }
    else
    {
        if( i_count < s->i_left )
        {
            s->cur_bits = (s->cur_bits << i_count) | i_bits;
            s->i_left -= i_count;
        }
        else
        {
            i_count -= s->i_left;
            s->cur_bits = (s->cur_bits << s->i_left) | (i_bits >> i_count);
#ifdef ALIGNMENT_32BIT_MANDATORY
/* special case for all architectures that require word aligned memory access */
	    int value = endian_fix( s->cur_bits );
	    s->p[0] = (value >> 24) & 0xff;
	    s->p[1] = (value >> 16) & 0xff;
	    s->p[2] = (value >>  8) & 0xff;
	    s->p[3] =  value        & 0xff;
#else
            *(uint32_t*)s->p = endian_fix( s->cur_bits );
#endif
            s->p += 4;
            s->cur_bits = i_bits;
            s->i_left = 32 - i_count;
        }
    }
}

/* Special case to eliminate branch in normal bs_write. */
/* Golomb never writes an even-size code, so this is only used in slice headers. */
static inline void bs_write32( bs_t *s, uint32_t i_bits )
{
    bs_write( s, 16, i_bits >> 16 );
    bs_write( s, 16, i_bits );
}

static inline void bs_write1( bs_t *s, uint32_t i_bit )
{
    s->cur_bits <<= 1;
    s->cur_bits |= i_bit;
    s->i_left--;
    if( s->i_left == WORD_SIZE*8-32 )
    {
#ifdef ALIGNMENT_32BIT_MANDATORY
/* special case for all architectures that require word aligned memory access */
	int value = endian_fix32( s->cur_bits );
	s->p[0] = (value >> 24) & 0xff;
	s->p[1] = (value >> 16) & 0xff;
	s->p[2] = (value >>  8) & 0xff;
	s->p[3] =  value        & 0xff;
#else
        *(uint32_t*)s->p = endian_fix32( s->cur_bits );
#endif
        s->p += 4;
        s->i_left = WORD_SIZE*8;
    }
}

static inline void bs_align_0( bs_t *s )
{
    if( s->i_left&7 )
    {
        s->cur_bits <<= s->i_left&7;
        s->i_left &= ~7;
    }
    bs_flush( s );
}
static inline void bs_align_1( bs_t *s )
{
    if( s->i_left&7 )
    {
        s->cur_bits <<= s->i_left&7;
        s->cur_bits |= (1 << (s->i_left&7)) - 1;
        s->i_left &= ~7;
    }
    bs_flush( s );
}

/* golomb functions */

static const uint8_t x264_ue_size_tab[256] =
{
     1, 1, 3, 3, 5, 5, 5, 5, 7, 7, 7, 7, 7, 7, 7, 7,
     9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
    11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,
    11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,
    13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,
    13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,
    13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,
    13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,
    15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,
    15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,
    15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,
    15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,
    15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,
    15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,
    15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,
    15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,
};

static inline void bs_write_ue_big( bs_t *s, unsigned int val )
{
    int size = 0;
    int tmp = ++val;
    if( tmp >= 0x10000 )
    {
        size = 32;
        tmp >>= 16;
    }
    if( tmp >= 0x100 )
    {
        size += 16;
        tmp >>= 8;
    }
    size += x264_ue_size_tab[tmp];
    bs_write( s, size>>1, 0 );
    bs_write( s, (size>>1)+1, val );
}

/* Only works on values under 255. */
static inline void bs_write_ue( bs_t *s, int val )
{
    bs_write( s, x264_ue_size_tab[val+1], val+1 );
}

static inline void bs_write_se( bs_t *s, int val )
{
    int size = 0;
    int tmp = val = val <= 0 ? -val*2+1 : val*2;
    if( tmp >= 0x100 )
    {
        size = 16;
        tmp >>= 8;
    }
    size += x264_ue_size_tab[tmp];
    bs_write( s, size, val );
}

static inline void bs_write_te( bs_t *s, int x, int val )
{
    if( x == 1 )
        bs_write1( s, 1^val );
    else if( x > 1 )
        bs_write_ue( s, val );
}

static inline void bs_rbsp_trailing( bs_t *s )
{
    bs_write1( s, 1 );
    bs_flush( s );
}

static inline int bs_size_ue( unsigned int val )
{
    return x264_ue_size_tab[val+1];
}

static inline int bs_size_ue_big( unsigned int val )
{
    if( val < 255 )
        return x264_ue_size_tab[val+1];
    else
        return x264_ue_size_tab[(val+1)>>8] + 16;
}

static inline int bs_size_se( int val )
{
    return bs_size_ue_big( val <= 0 ? -val*2 : val*2-1 );
}

static inline int bs_size_te( int x, int val )
{
    if( x == 1 )
        return 1;
    else if( x > 1 )
        return x264_ue_size_tab[val+1];
    else
        return 0;
}

#ifdef ALIGNMENT_32BIT_MANDATORY
#undef ALIGNMENT_32BIT_MANDATORY
#endif

#endif
