/* check IMAGEs in various ways
 *
 * im_iocheck()
 * Copyright: Nicos Dessipris
 * Written on: 12/02/1990
 * Modified on : 
 * 15/4/93 JC
 *	- im_incheck(), im_outcheck() added.
 *	- type field now checked.
 * 10/6/93 JC
 *	- auto-fallback to old-style input added
 * 6/6/95 JC
 *	- revised and improved fallback code
 *
 * im_rwcheck()
 * Copyright: John Cupitt
 * Written on: 17/6/92
 * Updated on:
 * 15/4/93
 *	- checks for partial images added
 *	- now uses type field
 * 31/8/93 JC
 *	- returns ok for IM_MMAPINRW type files now too
 *	- returns -1 rather than 1 on error
 *	- ANSIfied
 * 1/10/97 JC
 *	- moved here, and renamed im_rwcheck()
 * 13/2/01 JC
 *	- im_image_sanity() checks added
 *
 * im_piocheck()
 * 10/6/93 J.Cupitt
 * 	- im_iocheck() adapted to make im_piocheck()
 * 	- auto-rewind feature added
 * 27/10/95 JC
 *	- im_pincheck() on a setbuf now zaps generate function so as not to
 *	  confuse any later calls to im_prepare() or im_prepare_inplace()
 *
 * 12/10/09
 * 	- all the above rolled into this file
 * 	- plus chunks of predicate.c
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /*HAVE_CONFIG_H*/
#include <vips/intl.h>

#include <stdio.h>
#include <stdlib.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif /*HAVE_UNISTD_H*/
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_SYS_FILE_H
#include <sys/file.h>
#endif /*HAVE_SYS_FILE_H*/

#include <vips/vips.h>
#include <vips/internal.h>
#include <vips/debug.h>

#ifdef WITH_DMALLOC
#include <dmalloc.h>
#endif /*WITH_DMALLOC*/

/**
 * SECTION: check
 * @short_description: test images for various properties
 * @stability: Stable
 * @see_also: <link linkend="libvips-imagE">image</link>
 * @include: vips/vips.h
 *
 * These functions perform simple checks on an #IMAGE, or indicate that you
 * intend to use an #IMAGE in a certain way.
 *
 * im_incheck(), im_pincheck() and friends indicate the image IO style you
 * intend to use, transforming the underlying #IMAGE structure if
 * necessary.
 *
 * im_check_mono() and friends and convenience functions that test an #IMAGE 
 * for having various properties
 * and signal an error if the condition is not met. They are useful for
 * writing image processing operations which can only work on certain types of
 * image.
 */

/* Convert a partial to a setbuf.
 */
static int
convert_ptob( IMAGE *im )
{
	IMAGE *t1;

	/* Change to IM_SETBUF. First, make a memory buffer and copy into that.
	 */
	if( !(t1 = im_open( "im_incheck:1", "t" )) ) 
		return( -1 );
	if( im_copy( im, t1 ) ) {
		im_close( t1 );
		return( -1 );
	}

	/* Copy new stuff in. We can't im__close( im ) and free stuff, as this
	 * would kill of lots of regions and cause dangling pointers
	 * elsewhere.
	 */
	im->dtype = IM_SETBUF;
	im->data = t1->data; 
	t1->data = NULL;

	/* Close temp image.
	 */
	if( im_close( t1 ) )
		return( -1 );

	return( 0 );
}

/* Convert an openin to a mmapin.
 */
static int
convert_otom( IMAGE *im )
{
	/* just mmap() the whole thing.
	 */
	if( im_mapfile( im ) ) 
		return( -1 );
	im->data = im->baseaddr + im->sizeof_header;
	im->dtype = IM_MMAPIN;

	return( 0 );
}

/**
 * im_incheck:
 * @im: image to check
 *
 * Check that an image is readable via the IM_IMAGE_ADDR() macro. If it isn't, 
 * try to transform it so that IM_IMAGE_ADDR() can work.
 *
 * See also: im_outcheck(), im_pincheck(), im_rwcheck(), IM_IMAGE_ADDR().
 *
 * Returns: 0 on succeess, or -1 on error.
 */
