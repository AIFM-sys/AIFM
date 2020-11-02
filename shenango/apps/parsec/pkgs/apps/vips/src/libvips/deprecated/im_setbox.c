/* @(#)  Copies the coordinates of a box to an IMAGE_BOX
 * @(#)
 * @(#) Right call:
 * @(#) void im_setbox(pbox, xst, yst, xsiz, ysiz, ch_select)
 * @(#) IMAGE_BOX *pbox;
 * @(#) int xst, yst, xsiz, ysiz, ch_select;
 * @(#)  ch_select could be 0, 1, 2 or 3 corresponding to
 * @(#)  a, r , g or b respectively.
 * @(#)
 *
 * Copyright: Nicos Dessipris
 * Written on: 13/02/1990
 * Modified on : 04/04/1990
 */

/*

    This file is part of VIPS.
    
    VIPS is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

 */

/*

    These files are distributed with VIPS - http://www.vips.ecs.soton.ac.uk

 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /*HAVE_CONFIG_H*/
#include <vips/intl.h>

#include <sys/types.h>

#include <vips/vips.h>

#ifdef WITH_DMALLOC
#include <dmalloc.h>
#endif /*WITH_DMALLOC*/

void im_setbox(pbox, xst, yst, xsiz, ysiz, ch_select)
IMAGE_BOX *pbox;
int xst, yst, xsiz, ysiz, ch_select;
{
	pbox->xstart = xst;
	pbox->ystart = yst;
	pbox->xsize = xsiz;
	pbox->ysize = ysiz;
	pbox->chsel = ch_select;
}
