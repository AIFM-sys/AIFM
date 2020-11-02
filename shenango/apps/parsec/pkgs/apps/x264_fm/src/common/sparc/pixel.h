/*****************************************************************************
 * pixel.h: h264 encoder library
 *****************************************************************************
 * Copyright (C) 2005 Phil Jensen <philj@csufresno.edu>
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

#ifndef X264_SPARC_PIXEL_H
#define X264_SPARC_PIXEL_H

int x264_pixel_sad_8x8_vis( uint8_t *, int, uint8_t *, int );
int x264_pixel_sad_8x16_vis( uint8_t *, int, uint8_t *, int );
int x264_pixel_sad_16x8_vis( uint8_t *, int, uint8_t *, int );
int x264_pixel_sad_16x16_vis( uint8_t *, int, uint8_t *, int );

#endif
