;*****************************************************************************
;* quant-a.asm: h264 encoder library
;*****************************************************************************
;* Copyright (C) 2005-2008 x264 project
;*
;* Authors: Loren Merritt <lorenm@u.washington.edu>
;*          Jason Garrett-Glaser <darkshikari@gmail.com>
;*          Christian Heine <sennindemokrit@gmx.net>
;*
;* This program is free software; you can redistribute it and/or modify
;* it under the terms of the GNU General Public License as published by
;* the Free Software Foundation; either version 2 of the License, or
;* (at your option) any later version.
;*
;* This program is distributed in the hope that it will be useful,
;* but WITHOUT ANY WARRANTY; without even the implied warranty of
;* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;* GNU General Public License for more details.
;*
;* You should have received a copy of the GNU General Public License
;* along with this program; if not, write to the Free Software
;* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111, USA.
;*****************************************************************************

%include "x86inc.asm"
%include "x86util.asm"

SECTION_RODATA
pb_1:     times 16 db 1
pw_1:     times 8 dw 1
pd_1:     times 4 dd 1

%macro DQM4 3
    dw %1, %2, %1, %2, %2, %3, %2, %3
%endmacro
%macro DQM8 6
    dw %1, %4, %5, %4, %1, %4, %5, %4
    dw %4, %2, %6, %2, %4, %2, %6, %2
    dw %5, %6, %3, %6, %5, %6, %3, %6
    ; last line not used, just padding for power-of-2 stride
    times 8 dw 0
%endmacro

dequant4_scale:
    DQM4 10, 13, 16
    DQM4 11, 14, 18
    DQM4 13, 16, 20
    DQM4 14, 18, 23
    DQM4 16, 20, 25
    DQM4 18, 23, 29

dequant8_scale:
    DQM8 20, 18, 32, 19, 25, 24
    DQM8 22, 19, 35, 21, 28, 26
    DQM8 26, 23, 42, 24, 33, 31
    DQM8 28, 25, 45, 26, 35, 33
    DQM8 32, 28, 51, 30, 40, 38
    DQM8 36, 32, 58, 34, 46, 43

decimate_mask_table4:
    db  0,3,2,6,2,5,5,9,1,5,4,8,5,8,8,12,1,4,4,8,4,7,7,11,4,8,7,11,8,11,11,15,1,4
    db  3,7,4,7,7,11,3,7,6,10,7,10,10,14,4,7,7,11,7,10,10,14,7,11,10,14,11,14,14
    db 18,0,4,3,7,3,6,6,10,3,7,6,10,7,10,10,14,3,6,6,10,6,9,9,13,6,10,9,13,10,13
    db 13,17,4,7,6,10,7,10,10,14,6,10,9,13,10,13,13,17,7,10,10,14,10,13,13,17,10
    db 14,13,17,14,17,17,21,0,3,3,7,3,6,6,10,2,6,5,9,6,9,9,13,3,6,6,10,6,9,9,13
    db  6,10,9,13,10,13,13,17,3,6,5,9,6,9,9,13,5,9,8,12,9,12,12,16,6,9,9,13,9,12
    db 12,16,9,13,12,16,13,16,16,20,3,7,6,10,6,9,9,13,6,10,9,13,10,13,13,17,6,9
    db  9,13,9,12,12,16,9,13,12,16,13,16,16,20,7,10,9,13,10,13,13,17,9,13,12,16
    db 13,16,16,20,10,13,13,17,13,16,16,20,13,17,16,20,17,20,20,24

SECTION .text

%macro QUANT_DC_START 0
    movd       m6, r1m     ; mf
    movd       m7, r2m     ; bias
%ifidn m0, mm0
    pshufw     m6, m6, 0
    pshufw     m7, m7, 0
%else
    pshuflw    m6, m6, 0
    pshuflw    m7, m7, 0
    punpcklqdq m6, m6
    punpcklqdq m7, m7
%endif
%endmacro

%macro PABSW_MMX 2
    pxor       %1, %1
    pcmpgtw    %1, %2
    pxor       %2, %1
    psubw      %2, %1
    SWAP       %1, %2
%endmacro

%macro PSIGNW_MMX 2
    pxor       %1, %2
    psubw      %1, %2
%endmacro

%macro PABSW_SSSE3 2
    pabsw      %1, %2
%endmacro