int
im_incheck( IMAGE *im )
{	
	g_assert( !im_image_sanity( im ) );

#ifdef DEBUG_IO
	printf( "im_incheck: old-style input for %s\n", im->filename );
#endif/*DEBUG_IO*/

	switch( im->dtype ) {
	case IM_SETBUF:
	case IM_SETBUF_FOREIGN:
		/* Should have been written to.
		 */
		if( !im->data ) {
			im_error( "im_incheck", 
				"%s", _( "no image data" ) );
			return( -1 );
		}

		break;

	case IM_MMAPIN:
	case IM_MMAPINRW:
		/* Can read from all these, in principle anyway.
		 */
		break;

	case IM_PARTIAL:
#ifdef DEBUG_IO
		printf( "im_incheck: converting partial image to WIO\n" );
#endif/*DEBUG_IO*/

		/* Change to a setbuf, so our caller can use it.
		 */
		if( convert_ptob( im ) )
			return( -1 );

		break;

	case IM_OPENIN:
#ifdef DEBUG_IO
		printf( "im_incheck: converting openin image for old-style input\n" );
#endif/*DEBUG_IO*/

		/* Change to a MMAPIN.
		 */
		if( convert_otom( im ) )
			return( -1 );

		break;

	case IM_OPENOUT:
		/* Close file down and reopen as input. I guess this will only
		 * work for vips files?
		 */
#ifdef DEBUG_IO
		printf( "im_incheck: auto-rewind of %s\n", im->filename );
#endif/*DEBUG_IO*/
		if( im__close( im ) || 
			im_openin( im ) ||
			im_incheck( im ) ) {
			im_error( "im_incheck", 
				_( "auto-rewind for %s failed" ),
				im->filename );
			return( -1 );
		}

		break;

	default:
		im_error( "im_incheck", 
			"%s", _( "image not readable" ) );
		return( -1 );
	}

	return( 0 );
}

/**
 * im_outcheck:
 * @im: image to check
 *
 * Check that an image is writeable by im_writeline(). If it isn't,
 * try to transform it so that im_writeline() can work.
 *
 * Set the image properties (like size, type and so on), then call
 * im_setupout(), then call im_writeline() for each scan line.
 *
 * See also: im_incheck(), im_poutcheck().
 *
 * Returns: 0 on succeess, or -1 on error.
 */
int 
im_outcheck( IMAGE *im )
{
#ifdef DEBUG_IO
	printf( "im_outcheck: old-style output for %s\n", im->filename );
#endif/*DEBUG_IO*/

	switch( im->dtype ) {
	case IM_PARTIAL:
		/* Make sure nothing is attached.
		 */
		if( im->generate ) {
			im_error( "im_outcheck", 
				"%s", _( "image already written" ) );
			return( -1 );
		}

		/* Cannot do old-style write to PARTIAL. Turn to SETBUF.
		 */
		im->dtype = IM_SETBUF;

		/* Fall through to SETBUF case.
		 */

	case IM_SETBUF:
		/* Check that it has not been im_setupout().
		 */
		if( im->data ) {
			im_error( "im_outcheck", 
				"%s", _( "image already written" ) );
			return( -1 );
		}

		break;

	case IM_OPENOUT:
	case IM_SETBUF_FOREIGN:
		/* Can write to this ok.
		 */
		break;

	default:
		im_error( "im_outcheck", 
			"%s", _( "image not writeable" ) );
		return( -1 );
	}

	return( 0 );
}
 
/**
 * im_iocheck:
 * @in: input image
 * @out: output image
 *
 * A convenience function to check a pair of images for IO via IM_IMAGE_ADDR()
 * and im_writeline().
 *
 * See also: im_incheck(), im_outcheck().
 *
 * Returns: 0 on succeess, or -1 on error.
 */
int 
im_iocheck( IMAGE *in, IMAGE *out )
{	
	return( im_incheck( in ) || im_outcheck( out ) );
}

/**
 * im_rwcheck:
 * @im: image to make read-write
 *
 * Gets an image ready for an in-place operation, such as im_insertplace().
 * Operations like this both read and write with IM_IMAGE_ADDR().
 *
 * See also: im_insertplace(), im_incheck().
 *
 * Returns: 0 on succeess, or -1 on error.
 */
