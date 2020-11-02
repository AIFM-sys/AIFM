/* Read and write a VIPS file into an IMAGE *
 * 
 * 22/5/08
 * 	- from im_open.c, im_openin.c, im_desc_hd.c, im_readhist.c,
 * 	  im_openout.c
 * 19/3/09
 *	- block mmaps of nodata images
 * 12/5/09
 *	- fix signed/unsigned warnings
 * 12/10/09
 *	- heh argh reading history always stopped after the first line
 * 9/12/09
 * 	- only wholly map input files on im_incheck() ... this reduces VM use,
 * 	  especially with large numbers of small files
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
#include <ctype.h>

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#ifdef HAVE_SYS_FILE_H
#include <sys/file.h>
#endif /*HAVE_SYS_FILE_H*/
#include <sys/stat.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif /*HAVE_UNISTD_H*/
#ifdef HAVE_IO_H
#include <io.h>
#endif /*HAVE_IO_H*/
#include <libxml/parser.h>
#include <errno.h>

#ifdef OS_WIN32
#include <windows.h>
#endif /*OS_WIN32*/

#include <vips/vips.h>
#include <vips/internal.h>
#include <vips/debug.h>

#ifdef WITH_DMALLOC
#include <dmalloc.h>
#endif /*WITH_DMALLOC*/

/**
 * SECTION: image
 * @short_description: the VIPS image class
 * @stability: Stable
 * @see_also: <link linkend="libvips-region">region</link>
 * @include: vips/vips.h
 *
 * The VIPS image class and associated types and macros.
 */

/**
 * IM_MAGIC_INTEL:
 *
 * The first four bytes of a VIPS file in Intel byte ordering.
 */

/**
 * IM_MAGIC_SPARC:
 *
 * The first four bytes of a VIPS file in SPARC byte ordering.
 */

/** 
 * VipsDemandStyle:
 * @IM_SMALLTILE: demand in small (typically 64x64 pixel) tiles
 * @IM_FATSTRIP: demand in fat (typically 10 pixel high) strips
 * @IM_THINSTRIP: demand in thin (typically 1 pixel high) strips
 * @IM_ANY: demand geometry does not matter
 *
 * See im_demand_hint(). Operations can hint to the VIPS image IO system about
 * the kind of demand geometry they prefer. 
 *
 * These demand styles are given below in order of increasing
 * restrictiveness.  When demanding output from a pipeline, im_generate()
 * will use the most restrictive of the styles requested by the operations 
 * in the pipeline.
 *
 * IM_THINSTRIP --- This operation would like to output strips the width 
 * of the image and a few pels high. This is option suitable for 
 * point-to-point operations, such as those in the arithmetic package.
 *
 * This option is only efficient for cases where each output pel depends 
 * upon the pel in the corresponding position in the input image.
 *
 * IM_FATSTRIP --- This operation would like to output strips the width 
 * of the image and as high as possible. This option is suitable for area 
 * operations which do not violently transform coordinates, such as im_conv(). 
 *
 * IM_SMALLTILE --- This is the most general demand format.
 * Output is demanded in small (around 100x100 pel) sections. This style works 
 * reasonably efficiently, even for bizzare operations like 45 degree rotate.
 *
 * IM_ANY --- This image is not being demand-read from a disc file (even 
 * indirectly) so any demand style is OK. It's used for things like
 * im_black() where the pixels are calculated.
 *
 * See also: im_demand_hint().
 */

/**
 * VipsType: 
 * @IM_TYPE_MULTIBAND: generic many-band image
 * @IM_TYPE_B_W: some kind of single-band image
 * @IM_TYPE_HISTOGRAM: a 1D image such as a histogram or lookup table
 * @IM_TYPE_FOURIER: image is in fourier space
 * @IM_TYPE_XYZ: the first three bands are colours in CIE XYZ colourspace
 * @IM_TYPE_LAB: pixels are in CIE Lab space
 * @IM_TYPE_CMYK: the first four bands are in CMYK space
 * @IM_TYPE_LABQ: implies %IM_CODING_LABQ
 * @IM_TYPE_RGB: generic RGB space
 * @IM_TYPE_UCS: a uniform colourspace based on CMC
 * @IM_TYPE_LCH: pixels are in CIE LCh space
 * @IM_TYPE_LABS: pixels are CIE LAB coded as three signed 16-bit values
 * @IM_TYPE_sRGB: pixels are sRGB
 * @IM_TYPE_YXY: pixels are CIE Yxy
 * @IM_TYPE_RGB16: generic 16-bit RGB
 * @IM_TYPE_GREY16: generic 16-bit mono
 *
 * These values are set by operations as hints to user-interfaces built on top 
 * of VIPS to help them show images to the user in a meaningful way. 
 * Operations do not use these values to decide their action.
 */