%macro PSIGNW_SSSE3 2
    psignw     %1, %2
%endmacro

%macro QUANT_ONE 3
;;; %1      (m64)       dct[y][x]
;;; %2      (m64/mmx)   mf[y][x] or mf[0][0] (as uint16_t)
;;; %3      (m64/mmx)   bias[y][x] or bias[0][0] (as uint16_t)
    mova       m1, %1   ; load dct coeffs
    PABSW      m0, m1
    paddusw    m0, %3   ; round
    pmulhuw    m0, %2   ; divide
    PSIGNW     m0, m1   ; restore sign
    mova       %1, m0   ; store
%endmacro

;-----------------------------------------------------------------------------
; void x264_quant_4x4_dc_mmxext( int16_t dct[16], int mf, int bias )
;-----------------------------------------------------------------------------
%macro QUANT_DC 2
cglobal %1, 1,1
    QUANT_DC_START
%assign x 0
%rep %2
    QUANT_ONE [r0+x], m6, m7
%assign x x+mmsize
%endrep
    RET
%endmacro

;-----------------------------------------------------------------------------
; void x264_quant_4x4_mmx( int16_t dct[16], uint16_t mf[16], uint16_t bias[16] )
;-----------------------------------------------------------------------------
%macro QUANT_AC 2
cglobal %1, 3,3
%assign x 0
%rep %2
    QUANT_ONE [r0+x], [r1+x], [r2+x]
%assign x x+mmsize
%endrep
    RET
%endmacro

INIT_MMX
%define PABSW PABSW_MMX
%define PSIGNW PSIGNW_MMX
QUANT_DC x264_quant_2x2_dc_mmxext, 1
%ifndef ARCH_X86_64 ; not needed because sse2 is faster
QUANT_DC x264_quant_4x4_dc_mmxext, 4
QUANT_AC x264_quant_4x4_mmx, 4
QUANT_AC x264_quant_8x8_mmx, 16
%endif

INIT_XMM
QUANT_DC x264_quant_4x4_dc_sse2, 2
QUANT_AC x264_quant_4x4_sse2, 2
QUANT_AC x264_quant_8x8_sse2, 8

%define PABSW PABSW_SSSE3
%define PSIGNW PSIGNW_SSSE3
QUANT_DC x264_quant_4x4_dc_ssse3, 2
QUANT_AC x264_quant_4x4_ssse3, 2
QUANT_AC x264_quant_8x8_ssse3, 8

INIT_MMX
QUANT_DC x264_quant_2x2_dc_ssse3, 1



;=============================================================================
; dequant
;=============================================================================

%macro DEQUANT16_L 3
;;; %1      dct[y][x]
;;; %2,%3   dequant_mf[i_mf][y][x]
;;; m5      i_qbits

    mova     m0, %2
    packssdw m0, %3
    pmullw   m0, %1
    psllw    m0, m5
    mova     %1, m0
%endmacro

%macro DEQUANT32_R 3
;;; %1      dct[y][x]
;;; %2,%3   dequant_mf[i_mf][y][x]
;;; m5      -i_qbits
;;; m6      f
;;; m7      0

    mova      m0, %1
    mova      m1, m0
    punpcklwd m0, m7
    punpckhwd m1, m7
    pmaddwd   m0, %2
    pmaddwd   m1, %3
    paddd     m0, m6
    paddd     m1, m6
    psrad     m0, m5
    psrad     m1, m5
    packssdw  m0, m1
    mova      %1, m0
%endmacro

%macro DEQUANT_LOOP 3
%if 8*(%2-2*%3)
    mov t0d, 8*(%2-2*%3)
%%loop:
    %1 [r0+t0+8*%3], [r1+t0*2+16*%3], [r1+t0*2+24*%3]
    %1 [r0+t0     ], [r1+t0*2      ], [r1+t0*2+ 8*%3]
    sub t0d, 16*%3
    jge %%loop
    rep ret
%else
    %1 [r0+8*%3], [r1+16*%3], [r1+24*%3]
    %1 [r0     ], [r1      ], [r1+ 8*%3]
    ret
%endif
%endmacro

%macro DEQUANT16_FLAT 2-8
    mova   m0, %1
%assign i %0-2
%rep %0-1
%if i
    mova   m %+ i, [r0+%2]
    pmullw m %+ i, m0
%else
    pmullw m0, [r0+%2]