int
im_rwcheck( IMAGE *im )
{
	/* Do an im_incheck(). This will rewind im_openout() files, and
	 * generate im_partial() files.
	 */
	if( im_incheck( im ) ) {
		im_error( "im_rwcheck", 
			"%s", _( "unable to rewind file" ) );
		return( -1 );
	}

	/* Look at the type.
	 */
	switch( im->dtype ) {
	case IM_SETBUF:
	case IM_SETBUF_FOREIGN:
	case IM_MMAPINRW:
		/* No action necessary.
		 */
		break;

	case IM_MMAPIN:
		/* Try to remap read-write.
		 */
		if( im_remapfilerw( im ) )
			return( -1 );

		break;

	default:
		im_error( "im_rwcheck", 
			"%s", _( "bad file type" ) );
		return( -1 );
	}

	return( 0 );
}

/**
 * im_pincheck:
 * @im: image to check
 *
 * Check that an image is readable with im_prepare() and friends. If it isn't,
 * try to transform the image so that im_prepare() can work.
 *
 * See also: im_incheck(), im_poutcheck(), im_prepare().
 *
 * Returns: 0 on succeess, or -1 on error.
 */
int
im_pincheck( IMAGE *im )
{	
	g_assert( !im_image_sanity( im ) );

#ifdef DEBUG_IO
	printf( "im_pincheck: enabling partial input for %s\n", im->filename );
#endif /*DEBUG_IO*/

	switch( im->dtype ) {
	case IM_SETBUF:
	case IM_SETBUF_FOREIGN:
		/* Should have been written to.
		 */
		if( !im->data ) {
			im_error( "im_pincheck", "%s", _( "no image data" ) );
			return( -1 );
		}

		/* Should be no generate functions now.
		 */
		im->start = NULL;
		im->generate = NULL;
		im->stop = NULL;

		break;

	case IM_PARTIAL:
		/* Should have had generate functions attached.
		 */
		if( !im->generate ) {
			im_error( "im_pincheck", "%s", _( "no image data" ) );
			return( -1 );
		}

		break;

	case IM_MMAPIN:
	case IM_MMAPINRW:
	case IM_OPENIN:
		break;

	case IM_OPENOUT:
		/* Close file down and reopen as im_mmapin.
		 */
#ifdef DEBUG_IO
		printf( "im_pincheck: auto-rewind of %s\n", im->filename );
#endif/*DEBUG_IO*/
		if( im__close( im ) || im_openin( im ) ) {
			im_error( "im_pincheck", 
				_( "auto-rewind for %s failed" ),
				im->filename );
			return( -1 );
		}

		break;

	default:
		im_error( "im_pincheck", "%s", _( "image not readable" ) );
		return( -1 );
	}

	return( 0 );
}

/**
 * im_poutcheck:
 * @im: image to check
 *
 * Check that an image is writeable with im_generate(). If it isn't,
 * try to transform the image so that im_generate() can work.
 *
 * See also: im_incheck(), im_poutcheck(), im_generate().
 *
 * Returns: 0 on succeess, or -1 on error.
 */
int 
im_poutcheck( IMAGE *im )
{
	if( !im ) {
		im_error( "im_poutcheck", "%s", _( "null image descriptor" ) );
		return( -1 );
	}

#ifdef DEBUG_IO
	printf( "im_pincheck: enabling partial output for %s\n", im->filename );
#endif /*DEBUG_IO*/

	switch( im->dtype ) {
	case IM_SETBUF:
		/* Check that it has not been im_setupout().
		 */
		if( im->data ) {
			im_error( "im_poutcheck", "%s", 
				_( "image already written" ) );
			return( -1 );
		}

		break;

	case IM_PARTIAL:
		/* Make sure nothing is attached.
		 */
		if( im->generate ) {
			im_error( "im_poutcheck", "%s", 
				_( "image already written" ) );
			return( -1 );
		}

		break;

	case IM_OPENOUT:
	case IM_SETBUF_FOREIGN:
		/* Okeydoke. Not much checking here.
		 */
		break;

	default:
		im_error( "im_poutcheck", "%s", _( "image not writeable" ) );
		return( -1 );
	}

	return( 0 );
}
 
/**
 * im_piocheck:
 * @in: input image
 * @out: output image
 *
 * A convenience function to check a pair of images for IO via im_prepare()
 * and im_generate().
 *
 * See also: im_pincheck(), im_poutcheck().
 *
 * Returns: 0 on succeess, or -1 on error.
 */
int 
im_piocheck( IMAGE *in, IMAGE *out )
{	
	return( im_pincheck( in ) || im_poutcheck( out ) );
}

/**
 * im_check_uncoded:
 * @domain: the originating domain for the error message
 * @im: image to check
 *
 * Check that the image is not coded. 
 * If not, set an error message
 * and return non-zero.
 *
 * See also: im_error().
 *
 * Returns: 0 on OK, or -1 on error.
 */
