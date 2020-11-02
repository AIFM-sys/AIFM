/* Convert 1 to 4-band 8 or 16-bit VIPS images to/from PNG.
 *
 * 28/11/03 JC
 *	- better no-overshoot on tile loop
 * 22/2/05
 *	- read non-interlaced PNG with a line buffer (thanks Michel Brabants)
 * 11/1/06
 * 	- read RGBA palette-ized images more robustly (thanks Tom)
 * 20/4/06
 * 	- auto convert to sRGB/mono (with optional alpha) for save
 * 1/5/06
 * 	- from vips_png.c
 * 2/11/07
 * 	- use im_wbuffer() API for BG writes
 * 4/2/10
 * 	- gtkdoc
 * 	- fixed 16-bit save
 * 12/5/10
 * 	- lololo but broke 8-bit save, fixed again
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

#ifndef HAVE_PNG

#include <vips/vips.h>

int
im_vips2png( IMAGE *in, const char *filename )
{
	im_error( "im_vips2png", "%s",
		_( "PNG support disabled" ) );
	return( -1 );
}

#else /*HAVE_PNG*/

#include <stdio.h>
#include <stdlib.h>

#include <vips/vips.h>
#include <vips/internal.h>

#include <png.h>

#ifdef WITH_DMALLOC
#include <dmalloc.h>
#endif /*WITH_DMALLOC*/

#if PNG_LIBPNG_VER < 10003
#error "PNG library too old."
#endif

static void
user_error_function( png_structp png_ptr, png_const_charp error_msg )
{
	im_error( "im_vips2png", _( "PNG error: \"%s\"" ), error_msg );
}

static void
user_warning_function( png_structp png_ptr, png_const_charp warning_msg )
{
	im_error( "im_vips2png", _( "PNG warning: \"%s\"" ), warning_msg );
}

/* What we track during a PNG write.
 */
typedef struct {
	IMAGE *in;

	FILE *fp;
	png_structp pPng;
	png_infop pInfo;
	png_bytep *row_pointer;
} Write;

static void
write_destroy( Write *write )
{
	IM_FREEF( im_close, write->in );
	IM_FREEF( fclose, write->fp );
	if( write->pPng )
		png_destroy_write_struct( &write->pPng, &write->pInfo );
	IM_FREE( write->row_pointer );

	im_free( write );
}

#define UC IM_BANDFMT_UCHAR
#define US IM_BANDFMT_USHORT

/* Type promotion for save ... uchar or ushort.
 */
static int bandfmt_png[10] = {
/* UC  C   US  S   UI  I   F   X   D   DX */
   UC, UC, US, US, US, US, UC, UC, UC, UC
};

static Write *
write_new( IMAGE *in )
{
	Write *write;

	if( !(write = IM_NEW( NULL, Write )) )
		return( NULL );
	memset( write, 0, sizeof( Write ) );

	if( !(write->in = im__convert_saveable( in, IM__RGBA, bandfmt_png )) ) {
		im_error( "im_vips2png", 
			"%s", _( "unable to convert to saveable format" ) );
		write_destroy( write );
		return( NULL );
	}

	write->row_pointer = IM_ARRAY( NULL, in->Ysize, png_bytep );
	write->fp = NULL;
	write->pPng = NULL;
	write->pInfo = NULL;

	if( !write->row_pointer ) {
		write_destroy( write );
		return( NULL );
	}

	if( !(write->pPng = png_create_write_struct( 
		PNG_LIBPNG_VER_STRING, NULL,
		user_error_function, user_warning_function )) ) {
		write_destroy( write );
		return( NULL );
	}

	/* Catch PNG errors from png_create_info_struct().
	 */
	if( setjmp( write->pPng->jmpbuf ) ) {
		write_destroy( write );
		return( NULL );
	}

	if( !(write->pInfo = png_create_info_struct( write->pPng )) ) {
		write_destroy( write );
		return( NULL );
	}

	return( write );
}

static int
write_png_block( REGION *region, Rect *area, void *a )
{
	Write *write = (Write *) a;
	int i;

	/* Catch PNG errors. Yuk.
	 */
	if( setjmp( write->pPng->jmpbuf ) ) 
		return( -1 );

	for( i = 0; i < area->height; i++ ) 
		write->row_pointer[i] = (png_bytep)
			IM_REGION_ADDR( region, 0, area->top + i );

	png_write_rows( write->pPng, write->row_pointer, area->height );

	return( 0 );
}

/* Write a VIPS image to PNG.
 */