%endif
    psllw  m %+ i, m7
    mova   [r0+%2], m %+ i
    %assign i i-1
    %rotate 1
%endrep
%endmacro

%ifdef ARCH_X86_64
    %define t0  r4
    %define t0d r4d
    %define t1  r3
    %define t1d r3d
    %define t2  r2
    %define t2d r2d
%else
    %define t0  r2
    %define t0d r2d
    %define t1  r0
    %define t1d r0d
    %define t2  r1
    %define t2d r1d
%endif

%macro DEQUANT_START 2
    movifnidn t2d, r2m
    imul t0d, t2d, 0x2b
    shr  t0d, 8     ; i_qbits = i_qp / 6
    lea  t1, [t0*3]
    sub  t2d, t1d
    sub  t2d, t1d   ; i_mf = i_qp % 6
    shl  t2d, %1
%ifdef ARCH_X86_64
    add  r1, t2     ; dequant_mf[i_mf]
%else
    add  r1, r1m    ; dequant_mf[i_mf]
    mov  r0, r0m    ; dct
%endif
    sub  t0d, %2
    jl   .rshift32  ; negative qbits => rightshift
%endmacro

;-----------------------------------------------------------------------------
; void x264_dequant_4x4_mmx( int16_t dct[4][4], int dequant_mf[6][4][4], int i_qp )
;-----------------------------------------------------------------------------
%macro DEQUANT 4
cglobal x264_dequant_%2x%2_%1, 0,3
    DEQUANT_START %3+2, %3

.lshift:
    movd m5, t0d
    DEQUANT_LOOP DEQUANT16_L, %2*%2/4, %4

.rshift32:
    neg   t0d
    movd  m5, t0d
    mova  m6, [pd_1 GLOBAL]
    pxor  m7, m7
    pslld m6, m5
    psrld m6, 1
    DEQUANT_LOOP DEQUANT32_R, %2*%2/4, %4

cglobal x264_dequant_%2x%2_flat16_%1, 0,3
    movifnidn t2d, r2m
%if %2 == 8
    cmp  t2d, 12
    jl x264_dequant_%2x%2_%1
    sub  t2d, 12
%endif
    imul t0d, t2d, 0x2b
    shr  t0d, 8     ; i_qbits = i_qp / 6
    lea  t1, [t0*3]
    sub  t2d, t1d
    sub  t2d, t1d   ; i_mf = i_qp % 6
    shl  t2d, %3
%ifdef PIC
    lea  r1, [dequant%2_scale GLOBAL]
    add  r1, t2
%else
    lea  r1, [dequant%2_scale + t2 GLOBAL]
%endif
    movifnidn r0d, r0m
    movd m7, t0d
%if %2 == 4
%ifidn %1, mmx
    DEQUANT16_FLAT [r1], 0, 16
    DEQUANT16_FLAT [r1+8], 8, 24
%else
    DEQUANT16_FLAT [r1], 0, 16
%endif
%elifidn %1, mmx
    DEQUANT16_FLAT [r1], 0, 8, 64, 72
    DEQUANT16_FLAT [r1+16], 16, 24, 48, 56
    DEQUANT16_FLAT [r1+16], 80, 88, 112, 120
    DEQUANT16_FLAT [r1+32], 32, 40, 96, 104
%else
    DEQUANT16_FLAT [r1], 0, 64
    DEQUANT16_FLAT [r1+16], 16, 48, 80, 112
    DEQUANT16_FLAT [r1+32], 32, 96
%endif
    ret
%endmacro ; DEQUANT

%ifndef ARCH_X86_64
INIT_MMX
DEQUANT mmx, 4, 4, 1
DEQUANT mmx, 8, 6, 1
%endif
INIT_XMM
DEQUANT sse2, 4, 4, 2
DEQUANT sse2, 8, 6, 2

%macro DEQUANT_DC 1
cglobal x264_dequant_4x4dc_%1, 0,3
    DEQUANT_START 6, 6

.lshift:
    movd   m6, [r1]
    movd   m5, t0d
    pslld  m6, m5
%if mmsize==16
    pshuflw  m6, m6, 0
    punpcklqdq m6, m6
%else
    pshufw   m6, m6, 0
%endif
%assign x 0
%rep 16/mmsize
    mova     m0, [r0+mmsize*0+x]
    mova     m1, [r0+mmsize*1+x]
    pmullw   m0, m6
    pmullw   m1, m6
    mova     [r0+mmsize*0+x], m0
    mova     [r0+mmsize*1+x], m1