int
im_check_uncoded( const char *domain, IMAGE *im )
{
	if( im->Coding != IM_CODING_NONE ) {
		im_error( domain, "%s", _( "image must be uncoded" ) );
		return( -1 );
	}

	return( 0 );
}

/**
 * im_check_coding_known:
 * @domain: the originating domain for the error message
 * @im: image to check
 *
 * Check that the image is uncoded, LABQ coded or RAD coded. 
 * If not, set an error message
 * and return non-zero.
 *
 * See also: im_error().
 *
 * Returns: 0 on OK, or -1 on error.
 */
int
im_check_coding_known( const char *domain, IMAGE *im )
{
	/* These all have codings that extract/ifthenelse/etc can ignore.
	 */
	if( im->Coding != IM_CODING_NONE && 
		im->Coding != IM_CODING_LABQ &&
		im->Coding != IM_CODING_RAD ) {
		im_error( domain, "%s", _( "unknown image coding" ) );
		return( -1 );
	}

	return( 0 );
}

/**
 * im_check_coding_rad:
 * @domain: the originating domain for the error message
 * @im: image to check
 *
 * Check that the image is in Radiance coding. 
 * If not, set an error message
 * and return non-zero.
 *
 * See also: im_error().
 *
 * Returns: 0 on OK, or -1 on error.
 */
int
im_check_coding_rad( const char *domain, IMAGE *im )
{
	if( im->Coding != IM_CODING_RAD ||
		im->BandFmt != IM_BANDFMT_UCHAR || 
		im->Bands != 4 ) { 
		im_error( domain, "%s", _( "Radiance coding only" ) );
		return( -1 );
	}

	return( 0 );
}

/**
 * im_check_coding_labq:
 * @domain: the originating domain for the error message
 * @im: image to check
 *
 * Check that the image is in LABQ coding. 
 * If not, set an error message
 * and return non-zero.
 *
 * See also: im_error().
 *
 * Returns: 0 on OK, or -1 on error.
 */
int
im_check_coding_labq( const char *domain, IMAGE *im )
{
	if( im->Coding != IM_CODING_LABQ ||
		im->BandFmt != IM_BANDFMT_UCHAR || 
		im->Bands != 4 ) { 
		im_error( domain, "%s", _( "LABQ coding only" ) );
		return( -1 );
	}

	return( 0 );
}

/**
 * im_check_mono:
 * @domain: the originating domain for the error message
 * @im: image to check
 *
 * Check that the image has exactly one band.
 * Otherwise set an error message
 * and return non-zero.
 *
 * See also: im_error().
 *
 * Returns: 0 if OK, -1 otherwise.
 */
int
im_check_mono( const char *domain, IMAGE *im )
{
	if( im->Bands != 1 ) {
		im_error( domain, "%s", _( "image must one band" ) );
		return( -1 );
	}

	return( 0 );
}

/**
 * im_check_bands:
 * @domain: the originating domain for the error message
 * @im: image to check
 * @bands: must have this many bands
 *
 * Check that the image has @bands bands.
 * Otherwise set an error message
 * and return non-zero.
 *
 * See also: im_error().
 *
 * Returns: 0 if OK, -1 otherwise.
 */
int
im_check_bands( const char *domain, IMAGE *im, int bands )
{
	if( im->Bands != bands ) {
		im_error( domain, _( "image must %d bands" ), bands );
		return( -1 );
	}

	return( 0 );
}

/**
 * im_check_1or3:
 * @domain: the originating domain for the error message
 * @im: image to check
 *
 * Check that the image has either one or three bands.
 * Otherwise set an error message
 * and return non-zero.
 *
 * See also: im_error().
 *
 * Returns: 0 if OK, -1 otherwise.
 */
int
im_check_bands_1or3( const char *domain, IMAGE *im )
{
	if( im->Bands != 1 && im->Bands != 3 ) {
		im_error( domain, "%s", _( "image must one or three bands" ) );
		return( -1 );
	}

	return( 0 );
}

/**
 * im_check_bands_1orn:
 * @domain: the originating domain for the error message
 * @im1: first image to check
 * @im2: second image to check
 *
 * Check that the images have the same number of bands, or that one of the
 * images has just 1 band.
 * If not, set an error message
 * and return non-zero.
 *
 * See also: im_error().
 *
 * Returns: 0 on OK, or -1 on error.
 */
