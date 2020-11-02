/*****************************************************************************
* quant.c: h264 encoder
*****************************************************************************
* Copyright (C) 2007 Guillaume Poirier <gpoirier@mplayerhq.hu>
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

#if defined SYS_LINUX
#include <altivec.h>
#endif

#include "common/common.h"
#include "ppccommon.h"
#include "quant.h"            

// quant of a whole 4x4 block, unrolled 2x and "pre-scheduled"
#define QUANT_16_U( idx0, idx1 )                                             \
temp1v = vec_ld((idx0), *dct);                                               \
temp2v = vec_ld((idx1), *dct);                                               \
mfvA = vec_ld((idx0), mf);                                                   \
mfvB = vec_ld((idx1), mf);                                                   \
biasvA = vec_ld((idx0), bias);                                               \
biasvB = vec_ld((idx1), bias);                                               \
mskA = vec_cmplt(temp1v, zerov);                                             \
mskB = vec_cmplt(temp2v, zerov);                                             \
coefvA = (vec_u16_t)vec_max(vec_sub(zerov, temp1v), temp1v);                 \
coefvB = (vec_u16_t)vec_max(vec_sub(zerov, temp2v), temp2v);                 \
coefvA = vec_adds(coefvA, biasvA);                                           \
coefvB = vec_adds(coefvB, biasvB);                                           \
multEvenvA = vec_mule(coefvA, mfvA);                                         \
multOddvA = vec_mulo(coefvA, mfvA);                                          \
multEvenvB = vec_mule(coefvB, mfvB);                                         \
multOddvB = vec_mulo(coefvB, mfvB);                                          \
multEvenvA = vec_sr(multEvenvA, i_qbitsv);                                   \
multOddvA = vec_sr(multOddvA, i_qbitsv);                                     \
multEvenvB = vec_sr(multEvenvB, i_qbitsv);                                   \
multOddvB = vec_sr(multOddvB, i_qbitsv);                                     \
temp1v = (vec_s16_t) vec_packs(vec_mergeh(multEvenvA, multOddvA), vec_mergel(multEvenvA, multOddvA)); \
temp2v = (vec_s16_t) vec_packs(vec_mergeh(multEvenvB, multOddvB), vec_mergel(multEvenvB, multOddvB)); \
temp1v = vec_xor(temp1v, mskA);                                              \
temp2v = vec_xor(temp2v, mskB);                                              \
temp1v = vec_adds(temp1v, vec_and(mskA, one));                               \
vec_st(temp1v, (idx0), (int16_t*)dct);                                       \
temp2v = vec_adds(temp2v, vec_and(mskB, one));                               \
vec_st(temp2v, (idx1), (int16_t*)dct);
                
void x264_quant_4x4_altivec( int16_t dct[4][4], uint16_t mf[16], uint16_t bias[16] )
{
    vector bool short mskA;
    vec_u32_t i_qbitsv;
    vec_u16_t coefvA;
    vec_u32_t multEvenvA, multOddvA;
    vec_u16_t mfvA;
    vec_u16_t biasvA;
    vec_s16_t zerov, one;

    vector bool short mskB;
    vec_u16_t coefvB;
    vec_u32_t multEvenvB, multOddvB;
    vec_u16_t mfvB;
    vec_u16_t biasvB;

    vec_s16_t temp1v, temp2v;

    vect_int_u qbits_u;
    qbits_u.s[0]=16;
    i_qbitsv = vec_splat(qbits_u.v, 0);

    zerov = vec_splat_s16(0);
    one = vec_splat_s16(1);

    QUANT_16_U( 0, 16 );
}

// DC quant of a whole 4x4 block, unrolled 2x and "pre-scheduled"
#define QUANT_16_U_DC( idx0, idx1 )                             \
temp1v = vec_ld((idx0), *dct);                                  \
temp2v = vec_ld((idx1), *dct);                                  \
mskA = vec_cmplt(temp1v, zerov);                                \
mskB = vec_cmplt(temp2v, zerov);                                \
coefvA = (vec_u16_t) vec_max(vec_sub(zerov, temp1v), temp1v);   \
coefvB = (vec_u16_t) vec_max(vec_sub(zerov, temp2v), temp2v);   \
coefvA = vec_add(coefvA, biasv);                                \
coefvB = vec_add(coefvB, biasv);                                \
multEvenvA = vec_mule(coefvA, mfv);                             \
multOddvA = vec_mulo(coefvA, mfv);                              \
multEvenvB = vec_mule(coefvB, mfv);                             \
multOddvB = vec_mulo(coefvB, mfv);                              \
multEvenvA = vec_sr(multEvenvA, i_qbitsv);                      \
multOddvA = vec_sr(multOddvA, i_qbitsv);                        \
multEvenvB = vec_sr(multEvenvB, i_qbitsv);                      \
multOddvB = vec_sr(multOddvB, i_qbitsv);                        \
temp1v = (vec_s16_t) vec_packs(vec_mergeh(multEvenvA, multOddvA), vec_mergel(multEvenvA, multOddvA)); \
temp2v = (vec_s16_t) vec_packs(vec_mergeh(multEvenvB, multOddvB), vec_mergel(multEvenvB, multOddvB)); \
temp1v = vec_xor(temp1v, mskA);                                 \
temp2v = vec_xor(temp2v, mskB);                                 \
temp1v = vec_add(temp1v, vec_and(mskA, one));                   \
vec_st(temp1v, (idx0), (int16_t*)dct);                          \
temp2v = vec_add(temp2v, vec_and(mskB, one));                   \
vec_st(temp2v, (idx1), (int16_t*)dct);

void x264_quant_4x4_dc_altivec( int16_t dct[4][4], int mf, int bias )
{
    vector bool short mskA;
    vec_u32_t i_qbitsv;
    vec_u16_t coefvA;
    vec_u32_t multEvenvA, multOddvA;
    vec_s16_t zerov, one;

    vector bool short mskB;
    vec_u16_t coefvB;
    vec_u32_t multEvenvB, multOddvB;

    vec_s16_t temp1v, temp2v;

    vec_u16_t mfv;
    vec_u16_t biasv;

    vect_ushort_u mf_u;
    mf_u.s[0]=mf;
    mfv = vec_splat( mf_u.v, 0 );

    vect_int_u qbits_u;
    qbits_u.s[0]=16;
    i_qbitsv = vec_splat(qbits_u.v, 0);

    vect_ushort_u bias_u;
    bias_u.s[0]=bias;
    biasv = vec_splat(bias_u.v, 0);

    zerov = vec_splat_s16(0);
    one = vec_splat_s16(1);

    QUANT_16_U_DC( 0, 16 );
}

// DC quant of a whole 2x2 block
#define QUANT_4_U_DC( idx0 )                                    \
const vec_u16_t sel = (vec_u16_t) CV(-1,-1,-1,-1,0,0,0,0);      \
temp1v = vec_ld((idx0), *dct);                                  \
mskA = vec_cmplt(temp1v, zerov);                                \
coefvA = (vec_u16_t) vec_max(vec_sub(zerov, temp1v), temp1v);   \
coefvA = vec_add(coefvA, biasv);                                \
multEvenvA = vec_mule(coefvA, mfv);                             \
multOddvA = vec_mulo(coefvA, mfv);                              \
multEvenvA = vec_sr(multEvenvA, i_qbitsv);                      \
multOddvA = vec_sr(multOddvA, i_qbitsv);                        \
temp2v = (vec_s16_t) vec_packs(vec_mergeh(multEvenvA, multOddvA), vec_mergel(multEvenvA, multOddvA)); \
temp2v = vec_xor(temp2v, mskA);                                 \
temp2v = vec_add(temp2v, vec_and(mskA, one));                   \
temp1v = vec_sel(temp1v, temp2v, sel);                          \
vec_st(temp1v, (idx0), (int16_t*)dct);

void x264_quant_2x2_dc_altivec( int16_t dct[2][2], int mf, int bias )
{
    vector bool short mskA;
    vec_u32_t i_qbitsv;
    vec_u16_t coefvA;
    vec_u32_t multEvenvA, multOddvA;
    vec_s16_t zerov, one;

    vec_s16_t temp1v, temp2v;

    vec_u16_t mfv;
    vec_u16_t biasv;

    vect_ushort_u mf_u;
    mf_u.s[0]=mf;
    mfv = vec_splat( mf_u.v, 0 );

    vect_int_u qbits_u;
    qbits_u.s[0]=16;
    i_qbitsv = vec_splat(qbits_u.v, 0);

    vect_ushort_u bias_u;
    bias_u.s[0]=bias;
    biasv = vec_splat(bias_u.v, 0);

    zerov = vec_splat_s16(0);
    one = vec_splat_s16(1);

    QUANT_4_U_DC(0);
}

void x264_quant_8x8_altivec( int16_t dct[8][8], uint16_t mf[64], uint16_t bias[64] )
{
    vector bool short mskA;
    vec_u32_t i_qbitsv;
    vec_u16_t coefvA;
    vec_u32_t multEvenvA, multOddvA;
    vec_u16_t mfvA;
    vec_u16_t biasvA;
    vec_s16_t zerov, one;
    
    vector bool short mskB;
    vec_u16_t coefvB;
    vec_u32_t multEvenvB, multOddvB;
    vec_u16_t mfvB;
    vec_u16_t biasvB;
    
    vec_s16_t temp1v, temp2v;
    
    vect_int_u qbits_u;
    qbits_u.s[0]=16;
    i_qbitsv = vec_splat(qbits_u.v, 0);

    zerov = vec_splat_s16(0);
    one = vec_splat_s16(1);
    
    int i;

    for ( i=0; i<4; i++ ) {
      QUANT_16_U( i*2*16, i*2*16+16 );
    }
}

#define DEQUANT_SHL()                                                \
{                                                                    \
    dctv = vec_ld(0, dct[y]);                                        \
    mf1v = vec_ld(0, dequant_mf[i_mf][y]);                           \
    mf2v = vec_ld(16, dequant_mf[i_mf][y]);                          \
    mfv  = vec_packs(mf1v, mf2v);                                    \
                                                                     \
    multEvenvA = vec_mule(dctv, mfv);                                \
    multOddvA = vec_mulo(dctv, mfv);                                 \
    dctv = (vec_s16_t) vec_packs(vec_mergeh(multEvenvA, multOddvA),  \
                                 vec_mergel(multEvenvA, multOddvA)); \
    dctv = vec_sl(dctv, i_qbitsv);                                   \
    vec_st(dctv, 0, dct[y]);                                         \
}

#define DEQUANT_SHR()                                          \
{                                                              \
    dctv = vec_ld(0, dct[y]);                                  \
    dct1v = vec_mergeh(dctv, dctv);                            \
    dct2v = vec_mergel(dctv, dctv);                            \
    mf1v = vec_ld(0, dequant_mf[i_mf][y]);                     \
    mf2v = vec_ld(16, dequant_mf[i_mf][y]);                    \
                                                               \
    multEvenvA = vec_mule(dct1v, (vec_s16_t)mf1v);             \
    multOddvA = vec_mulo(dct1v, (vec_s16_t)mf1v);              \
    temp1v = vec_add(vec_sl(multEvenvA, sixteenv), multOddvA); \
    temp1v = vec_add(temp1v, fv);                              \
    temp1v = vec_sra(temp1v, i_qbitsv);                        \
                                                               \
    multEvenvA = vec_mule(dct2v, (vec_s16_t)mf2v);             \
    multOddvA = vec_mulo(dct2v, (vec_s16_t)mf2v);              \
    temp2v = vec_add(vec_sl(multEvenvA, sixteenv), multOddvA); \
    temp2v = vec_add(temp2v, fv);                              \
    temp2v = vec_sra(temp2v, i_qbitsv);                        \
                                                               \
    dctv = (vec_s16_t)vec_packs(temp1v, temp2v);               \
    vec_st(dctv, 0, dct[y]);                                   \
}

void x264_dequant_4x4_altivec( int16_t dct[4][4], int dequant_mf[6][4][4], int i_qp )
{
    const int i_mf = i_qp%6;
    const int i_qbits = i_qp/6 - 4;
    int y;

    vec_s16_t dctv;
    vec_s16_t dct1v, dct2v;
    vec_s32_t mf1v, mf2v;
    vec_s16_t mfv;
    vec_s32_t multEvenvA, multOddvA;
    vec_s32_t temp1v, temp2v;

    if( i_qbits >= 0 )
    {
        vec_u16_t i_qbitsv;
        vect_ushort_u qbits_u;
        qbits_u.s[0]=i_qbits;
        i_qbitsv = vec_splat(qbits_u.v, 0);

        for( y = 0; y < 4; y+=2 )
            DEQUANT_SHL();
    }
    else
    {
        const int f = 1 << (-i_qbits-1);

        vec_s32_t fv;
        vect_int_u f_u;
        f_u.s[0]=f;
        fv = (vec_s32_t)vec_splat(f_u.v, 0);

        vec_u32_t i_qbitsv;
        vect_int_u qbits_u;
        qbits_u.s[0]=-i_qbits;
        i_qbitsv = vec_splat(qbits_u.v, 0);

        vec_u32_t sixteenv;
        vect_int_u sixteen_u;
        sixteen_u.s[0]=16;
        sixteenv = vec_splat(sixteen_u.v, 0);

        for( y = 0; y < 4; y+=2 )
            DEQUANT_SHR();
    }
}

void x264_dequant_8x8_altivec( int16_t dct[8][8], int dequant_mf[6][8][8], int i_qp )
{
    const int i_mf = i_qp%6;
    const int i_qbits = i_qp/6 - 6;
    int y;

    vec_s16_t dctv;
    vec_s16_t dct1v, dct2v;
    vec_s32_t mf1v, mf2v;
    vec_s16_t mfv;
    vec_s32_t multEvenvA, multOddvA;
    vec_s32_t temp1v, temp2v;

    if( i_qbits >= 0 )
    {
        vec_u16_t i_qbitsv;
        vect_ushort_u qbits_u;
        qbits_u.s[0]=i_qbits;
        i_qbitsv = vec_splat(qbits_u.v, 0);

        for( y = 0; y < 8; y++ )
            DEQUANT_SHL();
    }
    else
    {
        const int f = 1 << (-i_qbits-1);

        vec_s32_t fv;
        vect_int_u f_u;
        f_u.s[0]=f;
        fv = (vec_s32_t)vec_splat(f_u.v, 0);

        vec_u32_t i_qbitsv;
        vect_int_u qbits_u;
        qbits_u.s[0]=-i_qbits;
        i_qbitsv = vec_splat(qbits_u.v, 0);

        vec_u32_t sixteenv;
        vect_int_u sixteen_u;
        sixteen_u.s[0]=16;
        sixteenv = vec_splat(sixteen_u.v, 0);

        for( y = 0; y < 8; y++ )
            DEQUANT_SHR();
    }
}