/**
 * VipsBandFmt: 
 * @IM_BANDFMT_UCHAR: unsigned char format
 * @IM_BANDFMT_CHAR: char format
 * @IM_BANDFMT_USHORT: unsigned short format
 * @IM_BANDFMT_SHORT: short format
 * @IM_BANDFMT_UINT: unsigned int format
 * @IM_BANDFMT_INT: int format
 * @IM_BANDFMT_FLOAT: float format
 * @IM_BANDFMT_COMPLEX: complex (two floats) format
 * @IM_BANDFMT_DOUBLE: double float format
 * @IM_BANDFMT_DPCOMPLEX: double complex (two double) format
 *
 * The format used for each band element. 
 *
 * Each corresponnds to a native C type for the current machine. For example,
 * %IM_BANDFMT_USHORT is <type>unsigned short</type>.
 */

/**
 * VipsCoding: 
 * @IM_CODING_NONE: pixels are not coded
 * @IM_CODING_LABQ: pixels encode 3 float CIELAB values as 4 uchar
 * @IM_CODING_RAD: pixels encode 3 float RGB as 4 uchar (Radiance coding)
 *
 * How pixels are coded. 
 *
 * Normally, pixels are uncoded and can be manipulated as you would expect.
 * However some file formats code pixels for compression, and sometimes it's
 * useful to be able to manipulate images in the coded format.
 */

/** 
 * VipsProgress:
 * @run: Time we have been running 
 * @eta: Estimated seconds of computation left 
 * @tpels: Number of pels we expect to calculate
 * @npels: Number of pels calculated so far
 * @percent: Percent complete
 * @start: Start time 
 *
 * A structure available to eval callbacks giving information on evaluation
 * progress. See im_add_eval_callback().
 */

/**
 * VipsImage:
 * @Xsize: image width, in pixels
 * @Ysize: image height, in pixels
 * @Bands: number of image bands
 * @BandFmt: #VipsFormat describing the pixel type
 * @Coding: #VipsCoding describing the pixel coding type
 * @Type: a #VipsType hinting how the image pixels should be interpreted
 * @Xres: horizontal pixels per millimetre
 * @Yres: vertical pixels per millimetre
 * @Xoffset: a hint giving the position of the origin in the image
 * @Yoffset: a hint giving the position of the origin in the image
 * @filename: the disc file associated with this image, or %NULL
 * @data: the pixel data associated with this image, or %NULL
 * @time: the evaluation progress associated with this image, or %NULL
 * @kill: set this to non-zero to block evaluation of this image
 *
 * An image. These can represent an image on disc, a memory buffer, an image
 * in the process of being written to disc or a partially evaluated image
 * in memory.
 */

/**
 * IM_IMAGE_SIZEOF_ELEMENT:
 * @I: a #VipsImage
 *
 * Returns: sizeof() a band element.
 */

/**
 * IM_IMAGE_SIZEOF_PEL:
 * @I: a #VipsImage
 *
 * Returns: sizeof() a pixel.
 */

/**
 * IM_IMAGE_SIZEOF_LINE:
 * @I: a #VipsImage
 *
 * Returns: sizeof() a scanline of pixels.
 */

/**
 * IM_IMAGE_N_ELEMENTS:
 * @I: a #VipsImage
 *
 * Returns: The number of band elements in a scanline.
 */

/**
 * IM_IMAGE_ADDR:
 * @I: a #VipsImage
 * @X: x coordinate
 * @Y: y coordinate
 *
 * This macro returns a pointer to a pixel in an image. It only works for
 * images which are fully available in memory, so memory buffers and small
 * mapped images only.
 * 
 * If DEBUG is defined, you get a version that checks bounds for you.
 *
 * Returns: The address of pixel (x,y) in the image.
 */

/** 
 * im_open_local_array:
 * @IM: image to open local to
 * @OUT: array to fill with #IMAGE
 * @N: array size
 * @NAME: filename to open
 * @MODE: mode to open with
 *
 * Just like im_open(), but opens an array of images. Handy for creating a set
 * of temporary images for a function.
 *
 * Example:
 *
 * |[
 * IMAGE *t[5];
 *
 * if( im_open_local_array( out, t, 5, "some-temps", "p" ) ||
 *   im_add( a, b, t[0] ) ||
 *   im_invert( t[0], t[1] ) ||
 *   im_add( t[1], t[0], t[2] ) ||
 *   im_costra( t[2], out ) )
 *   return( -1 );
 * ]|
 *
 * See also: im_open(), im_open_local(), im_local_array().
 *
 * Returns: 0 on sucess, or -1 on error
 */