int
im_check_bands_1orn( const char *domain, IMAGE *im1, IMAGE *im2 )
{
	if( im1->Bands != im2->Bands &&
		(im1->Bands != 1 && im2->Bands != 1) ) {
		im_error( domain, "%s", 
			_( "images must have the same number of bands, "
			"or one must be single-band" ) );
		return( -1 );
	}

	return( 0 );
}

/**
 * im_check_noncomplex:
 * @domain: the originating domain for the error message
 * @im: image to check
 *
 * Check that the image is not complex.
 * Otherwise set an error message
 * and return non-zero.
 *
 * See also: im_error().
 *
 * Returns: 0 if OK, -1 otherwise.
 */
int
im_check_noncomplex( const char *domain, IMAGE *im )
{
	if( vips_bandfmt_iscomplex( im->BandFmt ) ) {
		im_error( domain, "%s", _( "image must be non-complex" ) );
		return( -1 );
	}

	return( 0 );
}

/**
 * im_check_complex:
 * @domain: the originating domain for the error message
 * @im: image to check
 *
 * Check that the image is complex.
 * Otherwise set an error message
 * and return non-zero.
 *
 * See also: im_error().
 *
 * Returns: 0 if OK, -1 otherwise.
 */
int
im_check_complex( const char *domain, IMAGE *im )
{
	if( !vips_bandfmt_iscomplex( im->BandFmt ) ) {
		im_error( domain, "%s", _( "image must be complex" ) );
		return( -1 );
	}

	return( 0 );
}

/**
 * im_check_format:
 * @domain: the originating domain for the error message
 * @im: image to check
 * @fmt: format to test for
 *
 * Check that the image has the specified format.
 * Otherwise set an error message
 * and return non-zero.
 *
 * See also: im_error().
 *
 * Returns: 0 if OK, -1 otherwise.
 */
int
im_check_format( const char *domain, IMAGE *im, VipsBandFmt fmt )
{
	if( im->BandFmt != fmt ) {
		im_error( domain, 
			_( "image must be %s" ), im_BandFmt2char( fmt ) );
		return( -1 );
	}

	return( 0 );
}

/**
 * im_check_int:
 * @domain: the originating domain for the error message
 * @im: image to check
 *
 * Check that the image is in one of the integer formats.
 * Otherwise set an error message
 * and return non-zero.
 *
 * See also: im_error().
 *
 * Returns: 0 if OK, -1 otherwise.
 */
int
im_check_int( const char *domain, IMAGE *im )
{
	if( !vips_bandfmt_isint( im->BandFmt ) ) {
		im_error( domain, "%s", _( "image must be integer" ) );
		return( -1 );
	}

	return( 0 );
}

/**
 * im_check_uint:
 * @domain: the originating domain for the error message
 * @im: image to check
 *
 * Check that the image is in one of the unsigned integer formats.
 * Otherwise set an error message
 * and return non-zero.
 *
 * See also: im_error().
 *
 * Returns: 0 if OK, -1 otherwise.
 */
int
im_check_uint( const char *domain, IMAGE *im )
{
	if( !vips_bandfmt_isuint( im->BandFmt ) ) {
		im_error( domain, "%s", _( "image must be unsigned integer" ) );
		return( -1 );
	}

	return( 0 );
}

/**
 * im_check_8or16:
 * @domain: the originating domain for the error message
 * @im: image to check
 *
 * Check that the image is 8 or 16-bit integer, signed or unsigned.
 * Otherwise set an error message
 * and return non-zero.
 *
 * See also: im_error().
 *
 * Returns: 0 if OK, -1 otherwise.
 */
int
im_check_8or16( const char *domain, IMAGE *im )
{
	if( im->BandFmt != IM_BANDFMT_UCHAR &&
		im->BandFmt != IM_BANDFMT_USHORT &&
		im->BandFmt != IM_BANDFMT_CHAR &&
		im->BandFmt != IM_BANDFMT_SHORT ) {
		im_error( domain, "%s", 
			_( "image must be 8- or 16-bit integer, "
				"signed or unsigned" ) );
		return( -1 );
	}

	return( 0 );
}

/**
 * im_check_u8or16:
 * @domain: the originating domain for the error message
 * @im: image to check
 *
 * Check that the image is 8 or 16-bit unsigned integer.
 * Otherwise set an error message
 * and return non-zero.
 *
 * See also: im_error().
 *
 * Returns: 0 if OK, -1 otherwise.
 */
