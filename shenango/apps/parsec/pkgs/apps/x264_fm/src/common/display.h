/*****************************************************************************
 * x264: h264 encoder
 *****************************************************************************
 * Copyright (C) 2005 Tuukka Toivonen <tuukkat@ee.oulu.fi>
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

#ifndef X264_DISPLAY_H
#define X264_DISPLAY_H

void disp_sync(void);
void disp_setcolor(unsigned char *name);
/* Display a region of byte wide memory as a grayscale image.
 * num is the window to use for displaying. */
void disp_gray(int num, char *data, int width, int height,
               int stride, const unsigned char *title);
void disp_gray_zoom(int num, char *data, int width, int height,
               int stride, const unsigned char *title, int zoom);
void disp_point(int num, int x1, int y1);
void disp_line(int num, int x1, int y1, int x2, int y2);
void disp_rect(int num, int x1, int y1, int x2, int y2);

#endif