%assign x x+mmsize*2
%endrep
    RET

.rshift32:
    neg   t0d
    movd  m5, t0d
    mova  m6, [pw_1 GLOBAL]
    mova  m7, m6
    pslld m6, m5
    psrld m6, 1
    movd  m4, [r1]
%if mmsize==8
    punpcklwd m4, m4
%else
    pshuflw m4, m4, 0
%endif
    punpcklwd m4, m6
%assign x 0
%rep 32/mmsize
    mova      m0, [r0+x]
    mova      m1, m0
    punpcklwd m0, m7
    punpckhwd m1, m7
    pmaddwd   m0, m4
    pmaddwd   m1, m4
    psrad     m0, m5
    psrad     m1, m5
    packssdw  m0, m1
    mova      [r0+x], m0
%assign x x+mmsize
%endrep
    RET
%endmacro

INIT_MMX
DEQUANT_DC mmxext
INIT_XMM
DEQUANT_DC sse2

;-----------------------------------------------------------------------------
; void x264_denoise_dct_mmx( int16_t *dct, uint32_t *sum, uint16_t *offset, int size )
;-----------------------------------------------------------------------------
%macro DENOISE_DCT 1
cglobal x264_denoise_dct_%1, 4,5
    movzx     r4d, word [r0] ; backup DC coefficient
    pxor      m7, m7
.loop:
    sub       r3, mmsize
    mova      m2, [r0+r3*2+0*mmsize]
    mova      m3, [r0+r3*2+1*mmsize]
    PABSW     m0, m2
    PABSW     m1, m3
    mova      m4, m0
    mova      m5, m1
    psubusw   m0, [r2+r3*2+0*mmsize]
    psubusw   m1, [r2+r3*2+1*mmsize]
    PSIGNW    m0, m2
    PSIGNW    m1, m3
    mova      [r0+r3*2+0*mmsize], m0
    mova      [r0+r3*2+1*mmsize], m1
    mova      m2, m4
    mova      m3, m5
    punpcklwd m4, m7
    punpckhwd m2, m7
    punpcklwd m5, m7
    punpckhwd m3, m7
    paddd     m4, [r1+r3*4+0*mmsize]
    paddd     m2, [r1+r3*4+1*mmsize]
    paddd     m5, [r1+r3*4+2*mmsize]
    paddd     m3, [r1+r3*4+3*mmsize]
    mova      [r1+r3*4+0*mmsize], m4
    mova      [r1+r3*4+1*mmsize], m2
    mova      [r1+r3*4+2*mmsize], m5
    mova      [r1+r3*4+3*mmsize], m3
    jg .loop
    mov       [r0], r4w ; restore DC coefficient
    RET
%endmacro

%define PABSW PABSW_MMX
%define PSIGNW PSIGNW_MMX
%ifndef ARCH_X86_64
INIT_MMX
DENOISE_DCT mmx
%endif
INIT_XMM
DENOISE_DCT sse2
%define PABSW PABSW_SSSE3
%define PSIGNW PSIGNW_SSSE3
DENOISE_DCT ssse3



;-----------------------------------------------------------------------------
; int x264_decimate_score( int16_t *dct )
;-----------------------------------------------------------------------------

%macro DECIMATE_MASK_SSE2 6
%ifidn %5, ssse3
    pabsw    xmm0, [%3+ 0]
    pabsw    xmm1, [%3+16]
%else
    movdqa   xmm0, [%3+ 0]
    movdqa   xmm1, [%3+16]
    ABS2_MMX xmm0, xmm1, xmm3, xmm4
%endif
    packsswb xmm0, xmm1
    pxor     xmm2, xmm2
    pcmpeqb  xmm2, xmm0
    pcmpgtb  xmm0, %4
    pmovmskb %1, xmm2
    pmovmskb %2, xmm0
%endmacro

