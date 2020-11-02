/* Request an area of an image for input.
 * 
 * J.Cupitt, 17/4/93.
 *
 * Modified:
 * 22/11/94 JC
 *	- check for start and stop functions removed, as can now be NULL
 * 22/2/95 JC
 *	- im_fill_copy() added
 * 18/4/95 JC
 *	- kill flag added for compute cases
 * 27/10/95 JC
 *	- im_fill_copy() now only uses im_generate() mechanism if it has to
 *	- im_fill_copy() renamed as im_prepare_inplace()
 * 18/8/99 JC
 *	- oops ... cache stuff removed, interacted badly with inplace ops
 * 26/3/02 JC
 *	- better error message for im_prepare()
 * 9/8/02 JC
 *	- im_prepare_inplace() broke with mmap() windows ... im_prepare_to()
 *	  replaces it and is nicer
 * 30/9/05
 * 	- hmm, did not stop if a start function failed
 * 7/10/09
 * 	- gtkdoc comments
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

/*
#define DEBUG
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /*HAVE_CONFIG_H*/
#include <vips/intl.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vips/vips.h>
#include <vips/thread.h>
#include <vips/debug.h>
#include <vips/internal.h>

#ifdef WITH_DMALLOC
#include <dmalloc.h>
#endif /*WITH_DMALLOC*/

/* Generate into a region. 
 */
static int
fill_region( REGION *reg )
{
	IMAGE *im = reg->im;

        /* Start new sequence, if necessary.
         */
        if( im__call_start( reg ) )
		return( -1 );

	/* Ask for evaluation.
	 */
	if( im->generate( reg, reg->seq, im->client1, im->client2 ) )
		return( -1 );

	return( 0 );
}

int
im__test_kill( IMAGE *im )
{
	/* Has kill been set for this image? If yes, abort evaluation.
	 */
	if( im->kill ) {
		im_error( "im__test_kill", _( "killed for image \"%s\"" ),
			im->filename );
		return( -1 );
	}

	return( 0 );
}

/** 
 * im_prepare:
 * @reg: region to prepare
 * @r: #Rect of pixels you need to be able to address
 *
 * im_prepare() fills @reg with pixels. After calling, you can address at
 * least the area @r with IM_REGION_ADDR() and get valid pixels.
 *
 * im_prepare() runs in-line, that is, computation is done by the calling
 * thread, no new threads are involved, and computation blocks until the
 * pixels are ready.
 *
 * Use im_prepare_thread() to calculate an area of pixels with many
 * threads. Use im_render_priority() to calculate an area of pixels in the 
 * background.
 *
 * See also: im_prepare_thread(), im_render_priority(), im_prepare_to().
 *
 * Returns: 0 on success, or -1 on error.
 */
int
im_prepare( REGION *reg, Rect *r )
{	
	IMAGE *im = reg->im;

	Rect save = *r;

	im__region_check_ownership( reg );

	if( im__test_kill( im ) )
		return( -1 );

	/* We use save for sanity checking valid: we test at the end that the
	 * pixels we have generated are indeed all the ones that were asked
	 * for.
	 *
	 * However, r may be clipped by the image size, so we need to clip
	 * save as well to make sure we don't fail the assert due to that.
	 */
{	
	Rect image;

	image.left = 0;
	image.top = 0;
	image.width = reg->im->Xsize;
	image.height = reg->im->Ysize;
	im_rect_intersectrect( &save, &image, &save );
}

#ifdef DEBUG
        printf( "im_prepare: left = %d, top = %d, width = %d, height = %d\n",
		r->left, r->top, r->width, r->height );
#endif /*DEBUG*/

	switch( im->dtype ) {
	case IM_PARTIAL:
		if( im_region_fill( reg, r, 
			(im_region_fill_fn) fill_region, NULL ) )
			return( -1 );

		break;

	case IM_SETBUF:
	case IM_SETBUF_FOREIGN:
	case IM_MMAPIN:
	case IM_MMAPINRW:
	case IM_OPENIN:
		/* Attach to existing buffer.
		 */
		if( im_region_image( reg, r ) )
			return( -1 );

		break;

	default:
		im_error( "im_prepare", _( "unable to input from a %s image" ),
			im_dtype2char( im->dtype ) );
		return( -1 );
	}

	/* valid should now include all the pixels that were asked for.
	 */
	g_assert( im_rect_includesrect( &reg->valid, &save ) );

	return( 0 );
}