static int
write_vips( Write *write, int compress, int interlace )
{
	IMAGE *in = write->in;

	int i, nb_passes;

        g_assert( in->BandFmt == IM_BANDFMT_UCHAR || 
		in->BandFmt == IM_BANDFMT_USHORT );
	g_assert( in->Coding == IM_CODING_NONE );
        g_assert( in->Bands > 0 && in->Bands < 5 );

	/* Catch PNG errors.
	 */
	if( setjmp( write->pPng->jmpbuf ) ) 
		return( -1 );

	/* Check input image.
	 */
	if( im_pincheck( in ) )
		return( -1 );
	if( compress < 0 || compress > 9 ) {
		im_error( "im_vips2png", 
			"%s", _( "compress should be in [0,9]" ) );
		return( -1 );
	}

	/* Set compression parameters.
	 */
	png_set_compression_level( write->pPng, compress );

	write->pInfo->width = in->Xsize;
	write->pInfo->height = in->Ysize;
	write->pInfo->bit_depth = (in->BandFmt == IM_BANDFMT_UCHAR ? 8 : 16);
	write->pInfo->gamma = (float) 1.0;

	switch( in->Bands ) {
	case 1: write->pInfo->color_type = PNG_COLOR_TYPE_GRAY; break;
	case 2: write->pInfo->color_type = PNG_COLOR_TYPE_GRAY_ALPHA; break;
	case 3: write->pInfo->color_type = PNG_COLOR_TYPE_RGB; break;
	case 4: write->pInfo->color_type = PNG_COLOR_TYPE_RGB_ALPHA; break;

	default:
		g_assert( 0 );
	}

	png_write_info( write->pPng, write->pInfo ); 

	/* If we're an intel byte order CPU and this is a 16bit image, we need
	 * to swap bytes.
	 */
	if( write->pInfo->bit_depth > 8 && !im_amiMSBfirst() ) 
		png_set_swap( write->pPng ); 

	if( interlace )	
		nb_passes = png_set_interlace_handling( write->pPng );
	else
		nb_passes = 1;

	/* Write data.
	 */
	for( i = 0; i < nb_passes; i++ ) 
		if( vips_sink_disc( write->in, write_png_block, write ) )
			return( -1 );

	/* The setjmp() was held by our background writer: reset it.
	 */
	if( setjmp( write->pPng->jmpbuf ) ) 
		return( -1 );

	png_write_end( write->pPng, write->pInfo );

	return( 0 );
}

/**
 * im_vips2png:
 * @in: image to save 
 * @filename: file to write to 
 *
 * Write a VIPS image to a file as PNG.
 *
 * You can embed options in the filename. They have the form:
 *
 * |[
 * filename.png:<emphasis>compression</emphasis>,<emphasis>interlace</emphasis>
 * ]|
 *
 * <itemizedlist>
 *   <listitem>
 *     <para>
 * <emphasis>compression</emphasis> 
 * Compress with this much effort (0 - 9). Default 6.
 *     </para>
 *   </listitem>
 *   <listitem>
 *     <para>
 * <emphasis>interlace</emphasis> 
 * 0 means don't interlace (the default), 1 selects ADAM7 interlacing. Beware
 * than an interlaced PNG can be up to 7 times slower to write than a
 * non-interlaced image.
 *     </para>
 *   </listitem>
 * </itemizedlist>
 *
 * There is no support for attaching ICC profiles to PNG images.
 *
 * The image is automatically converted to RGB, RGBA, Monochrome or Mono +
 * alpha before saving. Images with more than one byte per band element are
 * saved as 16-bit PNG, others are saved as 8-bit PNG.
 *
 * Example:
 *
 * |[
 * im_vips2png( in, "fred.png:0,1" );
 * ]|
 *
 * Will write "fred.png" with no compression and with ADAM7 interlacing.
 *
 * See also: #VipsFormat, im_png2vips().
 *
 * Returns: 0 on success, -1 on error.
 */
int
im_vips2png( IMAGE *in, const char *filename )
{
	Write *write;
	int compress; 
	int interlace; 

	char *p, *q;

	char name[FILENAME_MAX];
	char mode[FILENAME_MAX];
	char buf[FILENAME_MAX];

	if( !(write = write_new( in )) )
		return( -1 );

	/* Extract write mode from filename and parse.
	 */
	im_filename_split( filename, name, mode );
	strcpy( buf, mode ); 
	p = &buf[0];
	compress = 6;
	interlace = 0;
	if( (q = im_getnextoption( &p )) ) 
		compress = atoi( q );
	if( (q = im_getnextoption( &p )) ) 
		interlace = atoi( q );

	/* Make output.
	 */
        if( !(write->fp = im__file_open_write( name )) ) {
		write_destroy( write );
		return( -1 );
	}
	png_init_io( write->pPng, write->fp );

	/* Convert it!
	 */
	if( write_vips( write, compress, interlace ) ) {
		write_destroy( write );
		im_error( "im_vips2png", _( "unable to write \"%s\"" ), name );

		return( -1 );
	}
	write_destroy( write );

	return( 0 );
}

#endif /*HAVE_PNG*/