/**
 * im_open_local:
 * @IM: image to open local to
 * @NAME: filename to open
 * @MODE: mode to open with
 *
 * Just like im_open(), but the #IMAGE will be closed for you automatically
 * when @IM is closed.
 *
 * See also: im_open(), im_close(), im_local().
 *
 * Returns: a new #IMAGE, or NULL on error
 */

/* Try to make an O_BINARY ... sometimes need the leading '_'.
 */
#ifdef BINARY_OPEN
#ifndef O_BINARY
#ifdef _O_BINARY
#define O_BINARY _O_BINARY
#endif /*_O_BINARY*/
#endif /*!O_BINARY*/
#endif /*BINARY_OPEN*/

/* Our XML namespace.
 */
#define NAMESPACE "http://www.vips.ecs.soton.ac.uk/vips" 

/* Sort of open for read for image files. Shared with im_binfile().
 */
int
im__open_image_file( const char *filename )
{
	int fd;

	/* Try to open read-write, so that calls to im_makerw() will work.
	 * When we later mmap this file, we set read-only, so there 
	 * is little danger of scrubbing over files we own.
	 */
#ifdef BINARY_OPEN
	if( (fd = open( filename, O_RDWR | O_BINARY )) == -1 ) {
#else /*BINARY_OPEN*/
	if( (fd = open( filename, O_RDWR )) == -1 ) {
#endif /*BINARY_OPEN*/
		/* Open read-write failed. Fall back to open read-only.
		 */
#ifdef BINARY_OPEN
		if( (fd = open( filename, O_RDONLY | O_BINARY )) == -1 ) {
#else /*BINARY_OPEN*/
		if( (fd = open( filename, O_RDONLY )) == -1 ) {
#endif /*BINARY_OPEN*/
			im_error( "im__open_image_file", 
				_( "unable to open \"%s\", %s" ),
				filename, strerror( errno ) );
			return( -1 );
		}
	}

	return( fd );
}

/* Predict the size of the header plus pixel data. Don't use off_t,
 * it's sometimes only 32 bits (eg. on many windows build environments) and we
 * want to always be 64 bit.
 */
gint64
im__image_pixel_length( IMAGE *im )
{
	gint64 psize;

	switch( im->Coding ) {
	case IM_CODING_LABQ:
	case IM_CODING_RAD:
	case IM_CODING_NONE:
		psize = (gint64) IM_IMAGE_SIZEOF_LINE( im ) * im->Ysize;
		break;

	default:
		psize = im->Length;
		break;
	}

	return( psize + im->sizeof_header );
}

/* Read short/int/float LSB and MSB first.
 */
void
im__read_4byte( int msb_first, unsigned char *to, unsigned char **from )
{
	unsigned char *p = *from;
	int out;

	if( msb_first )
		out = p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3];
	else
		out = p[3] << 24 | p[2] << 16 | p[1] << 8 | p[0];

	*from += 4;
	*((guint32 *) to) = out;
}

void
im__read_2byte( int msb_first, unsigned char *to, unsigned char **from )
{
	int out;
	unsigned char *p = *from;

	if( msb_first )
		out = p[0] << 8 | p[1];
	else
		out = p[1] << 8 | p[0];

	*from += 2;
	*((guint16 *) to) = out;
}

/* We always write in native byte order.
 */
void
im__write_4byte( unsigned char **to, unsigned char *from )
{
	*((guint32 *) *to) = *((guint32 *) from);
	*to += 4;
}

void
im__write_2byte( unsigned char **to, unsigned char *from )
{
	*((guint16 *) *to) = *((guint16 *) from);
	*to += 2;
}

/* offset, read, write functions.
 */
typedef struct _FieldIO {
	glong offset;
	void (*read)( int msb_first, unsigned char *to, unsigned char **from );
	void (*write)( unsigned char **to, unsigned char *from );
} FieldIO;

static FieldIO fields[] = {
	{ G_STRUCT_OFFSET( IMAGE, Xsize ), 
		im__read_4byte, im__write_4byte },
	{ G_STRUCT_OFFSET( IMAGE, Ysize ), 
		im__read_4byte, im__write_4byte },
	{ G_STRUCT_OFFSET( IMAGE, Bands ), 
		im__read_4byte, im__write_4byte },
	{ G_STRUCT_OFFSET( IMAGE, Bbits ), 
		im__read_4byte, im__write_4byte },
	{ G_STRUCT_OFFSET( IMAGE, BandFmt ), 
		im__read_4byte, im__write_4byte },
	{ G_STRUCT_OFFSET( IMAGE, Coding ), 
		im__read_4byte, im__write_4byte },
	{ G_STRUCT_OFFSET( IMAGE, Type ), 
		im__read_4byte, im__write_4byte },
	{ G_STRUCT_OFFSET( IMAGE, Xres ), 
		im__read_4byte, im__write_4byte },
	{ G_STRUCT_OFFSET( IMAGE, Yres ), 
		im__read_4byte, im__write_4byte },
	{ G_STRUCT_OFFSET( IMAGE, Length ), 
		im__read_4byte, im__write_4byte },
	{ G_STRUCT_OFFSET( IMAGE, Compression ), 
		im__read_2byte, im__write_2byte },
	{ G_STRUCT_OFFSET( IMAGE, Level ), 
		im__read_2byte, im__write_2byte },
	{ G_STRUCT_OFFSET( IMAGE, Xoffset ), 
		im__read_4byte, im__write_4byte },
	{ G_STRUCT_OFFSET( IMAGE, Yoffset ), 
		im__read_4byte, im__write_4byte }
};

int
im__read_header_bytes( IMAGE *im, unsigned char *from )
{
	int msb_first;
	int i;

	im__read_4byte( 1, (unsigned char *) &im->magic, &from );
	if( im->magic != IM_MAGIC_INTEL && im->magic != IM_MAGIC_SPARC ) {
		im_error( "im_open", _( "\"%s\" is not a VIPS image" ), 
			im->filename );
		return( -1 );
	}
	msb_first = im->magic == IM_MAGIC_SPARC;

	for( i = 0; i < IM_NUMBER( fields ); i++ )
		fields[i].read( msb_first,
			&G_STRUCT_MEMBER( unsigned char, im, fields[i].offset ),
			&from );

	/* Set this ourselves ... bbits is deprecated in the file format.
	 */
	im->Bbits = im_bits_of_fmt( im->BandFmt );

	return( 0 );
}

int
im__write_header_bytes( IMAGE *im, unsigned char *to )
{
	guint32 magic;
	int i;
	unsigned char *q;

	/* Always write the magic number MSB first.
	 */
	magic = im_amiMSBfirst() ? IM_MAGIC_SPARC : IM_MAGIC_INTEL;
	to[0] = magic >> 24;
	to[1] = magic >> 16;
	to[2] = magic >> 8;
	to[3] = magic;
	q = to + 4;

	for( i = 0; i < IM_NUMBER( fields ); i++ )
		fields[i].write( &q,
			&G_STRUCT_MEMBER( unsigned char, im, 
				fields[i].offset ) );

	/* Pad spares with zeros.
	 */
	while( q - to < im->sizeof_header )
		*q++ = 0;

	return( 0 );
}

/* Read a chunk of an fd into memory. Add a '\0' at the end.
 */
static char *
read_chunk( int fd, gint64 offset, size_t length )
{
	char *buf;

	if( im__seek( fd, offset ) )
		return( NULL );
	if( !(buf = im_malloc( NULL, length + 1 )) )
		return( NULL );
	if( read( fd, buf, length ) != (ssize_t) length ) {
		im_free( buf );
		im_error( "im_readhist", "%s", _( "unable to read history" ) );
		return( NULL );
	}
	buf[length] = '\0';

	return( buf );
}

/* Does it look like an image has an extension block?
 */
int
im__has_extension_block( IMAGE *im )
{
	gint64 psize;

	psize = im__image_pixel_length( im );
	g_assert( im->file_length > 0 );

	return( im->file_length - psize > 0 );
}

/* Read everything after the pixels into memory.
 */
void *
im__read_extension_block( IMAGE *im, int *size )
{
	gint64 psize;
	void *buf;

	psize = im__image_pixel_length( im );
	g_assert( im->file_length > 0 );
	if( im->file_length - psize > 10 * 1024 * 1024 ) {
		im_error( "im_readhist",
			"%s", _( "more than a 10 megabytes of XML? "
			"sufferin' succotash!" ) );
		return( NULL );
	}
	if( im->file_length - psize == 0 )
		return( NULL );
	if( !(buf = read_chunk( im->fd, psize, im->file_length - psize )) )
		return( NULL );
	if( size )
		*size = im->file_length - psize;

#ifdef DEBUG
	printf( "im__read_extension_block: read %d bytes from %s\n",
		(int) (im->file_length - psize), im->filename );
	printf( "data: \"%s\"\n", (char *) buf );
#endif /*DEBUG*/

	return( buf );
}

/* Read everything after the pixels into memory.

	FIXME ... why can't we use xmlParserInputBufferCreateFd and parse
	directly from the fd rather than having to read the stupid thing into 
	memory

	the libxml API docs are impossible to decipher

 */
static xmlDoc *
read_xml( IMAGE *im )
{
	void *buf;
	int size;
	xmlDoc *doc;
	xmlNode *node;

	if( !(buf = im__read_extension_block( im, &size )) )
		return( NULL );
	if( !(doc = xmlParseMemory( buf, size )) ) {
		im_free( buf );
		return( NULL );
	}
	im_free( buf );
	if( !(node = xmlDocGetRootElement( doc )) ||
		!node->nsDef ||
		!im_isprefix( NAMESPACE, (char *) node->nsDef->href ) ) {
		im_error( "im__readhist", 
			"%s", _( "incorrect namespace in XML" ) );
		xmlFreeDoc( doc );
		return( NULL );
	}

#ifdef DEBUG
	printf( "read_xml: namespace == %s\n", node->nsDef->href );
#endif /*DEBUG*/

	return( doc );
}

/* Find the first child node with a name.
 */
static xmlNode *
get_node( xmlNode *base, const char *name )
{
	xmlNode *i;

	for( i = base->children; i; i = i->next )
		if( strcmp( (char *) i->name, name ) == 0 )
			return( i );

	return( NULL );
}

/* Read a string property to a buffer. TRUE for success.
 */
static int
get_sprop( xmlNode *xnode, const char *name, char *buf, int sz )
{
        char *value = (char *) xmlGetProp( xnode, (xmlChar *) name );

        if( !value )
                return( 0 );

        im_strncpy( buf, value, sz );
        IM_FREEF( xmlFree, value );

        return( 1 );
}

/* Chop history into lines, add each one as a refstring.
 */
static void
set_history( IMAGE *im, char *history )
{
	GSList *history_list;
	char *p, *q;

	/* There can be history there already if we're rewinding.
	 */
	IM_FREEF( im__gslist_gvalue_free, im->history_list );

	history_list = NULL;

	for( p = history; *p; p = q ) {
		if( (q = strchr( p, '\n' )) ) {
			*q = '\0';
			q += 1;
		}
		else 
			q = p + strlen( p );

		history_list = g_slist_prepend( history_list, 
			im__gvalue_ref_string_new( p ) );
	}

	im->history_list = g_slist_reverse( history_list );
}

/* Load header fields.
 */
static int
rebuild_header_builtin( IMAGE *im, xmlNode *i )
{
	char name[256];

	if( get_sprop( i, "name", name, 256 ) ) {
		if( strcmp( name, "Hist" ) == 0 ) {
			char *history;

			/* Have to take (another) copy, since we need to free
			 * with xmlFree().
			 */
			history = (char *) xmlNodeGetContent( i );
			set_history( im, history );
			xmlFree( history );
		}
	}

	return( 0 );
}

/* Load meta fields.
 */
static int
rebuild_header_meta( IMAGE *im, xmlNode *i )
{
	char name[256];
	char type[256];

	if( get_sprop( i, "name", name, 256 ) &&
		get_sprop( i, "type", type, 256 ) ) {
		GType gtype = g_type_from_name( type );

		/* Can we convert from IM_SAVE_STRING to type?
		 */
		if( gtype && 
			g_value_type_transformable( 
				IM_TYPE_SAVE_STRING, gtype ) ) {
			char *content;
			GValue save_value = { 0 };
			GValue value = { 0 };

			content = (char *) xmlNodeGetContent( i );
			g_value_init( &save_value, IM_TYPE_SAVE_STRING );
			im_save_string_set( &save_value, content );
			xmlFree( content );

			g_value_init( &value, gtype );
			if( !g_value_transform( &save_value, &value ) ) {
				g_value_unset( &save_value );
				im_error( "im__readhist", 
					"%s", _( "error transforming from "
					"save format" ) );
				return( -1 );
			}
			if( im_meta_set( im, name, &value ) ) {
				g_value_unset( &save_value );
				g_value_unset( &value );
				return( -1 );
			}
			g_value_unset( &save_value );
			g_value_unset( &value );
		}
	}

	return( 0 );
}

static xmlDoc *
get_xml( IMAGE *im )
{
	if( im_header_get_typeof( im, IM_META_XML ) ) {
		xmlDoc *doc;

		if( im_meta_get_area( im, IM_META_XML, (void *) &doc ) )
			return( NULL );

		return( doc );
	}

	return( NULL );
}

/* Rebuild header fields that depend on stuff saved in xml.
 */
static int
rebuild_header( IMAGE *im )
{
	xmlDoc *doc;

	if( (doc = get_xml( im )) ) {
		xmlNode *root;
		xmlNode *block;

		if( !(root = xmlDocGetRootElement( doc )) )
			return( -1 );
		if( (block = get_node( root, "header" )) ) {
			xmlNode *i;

			for( i = block->children; i; i = i->next )
				if( strcmp( (char *) i->name, "field" ) == 0 ) 
					if( rebuild_header_builtin( im, i ) )
						return( -1 );
		}
		if( (block = get_node( root, "meta" )) ) {
			xmlNode *i;

			for( i = block->children; i; i = i->next )
				if( strcmp( (char *) i->name, "field" ) == 0 ) 
					if( rebuild_header_meta( im, i ) )
						return( -1 );
		}
	}

	return( 0 );
}

/* Called at the end of im_openin() ... get any XML after the pixel data
 * and read it in.
 */
static int 
im__readhist( IMAGE *im )
{
	/* Junk any old xml meta.
	 */
	if( im_header_get_typeof( im, IM_META_XML ) ) 
		im_meta_set_area( im, IM_META_XML, NULL, NULL );

	if( im__has_extension_block( im ) ) {
		xmlDoc *doc;

		if( !(doc = read_xml( im )) )
			return( -1 );
		if( im_meta_set_area( im, IM_META_XML, 
			(im_callback_fn) xmlFreeDoc, doc ) ) {
			xmlFreeDoc( doc );
			return( -1 );
		}
	}

	if( rebuild_header( im ) )
		return( -1 );

	return( 0 );
}

#define MAX_STRSIZE (32768)     /* Max size of text for stack strings */

static int
set_prop( xmlNode *node, const char *name, const char *fmt, ... )
{       
        va_list ap;
        char value[MAX_STRSIZE];

        va_start( ap, fmt );
        (void) im_vsnprintf( value, MAX_STRSIZE, fmt, ap );
        va_end( ap );

        if( !xmlSetProp( node, (xmlChar *) name, (xmlChar *) value ) ) {
                im_error( "im_writehist", _( "unable to set property \"%s\" "
                        "to value \"%s\"." ),
                        name, value );
                return( -1 );
        }       
        
        return( 0 );
}

static int
set_sprop( xmlNode *node, const char *name, const char *value )
{
        if( value && set_prop( node, name, "%s", value ) )
                return( -1 );

        return( 0 );
}

static int
set_field( xmlNode *node, 
	const char *name, const char *type, const char *content )
{
	xmlNode *field;

	if( !(field = xmlNewChild( node, NULL, (xmlChar *) "field", NULL )) || 
		set_sprop( field, "type", type ) ||
		set_sprop( field, "name", name ) )
		return( -1 );
	xmlNodeSetContent( field, (xmlChar *) content );

	return( 0 );
}

static void *
save_fields_meta( Meta *meta, xmlNode *node )
{
	GType type = G_VALUE_TYPE( &meta->value );

	/* If we can transform to IM_TYPE_SAVE_STRING and back, we can save and
	 * restore. 
	 */
	if( g_value_type_transformable( type, IM_TYPE_SAVE_STRING ) &&
		g_value_type_transformable( IM_TYPE_SAVE_STRING, type ) ) {
		GValue save_value = { 0 };

		g_value_init( &save_value, IM_TYPE_SAVE_STRING );
		if( !g_value_transform( &meta->value, &save_value ) ) {
			im_error( "im__writehist", "%s", 
				_( "error transforming to save format" ) );
			return( node );
		}
		if( set_field( node, meta->field, g_type_name( type ), 
			im_save_string_get( &save_value ) ) ) {
			g_value_unset( &save_value );
			return( node );
		}
		g_value_unset( &save_value );
	}

	return( NULL );
}

static int
save_fields( IMAGE *im, xmlNode *node )
{
	xmlNode *this;

	/* Save header fields.
	 */
	if( !(this = xmlNewChild( node, NULL, (xmlChar *) "header", NULL )) )
		return( -1 ); 
	if( set_field( this, "Hist", 
		g_type_name( IM_TYPE_REF_STRING ), im_history_get( im ) ) ) 
		return( -1 );

	if( !(this = xmlNewChild( node, NULL, (xmlChar *) "meta", NULL )) )
		return( -1 );
	if( im->Meta_traverse && 
		im_slist_map2( im->Meta_traverse, 
			(VSListMap2Fn) save_fields_meta, this, NULL ) )
		return( -1 );

	return( 0 );
}

int
im__write_extension_block( IMAGE *im, void *buf, int size )
{
	gint64 length;
	gint64 psize;

	psize = im__image_pixel_length( im );
	if( (length = im_file_length( im->fd )) == -1 )
		return( -1 );
	if( length - psize < 0 ) {
		im_error( "im__write_extension_block",
			"%s", _( "file has been truncated" ) );
		return( -1 );
	}

	if( im__ftruncate( im->fd, psize ) ||
		im__seek( im->fd, psize ) ) 
		return( -1 );
	if( im__write( im->fd, buf, size ) )
                return( -1 );

#ifdef DEBUG
	printf( "im__write_extension_block: written %d bytes of XML to %s\n",
		size, im->filename );
#endif /*DEBUG*/

	return( 0 );
}

#ifdef DEBUG
/* Return a string of n characters. Buffer is zapped each time!
 */
const char *
rpt( char ch, int n )
{
        int i;
        static char buf[200];

        n = IM_MIN( 190, n );

        for( i = 0; i < n; i++ )
                buf[i] = ch;
        buf[i] = '\0';

        return( buf );
}

/* Return a string of n spaces. Buffer is zapped each time!
 */
const char *
spc( int n )
{
        return( rpt( ' ', n ) );
}

static void
prettify_tree_sub( xmlNode *xnode, int indent )
{
        xmlNode *txt;
        xmlNode *next;

        for(;;) {
                next = xnode->next;

                /* According to memprof, this leaks :-( If you cut it out into
                 * a separate prog though, it's OK

                        FIXME ... how odd

                 */
                txt = xmlNewText( "\n" );
                xmlAddPrevSibling( xnode, txt );
                txt = xmlNewText( spc( indent ) );
                xmlAddPrevSibling( xnode, txt );

                if( xnode->children )
                        prettify_tree_sub( xnode->children, indent + 2 );

                if( !next )
                        break;

                xnode = next;
        }

        txt = xmlNewText( spc( indent - 2 ) );
        xmlAddNextSibling( xnode, txt );
        txt = xmlNewText( "\n" );
        xmlAddNextSibling( xnode, txt );
}

/* Walk an XML document, adding extra blank text elements so that it's easier
 * to read. Don't call me twice!
 */
void
prettify_tree( xmlDoc *xdoc )
{
        xmlNode *xnode = xmlDocGetRootElement( xdoc );

        prettify_tree_sub( xnode, 0 );
}
#endif /*DEBUG*/

/* Append XML to output fd.
 */
int 
im__writehist( IMAGE *im )
{
	xmlDoc *doc;
	char namespace[256];
	char *dump;
	int dump_size;

	assert( im->dtype == IM_OPENOUT );
	assert( im->fd != -1 );

	if( !(doc = xmlNewDoc( (xmlChar *) "1.0" )) )
		return( -1 );

        im_snprintf( namespace, 256, "%s/%d.%d.%d",
                NAMESPACE,
		IM_MAJOR_VERSION, IM_MINOR_VERSION, IM_MICRO_VERSION );
	if( !(doc->children = xmlNewDocNode( doc, 
			NULL, (xmlChar *) "root", NULL )) ||
                set_sprop( doc->children, "xmlns", namespace ) ||
		save_fields( im, doc->children ) ) {
		im_error( "im__writehist", "%s", _( "xml save error" ) );
                xmlFreeDoc( doc );
                return( -1 );
        }

	/* Bizarre double-cast stops a bogus gcc 4.1 compiler warning.
	 */
	xmlDocDumpMemory( doc, (xmlChar **) ((char *) &dump), &dump_size );
	if( !dump ) {
		im_error( "im__writehist", "%s", _( "xml save error" ) );
                xmlFreeDoc( doc );
                return( -1 );
	}

	if( im__write_extension_block( im, dump, dump_size ) ) {
                xmlFreeDoc( doc );
		xmlFree( dump );
                return( -1 );
        }

#ifdef DEBUG
{
	char *dump2;
	int dump_size2;

	/* Uncomment to have XML pretty-printed. Can be annoying during
 	 * debugging tho'
	 */
	prettify_tree( doc );

	xmlDocDumpMemory( doc, (xmlChar **) &dump2, &dump_size2 );
	if( !dump2 ) {
		im_error( "im__writehist", "%s", _( "xml save error" ) );
                xmlFreeDoc( doc );
		xmlFree( dump );
                return( -1 );
	}
	
	printf( "im__writehist: saved XML is: \"%s\"", dump2 );
	xmlFree( dump2 );
}
#endif /*DEBUG*/

	xmlFreeDoc( doc );
	xmlFree( dump );

	return( 0 );
}

/* Open the filename, read the header, some sanity checking.
 */
int
im_openin( IMAGE *image )
{
	/* We don't use im->sizeof_header here, but we know we're reading a
	 * VIPS image anyway.
	 */
	unsigned char header[IM_SIZEOF_HEADER];

	gint64 psize;
	gint64 rsize;

	image->dtype = IM_OPENIN;
	if( (image->fd = im__open_image_file( image->filename )) == -1 ) 
		return( -1 );
	if( read( image->fd, header, IM_SIZEOF_HEADER ) != IM_SIZEOF_HEADER ||
		im__read_header_bytes( image, header ) ) {
		im_error( "im__read_header", 
			_( "unable to read header for \"%s\", %s" ),
			image->filename, strerror( errno ) );
		return( -1 );
	}

	/* Predict and check the file size.
	 */
	psize = im__image_pixel_length( image );
	if( (rsize = im_file_length( image->fd )) == -1 ) 
		return( -1 );
	image->file_length = rsize;
	if( psize > rsize ) 
		im_warn( "im__read_header", 
			_( "unable to read data for \"%s\", %s" ),
			image->filename, _( "file has been truncated" ) );

	/* Set demand style. Allow the most permissive sort.
	 */
	image->dhint = IM_THINSTRIP;

	/* Set the history part of im descriptor. Don't return an error if this
	 * fails (due to eg. corrupted XML) because it's probably mostly
	 * harmless.
	 */
	if( im__readhist( image ) ) {
		im_warn( "im__read_header", _( "error reading XML: %s" ),
			im_error_buffer() );
		im_error_clear();
	}

	return( 0 );
}

/* Open, then mmap() read/write. This is old and deprecated API, use
 * im_vips_open() in preference.
 */
int
im_openinrw( IMAGE *image )
{
	if( im_openin( image ) )
		return( -1 );
	if( im_mapfilerw( image ) ) 
		return( -1 );
	image->data = image->baseaddr + image->sizeof_header;
	image->dtype = IM_MMAPINRW;

#ifdef DEBUG
	printf( "im_openinrw: completely mmap()ing \"%s\" read-write\n",
		image->filename );
#endif /*DEBUG*/

	return( 0 );
}

/* Open a VIPS image for reading and byte-swap the image data if necessary. A
 * ":w" at the end of the filename means we open read-write.
 */
IMAGE *
im_open_vips( const char *filename )
{
	char name[FILENAME_MAX];
	char mode[FILENAME_MAX];
	IMAGE *im;

	im_filename_split( filename, name, mode );

	if( !(im = im_init( name )) )
		return( NULL );
	if( mode[0] == 'w' ) {
		if( im_openinrw( im ) ) {
			im_close( im );
			return( NULL );
		}
		if( im_isMSBfirst( im ) != im_amiMSBfirst() ) {
			im_close( im );
			im_error( "im_open_vips", "%s", 
				_( "open for read-write for "
				"native format images only" ) );
			return( NULL );
		}
	}
	else {
		if( im_openin( im ) ) {
			im_close( im );
			return( NULL );
		}
	}

	/* Not in native format? And needs swapping?
	 */
	if( im_isMSBfirst( im ) != im_amiMSBfirst() &&
		im->Coding == IM_CODING_NONE &&
		im->BandFmt != IM_BANDFMT_CHAR &&
		im->BandFmt != IM_BANDFMT_UCHAR ) {
		IMAGE *im2;

		if( !(im2 = im_open( filename, "p" )) ) {
			im_close( im );
			return( NULL );
		}
		if( im_add_close_callback( im2, 
			(im_callback_fn) im_close, im, NULL ) ) {
			im_close( im );
			im_close( im2 );
			return( NULL );
		}
		if( im_copy_swap( im, im2 ) ) {
			im_close( im2 );
			return( NULL );
		}
		im = im2;
	}

	return( im );
}

IMAGE *
im_openout( const char *filename )
{	
	IMAGE *image;

	if( !(image = im_init( filename )) ) 
		return( NULL );
	image->dtype = IM_OPENOUT;

	return( image );
}