/* We need to make pixels using reg's generate function, and write the result
 * to dest.
 */
static int
im_prepare_to_generate( REGION *reg, REGION *dest, Rect *r, int x, int y )
{
	IMAGE *im = reg->im;
	char *p;

	if( !im->generate ) {
		im_error( "im_prepare_to", "%s", _( "incomplete header" ) );
		return( -1 );
	}

	if( im_region_region( reg, dest, r, x, y ) )
		return( -1 );

	/* Remember where reg is pointing now.
	 */
	p = IM_REGION_ADDR( reg, reg->valid.left, reg->valid.top );

	/* Run sequence into reg.
	 */
	if( fill_region( reg ) )
		return( -1 );

	/* The generate function may not have actually made any pixels ... it
	 * might just have redirected reg to point somewhere else. If it has,
	 * we need an extra copy operation.
	 */
	if( IM_REGION_ADDR( reg, reg->valid.left, reg->valid.top ) != p )
		im_region_copy( reg, dest, r, x, y );

	return( 0 );
}

/** 
 * im_prepare_to:
 * @reg: region to prepare
 * @dest: region to write to
 * @r: #Rect of pixels you need to be able to address
 * @x: postion of @r in @dest
 * @y: postion of @r in @dest
 *
 * Like im_prepare(): fill @reg with data, ready to be read from by our caller.
 * Unlike im_prepare(), rather than allocating memory local to @reg for the
 * result, we guarantee that we will fill the pixels in @dest at offset @x, @y.
 * In other words, we generate an extra copy operation if necessary. 
 *
 * Also unlike im_prepare(), @dest is not set up for writing for you with
 * im_region_buffer(). You can
 * point @dest at anything, and pixels really will be written there. 
 * This makes im_prepare_to() useful for making the ends of pipelines, since
 * it (effectively) makes a break in the pipe.
 *
 * See also: im_prepare(), vips_sink_disc().
 *
 * Returns: 0 on success, or -1 on error
 */
int
im_prepare_to( REGION *reg, REGION *dest, Rect *r, int x, int y )
{
	IMAGE *im = reg->im;
	Rect image;
	Rect wanted;
	Rect clipped;
	Rect clipped2;
	Rect final;

	if( im__test_kill( im ) )
		return( -1 );

	/* Sanity check.
	 */
	if( !dest->data || 
		dest->im->BandFmt != reg->im->BandFmt ||
		dest->im->Bands != reg->im->Bands ) {
		im_error( "im_prepare_to", 
			"%s", _( "inappropriate region type" ) );
		return( -1 );
	}

	/* clip r first against the size of reg->im, then again against the 
	 * memory we have available to write to on dest. Just like 
	 * im_region_region()
	 */
	image.top = 0;
	image.left = 0;
	image.width = reg->im->Xsize;
	image.height = reg->im->Ysize;
	im_rect_intersectrect( r, &image, &clipped );

	g_assert( clipped.left == r->left );
	g_assert( clipped.top == r->top );

	wanted.left = x + (clipped.left - r->left);
	wanted.top = y + (clipped.top - r->top);
	wanted.width = clipped.width;
	wanted.height = clipped.height;

	/* Test that dest->valid is large enough.
	 */
	if( !im_rect_includesrect( &dest->valid, &wanted ) ) {
		im_error( "im_prepare_to", "%s", _( "dest too small" ) );
		return( -1 );
	}

	im_rect_intersectrect( &wanted, &dest->valid, &clipped2 );

	/* Translate back to reg's coordinate space and set as valid.
	 */
	final.left = r->left + (clipped2.left - wanted.left);
	final.top = r->top + (clipped2.top - wanted.top);
	final.width = clipped2.width;
	final.height = clipped2.height;

	x = clipped2.left;
	y = clipped2.top;

	if( im_rect_isempty( &final ) ) {
		im_error( "im_prepare_to", 
			"%s", _( "valid clipped to nothing" ) );
		return( -1 );
	}

#ifdef DEBUG
        printf( "im_prepare_to: left = %d, top = %d, width = %d, height = %d\n",
		final.left, final.top, final.width, final.height );
#endif /*DEBUG*/

	/* Input or output image type?
	 */
	switch( im->dtype ) {
	case IM_OPENOUT:
	case IM_PARTIAL:
		/* We are generating with a sequence. 
		 */
		if( im_prepare_to_generate( reg, dest, &final, x, y ) )
			return( -1 );

		break;

	case IM_MMAPIN:
	case IM_MMAPINRW:
	case IM_OPENIN:
		/* Attach to existing buffer and copy to dest.
		 */
		if( im_region_image( reg, &final ) )
			return( -1 );
		im_region_copy( reg, dest, &final, x, y );

		break;

	case IM_SETBUF:
	case IM_SETBUF_FOREIGN:
		/* Could be either input or output. If there is a generate
		 * function, we are outputting.
		 */
		if( im->generate ) {
			if( im_prepare_to_generate( reg, dest, &final, x, y ) )
				return( -1 );
		}
		else {
			if( im_region_image( reg, &final ) )
				return( -1 );
			im_region_copy( reg, dest, &final, x, y );
		}

		break;

	default:
		im_error( "im_prepare_to", _( "unable to input from a "
			"%s image" ), im_dtype2char( im->dtype ) );
		return( -1 );
	}

	/* We've written fresh pixels to dest, it's no longer invalid (if it
	 * was).
	 *
	 * We need this extra thing here because, unlike im_prepare(), we
	 * don't im_region_buffer() dest before writing it.
	 */
	dest->invalid = FALSE;

	return( 0 );
}