int
im_check_u8or16( const char *domain, IMAGE *im )
{
	if( im->BandFmt != IM_BANDFMT_UCHAR &&
		im->BandFmt != IM_BANDFMT_USHORT ) {
		im_error( domain, "%s", 
			_( "image must be 8- or 16-bit unsigned integer" ) );
		return( -1 );
	}

	return( 0 );
}

/**
 * im_check_u8or16orf:
 * @domain: the originating domain for the error message
 * @im: image to check
 *
 * Check that the image is 8 or 16-bit unsigned integer, or float.
 * Otherwise set an error message and return non-zero.
 *
 * See also: im_error().
 *
 * Returns: 0 if OK, -1 otherwise.
 */
int
im_check_u8or16orf( const char *domain, IMAGE *im )
{
	if( im->BandFmt != IM_BANDFMT_UCHAR &&
		im->BandFmt != IM_BANDFMT_USHORT &&
		im->BandFmt != IM_BANDFMT_FLOAT ) {
		im_error( domain, "%s", 
			_( "image must be 8- or 16-bit unsigned integer, "
				"or float" ) );
		return( -1 );
	}

	return( 0 );
}

/**
 * im_check_uintorf:
 * @domain: the originating domain for the error message
 * @im: image to check
 *
 * Check that the image is unsigned int or float.
 * Otherwise set an error message and return non-zero.
 *
 * See also: im_error().
 *
 * Returns: 0 if OK, -1 otherwise.
 */
int
im_check_uintorf( const char *domain, IMAGE *im )
{
	if( im->BandFmt != IM_BANDFMT_UCHAR &&
		im->BandFmt != IM_BANDFMT_USHORT &&
		im->BandFmt != IM_BANDFMT_UINT &&
		im->BandFmt != IM_BANDFMT_FLOAT ) {
		im_error( domain, "%s", 
			_( "image must be unsigned int or float" ) );
		return( -1 );
	}

	return( 0 );
}

/**
 * im_check_size_same:
 * @domain: the originating domain for the error message
 * @im1: first image to check
 * @im2: second image to check
 *
 * Check that the images have the same size.
 * If not, set an error message
 * and return non-zero.
 *
 * See also: im_error().
 *
 * Returns: 0 if OK, -1 otherwise.
 */
int
im_check_size_same( const char *domain, IMAGE *im1, IMAGE *im2 )
{
	if( im1->Xsize != im2->Xsize || im1->Ysize != im2->Ysize ) {
		im_error( domain, "%s", _( "images must match in size" ) );
		return( -1 );
	}

	return( 0 );
}

/**
 * im_check_bands_same:
 * @domain: the originating domain for the error message
 * @im1: first image to check
 * @im2: second image to check
 *
 * Check that the images have the same number of bands.
 * If not, set an error message
 * and return non-zero.
 *
 * See also: im_error().
 *
 * Returns: 0 if OK, -1 otherwise.
 */
int
im_check_bands_same( const char *domain, IMAGE *im1, IMAGE *im2 )
{
	if( im1->Bands != im2->Bands ) {
		im_error( domain, "%s", 
			_( "images must have the same number of bands" ) ); 
		return( -1 );
	}

	return( 0 );
}

/**
 * im_check_bandno:
 * @domain: the originating domain for the error message
 * @im: image to check
 * @bandno: band number
 *
 * @bandno should be a valid band number (ie. 0 to im->Bands - 1), or can be
 * -1, meaning all bands. 
 * If not, set an error message
 * and return non-zero.
 *
 * See also: im_error().
 *
 * Returns: 0 if OK, -1 otherwise.
 */
int
im_check_bandno( const char *domain, IMAGE *im, int bandno )
{
	if( bandno < -1 ||
		bandno > im->Bands - 1 ) {
		im_error( domain, "bandno must be -1, or less than %d",
			im->Bands );
		return( -1 );
	}

	return( 0 );
}

/**
 * im_check_format_same:
 * @domain: the originating domain for the error message
 * @im1: first image to check
 * @im2: second image to check
 *
 * Check that the images have the same format.
 * If not, set an error message
 * and return non-zero.
 *
 * See also: im_error().
 *
 * Returns: 0 if OK, -1 otherwise.
 */