%macro DECIMATE_MASK_MMX 6
    movq      mm0, [%3+ 0]
    movq      mm1, [%3+ 8]
    movq      mm2, [%3+16]
    movq      mm3, [%3+24]
    ABS2_MMX  mm0, mm1, mm4, mm5
    ABS2_MMX  mm2, mm3, mm4, mm5
    packsswb  mm0, mm1
    packsswb  mm2, mm3
    pxor      mm4, mm4
    pxor      mm5, mm5
    pcmpeqb   mm4, mm0
    pcmpeqb   mm5, mm2
    pcmpgtb   mm0, %4
    pcmpgtb   mm2, %4
    pmovmskb   %6, mm4
    pmovmskb   %1, mm5
    shl        %1, 8
    or         %1, %6
    pmovmskb   %6, mm0
    pmovmskb   %2, mm2
    shl        %2, 8
    or         %2, %6
%endmacro

cextern x264_decimate_table4
cextern x264_decimate_table8

%macro DECIMATE4x4 2

;A LUT is faster than bsf on AMD processors, and no slower on Intel
;This is not true for score64.
cglobal x264_decimate_score%1_%2, 1,3
%ifdef PIC
    lea r10, [x264_decimate_table4 GLOBAL]
    lea r11, [decimate_mask_table4 GLOBAL]
    %define table r10
    %define mask_table r11
%else
    %define table x264_decimate_table4
    %define mask_table decimate_mask_table4
%endif
    DECIMATE_MASK edx, eax, r0, [pb_1 GLOBAL], %2, ecx
    xor   edx, 0xffff
    je   .ret
    test  eax, eax
    jne  .ret9
%if %1==15
    shr   edx, 1
%endif
    movzx ecx, dl
    movzx eax, byte [mask_table + rcx]
    cmp   edx, ecx
    je   .ret
    bsr   ecx, ecx
    shr   edx, 1
    shr   edx, cl
    bsf   ecx, edx
    shr   edx, 1
    shr   edx, cl
    add    al, byte [table + rcx]
    add    al, byte [mask_table + rdx]
.ret:
    REP_RET
.ret9:
    mov   eax, 9
    RET

%endmacro

%ifndef ARCH_X86_64
%define DECIMATE_MASK DECIMATE_MASK_MMX
DECIMATE4x4 15, mmxext
DECIMATE4x4 16, mmxext
%endif
%define DECIMATE_MASK DECIMATE_MASK_SSE2
DECIMATE4x4 15, sse2
DECIMATE4x4 15, ssse3
DECIMATE4x4 16, sse2
DECIMATE4x4 16, ssse3

%macro DECIMATE8x8 1

%ifdef ARCH_X86_64
cglobal x264_decimate_score64_%1, 1,4
%ifdef PIC
    lea r10, [x264_decimate_table8 GLOBAL]
    %define table r10
%else
    %define table x264_decimate_table8
%endif
    mova  m7, [pb_1 GLOBAL]
    DECIMATE_MASK r1d, eax, r0, m7, %1, null
    test  eax, eax
    jne  .ret9
    DECIMATE_MASK r2d, eax, r0+32, m7, %1, null
    shl   r2d, 16
    or    r1d, r2d
    DECIMATE_MASK r2d, r3d, r0+64, m7, %1, null
    shl   r2, 32
    or    eax, r3d
    or    r1, r2
    DECIMATE_MASK r2d, r3d, r0+96, m7, %1, null
    shl   r2, 48
    or    r1, r2
    xor   r1, -1
    je   .ret
    or    eax, r3d
    jne  .ret9
.loop:
    bsf   rcx, r1
    shr   r1, cl
    add   al, byte [table + rcx]
    shr   r1, 1
    jne  .loop
.ret:
    REP_RET
.ret9:
    mov   eax, 9
    RET

%else ; ARCH
%ifidn %1, mmxext
cglobal x264_decimate_score64_%1, 1,6
%else
cglobal x264_decimate_score64_%1, 1,5
%endif
    mova  m7, [pb_1 GLOBAL]
    DECIMATE_MASK r3, r2, r0, m7, %1, r5
    test  r2, r2
    jne  .ret9
    DECIMATE_MASK r4, r2, r0+32, m7, %1, r5
    shl   r4, 16
    or    r3, r4
    DECIMATE_MASK r4, r1, r0+64, m7, %1, r5
    or    r2, r1
    DECIMATE_MASK r1, r0, r0+96, m7, %1, r5
    shl   r1, 16
    or    r4, r1
    xor   r3, -1
    je   .tryret
    xor   r4, -1
.cont:
    or    r0, r2
    jne  .ret9      ;r0 is zero at this point, so we don't need to zero it