int
im_prepare_many( REGION **reg, Rect *r )
{
	for( ; *reg; ++reg )
		if( im_prepare( *reg, r ) )
			return( -1 );

	return( 0 );
}

static void *
im_invalidate_region( REGION *reg )
{
	reg->invalid = TRUE;

	return( NULL );
}

static void *
im_invalidate_image( IMAGE *im, GSList **to_be_invalidated )
{
	g_mutex_lock( im->sslock );
	(void) im_slist_map2( im->regions,
		(VSListMap2Fn) im_invalidate_region, NULL, NULL );
	g_mutex_unlock( im->sslock );

	*to_be_invalidated = g_slist_prepend( *to_be_invalidated, im );

	return( NULL );
}

/* Trigger a callbacks on a list of images, where the callbacks might create
 * or destroy the images.
 *
 * We make a set of temp regions to hold the images open, but when we switch
 * to VipsObject we should incr/decr ref count.
 */
static void
im_invalidate_trigger( GSList *images )
{
	GSList *regions;
	GSList *p;

	regions = NULL;
	for( p = images; p; p = p->next ) {
		IMAGE *im = (IMAGE *) p->data;

		regions = g_slist_prepend( regions, im_region_create( im ) );
	}

	for( p = images; p; p = p->next ) {
		IMAGE *im = (IMAGE *) p->data;

		(void) im__trigger_callbacks( im->invalidatefns );
	}

	for( p = regions; p; p = p->next ) {
		REGION *r = (REGION *) p->data;

		im_region_free( r );
	}

	g_slist_free( regions );
}

/**
 * im_invalidate:
 * @im: #IMAGE to invalidate
 *
 * Invalidate all pixel caches on an #IMAGE and any derived images. The 
 * "invalidate" callback is triggered for all invalidated images.
 *
 * See also: im_add_invalidate_callback().
 */
void
im_invalidate( IMAGE *im )
{
	GSList *to_be_invalidated;

	/* Invalidate callbacks might do anything, including removing images
	 * or invalidating other images, so we can't trigger them from within 
	 * the image loop. Instead we collect a list of image to invalidate 
	 * and trigger them all in one go, checking that they are not
	 * invalidated.
	 */
	to_be_invalidated = NULL;
	(void) im__link_map( im, 
		(VSListMap2Fn) im_invalidate_image, &to_be_invalidated, NULL );

	im_invalidate_trigger( to_be_invalidated );

	g_slist_free( to_be_invalidated );
}