int
im_check_format_same( const char *domain, IMAGE *im1, IMAGE *im2 )
{
	if( im1->BandFmt != im2->BandFmt ) {
		im_error( domain, "%s", 
			_( "images must have the same band format" ) ); 
		return( -1 );
	}

	return( 0 );
}

/**
 * im_check_coding_same:
 * @domain: the originating domain for the error message
 * @im1: first image to check
 * @im2: second image to check
 *
 * Check that the images have the same coding.
 * If not, set an error message
 * and return non-zero.
 *
 * See also: im_error().
 *
 * Returns: 0 if OK, -1 otherwise.
 */
int
im_check_coding_same( const char *domain, IMAGE *im1, IMAGE *im2 )
{
	if( im1->Coding != im2->Coding ) {
		im_error( domain, "%s", 
			_( "images must have the same coding" ) ); 
		return( -1 );
	}

	return( 0 );
}

/**
 * im_check_vector:
 * @domain: the originating domain for the error message
 * @n: number of elements in vector
 * @im: image to check against
 *
 * Operations with a vector constant need a 1-element vector, or a vector with
 * the same number of elements as there are bands in the image.
 *
 * See also: im_error().
 *
 * Returns: 0 if OK, -1 otherwise.
 */
int
im_check_vector( const char *domain, int n, IMAGE *im )
{
	if( n != 1 && im->Bands != 1 && n != im->Bands ) {
		im_error( domain, 
			_( "vector must have 1 or %d elements" ), im->Bands );
		return( -1 );
	}

	return( 0 );
}

/**
 * im_check_hist:
 * @domain: the originating domain for the error message
 * @im: image to check 
 *
 * Histogram images must have width or height 1, and must not have more than 
 * 65536 elements. Return 0 if the image will pass as a histogram, or -1 and
 * set an error message otherwise.
 *
 * See also: im_error().
 *
 * Returns: 0 if OK, -1 otherwise.
 */
int
im_check_hist( const char *domain, IMAGE *im )
{
	if( im->Xsize != 1 && im->Ysize != 1 ) {
		im_error( domain, "%s", 
			_( "histograms must have width or height 1" ) );
		return( -1 );
	}
	if( im->Xsize * im->Ysize > 65536 ) {
		im_error( domain, "%s", 
			_( "histograms must have not have more than "
				"65536 elements" ) );
		return( -1 );
	}

	return( 0 );
}

/**
 * im_check_imask:
 * @domain: the originating domain for the error message
 * @mask: mask to check
 *
 * Sanity-check a mask parameter.
 *
 * See also: im_error().
 *
 * Returns: 0 if OK, -1 otherwise.
 */
int
im_check_imask( const char *domain, INTMASK *mask )
{
	if( !mask || 
		mask->xsize > 1000 || 
		mask->ysize > 1000 || 
		mask->xsize <= 0 || 
		mask->ysize <= 0 || 
		!mask->coeff ) {
		im_error( "im_conv", "%s", _( "nonsense mask parameters" ) );
		return( -1 );
	}

	return( 0 );
}

/**
 * im_check_dmask:
 * @domain: the originating domain for the error message
 * @mask: mask to check
 *
 * Sanity-check a mask parameter.
 *
 * See also: im_error().
 *
 * Returns: 0 if OK, -1 otherwise.
 */
int
im_check_dmask( const char *domain, DOUBLEMASK *mask )
{
	if( !mask || 
		mask->xsize > 1000 || 
		mask->ysize > 1000 || 
		mask->xsize <= 0 || 
		mask->ysize <= 0 || 
		!mask->coeff ) {
		im_error( "im_conv", "%s", _( "nonsense mask parameters" ) );
		return( -1 );
	}

	return( 0 );
}

/**
 * vips_bandfmt_isint:
 * @fmt: format to test
 *
 * Return %TRUE if @fmt is one of the integer types.
 */
gboolean
vips_bandfmt_isint( VipsBandFmt fmt )
{
	switch( fmt ) {
	case IM_BANDFMT_UCHAR:
	case IM_BANDFMT_CHAR:
	case IM_BANDFMT_USHORT:
	case IM_BANDFMT_SHORT:
	case IM_BANDFMT_UINT:
	case IM_BANDFMT_INT:
		return( TRUE );

	case IM_BANDFMT_FLOAT:
	case IM_BANDFMT_DOUBLE:	
	case IM_BANDFMT_COMPLEX:
	case IM_BANDFMT_DPCOMPLEX:	
		return( FALSE );
	
	default:
		g_assert( 0 );
		return( -1 );
	}
}

