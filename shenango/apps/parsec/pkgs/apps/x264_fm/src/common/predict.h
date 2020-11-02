/*****************************************************************************
 * predict.h: h264 encoder library
 *****************************************************************************
 * Copyright (C) 2003-2008 x264 project
 *
 * Authors: Loren Merritt <lorenm@u.washington.edu>
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

#ifndef X264_PREDICT_H
#define X264_PREDICT_H

typedef void (*x264_predict_t)( uint8_t *src );
typedef void (*x264_predict8x8_t)( uint8_t *src, uint8_t edge[33] );

enum intra_chroma_pred_e
{
    I_PRED_CHROMA_DC = 0,
    I_PRED_CHROMA_H  = 1,
    I_PRED_CHROMA_V  = 2,
    I_PRED_CHROMA_P  = 3,

    I_PRED_CHROMA_DC_LEFT = 4,
    I_PRED_CHROMA_DC_TOP  = 5,
    I_PRED_CHROMA_DC_128  = 6
};
static const uint8_t x264_mb_pred_mode8x8c_fix[7] =
{
    I_PRED_CHROMA_DC, I_PRED_CHROMA_H, I_PRED_CHROMA_V, I_PRED_CHROMA_P,
    I_PRED_CHROMA_DC, I_PRED_CHROMA_DC,I_PRED_CHROMA_DC
};

enum intra16x16_pred_e
{
    I_PRED_16x16_V  = 0,
    I_PRED_16x16_H  = 1,
    I_PRED_16x16_DC = 2,
    I_PRED_16x16_P  = 3,

    I_PRED_16x16_DC_LEFT = 4,
    I_PRED_16x16_DC_TOP  = 5,
    I_PRED_16x16_DC_128  = 6,
};
static const uint8_t x264_mb_pred_mode16x16_fix[7] =
{
    I_PRED_16x16_V, I_PRED_16x16_H, I_PRED_16x16_DC, I_PRED_16x16_P,
    I_PRED_16x16_DC,I_PRED_16x16_DC,I_PRED_16x16_DC
};

enum intra4x4_pred_e
{
    I_PRED_4x4_V  = 0,
    I_PRED_4x4_H  = 1,
    I_PRED_4x4_DC = 2,
    I_PRED_4x4_DDL= 3,
    I_PRED_4x4_DDR= 4,
    I_PRED_4x4_VR = 5,
    I_PRED_4x4_HD = 6,
    I_PRED_4x4_VL = 7,
    I_PRED_4x4_HU = 8,

    I_PRED_4x4_DC_LEFT = 9,
    I_PRED_4x4_DC_TOP  = 10,
    I_PRED_4x4_DC_128  = 11,
};
static const int8_t x264_mb_pred_mode4x4_fix[13] =
{
    -1,
    I_PRED_4x4_V,   I_PRED_4x4_H,   I_PRED_4x4_DC,
    I_PRED_4x4_DDL, I_PRED_4x4_DDR, I_PRED_4x4_VR,
    I_PRED_4x4_HD,  I_PRED_4x4_VL,  I_PRED_4x4_HU,
    I_PRED_4x4_DC,  I_PRED_4x4_DC,  I_PRED_4x4_DC
};
#define x264_mb_pred_mode4x4_fix(t) x264_mb_pred_mode4x4_fix[(t)+1]

/* must use the same numbering as intra4x4_pred_e */
enum intra8x8_pred_e
{
    I_PRED_8x8_V  = 0,
    I_PRED_8x8_H  = 1,
    I_PRED_8x8_DC = 2,
    I_PRED_8x8_DDL= 3,
    I_PRED_8x8_DDR= 4,
    I_PRED_8x8_VR = 5,
    I_PRED_8x8_HD = 6,
    I_PRED_8x8_VL = 7,
    I_PRED_8x8_HU = 8,

    I_PRED_8x8_DC_LEFT = 9,
    I_PRED_8x8_DC_TOP  = 10,
    I_PRED_8x8_DC_128  = 11,
};

// FIXME enforce edge alignment via uint64_t ?
void x264_predict_8x8_filter( uint8_t *src, uint8_t edge[33], int i_neighbor, int i_filters );

void x264_predict_16x16_init ( int cpu, x264_predict_t pf[7] );
void x264_predict_8x8c_init  ( int cpu, x264_predict_t pf[7] );
void x264_predict_4x4_init   ( int cpu, x264_predict_t pf[12] );
void x264_predict_8x8_init   ( int cpu, x264_predict8x8_t pf[12] );


#endif