.loop:
    bsf   ecx, r3
    test  r3, r3
    je   .largerun
    shrd  r3, r4, cl
    shr   r4, cl
    add   r0b, byte [x264_decimate_table8 + ecx]
    shrd  r3, r4, 1
    shr   r4, 1
    cmp   r0, 6     ;score64's threshold is never higher than 6
    jge  .ret9      ;this early termination is only useful on 32-bit because it can be done in the latency after shrd
    test  r3, r3
    jne  .loop
    test  r4, r4
    jne  .loop
.ret:
    REP_RET
.tryret:
    xor   r4, -1
    jne  .cont
    REP_RET
.ret9:
    mov   eax, 9
    RET
.largerun:
    mov   r3, r4
    xor   r4, r4
    bsf   ecx, r3
    shr   r3, cl
    shr   r3, 1
    jne  .loop
    REP_RET
%endif ; ARCH

%endmacro

%ifndef ARCH_X86_64
INIT_MMX
%define DECIMATE_MASK DECIMATE_MASK_MMX
DECIMATE8x8 mmxext
%endif
INIT_XMM
%define DECIMATE_MASK DECIMATE_MASK_SSE2
DECIMATE8x8 sse2
DECIMATE8x8 ssse3

%macro LAST_MASK_SSE2 2-3
    movdqa   xmm0, [%2+ 0]
    pxor     xmm2, xmm2
    packsswb xmm0, [%2+16]
    pcmpeqb  xmm0, xmm2
    pmovmskb   %1, xmm0
%endmacro

%macro LAST_MASK_MMX 3
    movq     mm0, [%2+ 0]
    movq     mm1, [%2+16]
    pxor     mm2, mm2
    packsswb mm0, [%2+ 8]
    packsswb mm1, [%2+24]
    pcmpeqb  mm0, mm2
    pcmpeqb  mm1, mm2
    pmovmskb  %1, mm0
    pmovmskb  %3, mm1
    shl       %3, 8
    or        %1, %3
%endmacro

%ifdef ARCH_X86_64
cglobal x264_coeff_last4_mmxext, 1,1
    bsr rax, [r0]
    shr eax, 4
    RET
%else
cglobal x264_coeff_last4_mmxext, 0,3
    mov   edx, r0m
    mov   eax, [edx+4]
    xor   ecx, ecx
    test  eax, eax
    cmovz eax, [edx]
    setnz cl
    bsr   eax, eax
    shr   eax, 4
    lea   eax, [eax+ecx*2]
    RET
%endif

%macro COEFF_LAST 1
cglobal x264_coeff_last15_%1, 1,3
    LAST_MASK r1d, r0-2, r2d
    xor r1d, 0xffff
    bsr eax, r1d
    dec eax
    RET

cglobal x264_coeff_last16_%1, 1,3
    LAST_MASK r1d, r0, r2d
    xor r1d, 0xffff
    bsr eax, r1d
    RET

%ifndef ARCH_X86_64
%ifidn %1, mmxext
    cglobal x264_coeff_last64_%1, 1,5
%else
    cglobal x264_coeff_last64_%1, 1,4
%endif
    LAST_MASK r1d, r0, r4d
    LAST_MASK r2d, r0+32, r4d
    shl r2d, 16
    or  r1d, r2d
    LAST_MASK r2d, r0+64, r4d
    LAST_MASK r3d, r0+96, r4d
    shl r3d, 16
    or  r2d, r3d
    not r1d
    xor r2d, -1
    jne .secondhalf
    bsr eax, r1d
    RET
.secondhalf:
    bsr eax, r2d
    add eax, 32
    RET
%endif
%endmacro

%ifdef ARCH_X86_64
    cglobal x264_coeff_last64_sse2, 1,4
    LAST_MASK_SSE2 r1d, r0
    LAST_MASK_SSE2 r2d, r0+32
    LAST_MASK_SSE2 r3d, r0+64
    LAST_MASK_SSE2 r0d, r0+96
    shl r2d, 16
    shl r0d, 16
    or  r1d, r2d
    or  r3d, r0d
    shl r3,  32
    or  r1,  r3
    not r1
    bsr rax, r1
    RET
%endif

%ifndef ARCH_X86_64
%define LAST_MASK LAST_MASK_MMX
COEFF_LAST mmxext
%endif
%define LAST_MASK LAST_MASK_SSE2
COEFF_LAST sse2