/**
 * vips_bandfmt_isuint:
 * @fmt: format to test
 *
 * Return %TRUE if @fmt is one of the unsigned integer types.
 */
gboolean
vips_bandfmt_isuint( VipsBandFmt fmt )
{
	switch( fmt ) {
	case IM_BANDFMT_UCHAR:
	case IM_BANDFMT_USHORT:
	case IM_BANDFMT_UINT:
		return( 1 );

	case IM_BANDFMT_INT:
	case IM_BANDFMT_SHORT:
	case IM_BANDFMT_CHAR:
	case IM_BANDFMT_FLOAT:
	case IM_BANDFMT_DOUBLE:	
	case IM_BANDFMT_COMPLEX:
	case IM_BANDFMT_DPCOMPLEX:	
		return( 0 );
	
	default:
		g_assert( 0 );
		return( -1 );
	}
}

/**
 * vips_bandfmt_isfloat:
 * @fmt: format to test
 *
 * Return %TRUE if @fmt is one of the float types.
 */
gboolean
vips_bandfmt_isfloat( VipsBandFmt fmt )
{
	switch( fmt ) {
	case IM_BANDFMT_FLOAT:
	case IM_BANDFMT_DOUBLE:	
		return( 1 );

	case IM_BANDFMT_UCHAR:
	case IM_BANDFMT_CHAR:
	case IM_BANDFMT_USHORT:
	case IM_BANDFMT_SHORT:
	case IM_BANDFMT_UINT:
	case IM_BANDFMT_INT:
	case IM_BANDFMT_COMPLEX:
	case IM_BANDFMT_DPCOMPLEX:	
		return( 0 );
	
	default:
		g_assert( 0 );
		return( -1 );
	}
}

/**
 * vips_bandfmt_iscomplex:
 * @im: image to test
 *
 * Return %TRUE if @fmt is one of the complex types.
 */
gboolean
vips_bandfmt_iscomplex( VipsBandFmt fmt )
{
	switch( fmt ) {
	case IM_BANDFMT_COMPLEX:
	case IM_BANDFMT_DPCOMPLEX:	
		return( 1 );

	case IM_BANDFMT_UCHAR:
	case IM_BANDFMT_CHAR:
	case IM_BANDFMT_USHORT:
	case IM_BANDFMT_SHORT:
	case IM_BANDFMT_UINT:
	case IM_BANDFMT_INT:
	case IM_BANDFMT_FLOAT:
	case IM_BANDFMT_DOUBLE:	
		return( 0 );
	
	default:
		g_assert( 0 );
		return( -1 );
	}
}

/**
 * im_isMSBfirst:
 * @im: image to test
 *
 * Return %TRUE if @im is in most-significant-
 * byte first form. This is the byte order used on the SPARC
 * architecture
 * and others. 
 */
gboolean
im_isMSBfirst( IMAGE *im )
{	
	if( im->magic == IM_MAGIC_SPARC )
		return( 1 );
	else
		return( 0 );
}

/**
 * im_isfile:
 * @im: image to test
 *
 * Return %TRUE if @im represents a file on disc in some way. 
 */
gboolean 
im_isfile( IMAGE *im )
{
	switch( im->dtype ) {
	case IM_MMAPIN:
	case IM_MMAPINRW:
	case IM_OPENOUT:
	case IM_OPENIN:
		return( 1 );

	case IM_PARTIAL:
	case IM_SETBUF:
	case IM_SETBUF_FOREIGN:
	case IM_NONE:
		return( 0 );

	default:
		error_exit( "im_isfile: corrupt IMAGE descriptor" );
		/*NOTREACHED*/
		return( -1 );
	}
}

/**
 * im_ispartial:
 * @im: image to test
 *
 * Return %TRUE if @im represents a partial image (a delayed calculation).
 */
gboolean 
im_ispartial( IMAGE *im )
{
	switch( im->dtype ) {
	case IM_PARTIAL:
		return( 1 );

	case IM_SETBUF:
	case IM_SETBUF_FOREIGN:
	case IM_MMAPIN:
	case IM_MMAPINRW:
	case IM_OPENIN:
	case IM_OPENOUT:
	case IM_NONE:
		return( 0 );

	default:
		error_exit( "im_ispartial: corrupt IMAGE descriptor" );
		/*NOTREACHED*/
		return( -1 );
	}
}
