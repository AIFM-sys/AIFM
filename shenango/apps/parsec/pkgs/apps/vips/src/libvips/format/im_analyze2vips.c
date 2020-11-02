/* Read a Analyze file. Old-style header (so called 7.5 format).
 * 
 * 3/8/05
 * 	- dbh.h header from Ralph Myers
 * 22/8/05
 * 	- better byteswapper
 * 12/5/09
 *	- fix signed/unsigned warning
 * 13/1/09
 * 	- try harder not to generate error messages in "isanalyze"
 * 4/2/10
 * 	- gtkdoc
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
 */
#define DEBUG

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /*HAVE_CONFIG_H*/
#include <vips/intl.h>

#include <ctype.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>

#include <vips/vips.h>
#include <vips/internal.h>

#include "dbh.h"

#ifdef WITH_DMALLOC
#include <dmalloc.h>
#endif /*WITH_DMALLOC*/

/* The things we can have in header fields. Can't use GType, since we want a
 * static value we can use in a declaration.
 */
typedef enum {
	BYTE,
	SHORT,
	INT,
	FLOAT, 
	STRING
} Type;

/* A field in the dsr header.
 */
typedef struct {
	const char *name;	/* Eg. "header_key.sizeof_hdr" */
	Type type;
	glong offset;		/* Offset in struct */
	int len;		/* Sizeof ... useful for string types */
} Field;

static Field dsr_header[] = {
	{ "dsr-header_key.sizeof_hdr", INT, 
		G_STRUCT_OFFSET( struct dsr, hk.sizeof_hdr ), 4 },
	{ "dsr-header_key.data_type", STRING, 
		G_STRUCT_OFFSET( struct dsr, hk.data_type ), 10 },
	{ "dsr-header_key.db_name", STRING, 
		G_STRUCT_OFFSET( struct dsr, hk.db_name ), 18 },
	{ "dsr-header_key.extents", INT, 
		G_STRUCT_OFFSET( struct dsr, hk.extents ), 4 },
	{ "dsr-header_key.session_error", SHORT, 
		G_STRUCT_OFFSET( struct dsr, hk.session_error ), 2 },
	{ "dsr-header_key.regular", BYTE, 
		G_STRUCT_OFFSET( struct dsr, hk.regular ), 1 },
	{ "dsr-header_key.hkey_un0", BYTE, 
		G_STRUCT_OFFSET( struct dsr, hk.hkey_un0 ), 1 },

	{ "dsr-image_dimension.dim[0]", SHORT, 
		G_STRUCT_OFFSET( struct dsr, dime.dim[0] ), 2 },
	{ "dsr-image_dimension.dim[1]", SHORT, 
		G_STRUCT_OFFSET( struct dsr, dime.dim[1] ), 2 },
	{ "dsr-image_dimension.dim[2]", SHORT, 
		G_STRUCT_OFFSET( struct dsr, dime.dim[2] ), 2 },
	{ "dsr-image_dimension.dim[3]", SHORT, 
		G_STRUCT_OFFSET( struct dsr, dime.dim[3] ), 2 },
	{ "dsr-image_dimension.dim[4]", SHORT, 
		G_STRUCT_OFFSET( struct dsr, dime.dim[4] ), 2 },
	{ "dsr-image_dimension.dim[5]", SHORT, 
		G_STRUCT_OFFSET( struct dsr, dime.dim[5] ), 2 },
	{ "dsr-image_dimension.dim[6]", SHORT, 
		G_STRUCT_OFFSET( struct dsr, dime.dim[6] ), 2 },
	{ "dsr-image_dimension.dim[7]", SHORT, 
		G_STRUCT_OFFSET( struct dsr, dime.dim[7] ), 2 },
	{ "dsr-image_dimension.vox_units[0]", BYTE, 
		G_STRUCT_OFFSET( struct dsr, dime.vox_units[0] ), 1 },
	{ "dsr-image_dimension.vox_units[1]", BYTE, 
		G_STRUCT_OFFSET( struct dsr, dime.vox_units[1] ), 1 },
	{ "dsr-image_dimension.vox_units[2]", BYTE, 
		G_STRUCT_OFFSET( struct dsr, dime.vox_units[2] ), 1 },
	{ "dsr-image_dimension.vox_units[3]", BYTE, 
		G_STRUCT_OFFSET( struct dsr, dime.vox_units[3] ), 1 },
	{ "dsr-image_dimension.cal_units[0]", BYTE, 
		G_STRUCT_OFFSET( struct dsr, dime.cal_units[0] ), 1 },
	{ "dsr-image_dimension.cal_units[1]", BYTE, 
		G_STRUCT_OFFSET( struct dsr, dime.cal_units[1] ), 1 },
	{ "dsr-image_dimension.cal_units[2]", BYTE, 
		G_STRUCT_OFFSET( struct dsr, dime.cal_units[2] ), 1 },
	{ "dsr-image_dimension.cal_units[3]", BYTE, 
		G_STRUCT_OFFSET( struct dsr, dime.cal_units[3] ), 1 },
	{ "dsr-image_dimension.cal_units[4]", BYTE, 
		G_STRUCT_OFFSET( struct dsr, dime.cal_units[4] ), 1 },
	{ "dsr-image_dimension.cal_units[5]", BYTE, 
		G_STRUCT_OFFSET( struct dsr, dime.cal_units[5] ), 1 },
	{ "dsr-image_dimension.cal_units[6]", BYTE, 
		G_STRUCT_OFFSET( struct dsr, dime.cal_units[6] ), 1 },
	{ "dsr-image_dimension.cal_units[7]", BYTE, 
		G_STRUCT_OFFSET( struct dsr, dime.cal_units[7] ), 1 },
	{ "dsr-image_dimension.data_type", SHORT, 
		G_STRUCT_OFFSET( struct dsr, dime.datatype ), 2 },
	{ "dsr-image_dimension.bitpix", SHORT, 
		G_STRUCT_OFFSET( struct dsr, dime.bitpix ), 2 },
	{ "dsr-image_dimension.dim_un0", SHORT, 
		G_STRUCT_OFFSET( struct dsr, dime.dim_un0 ), 2 },
	{ "dsr-image_dimension.pixdim[0]", FLOAT, 
		G_STRUCT_OFFSET( struct dsr, dime.pixdim[0] ), 4 },
	{ "dsr-image_dimension.pixdim[1]", FLOAT, 
		G_STRUCT_OFFSET( struct dsr, dime.pixdim[1] ), 4 },
	{ "dsr-image_dimension.pixdim[2]", FLOAT, 
		G_STRUCT_OFFSET( struct dsr, dime.pixdim[2] ), 4 },
	{ "dsr-image_dimension.pixdim[3]", FLOAT, 
		G_STRUCT_OFFSET( struct dsr, dime.pixdim[3] ), 4 },
	{ "dsr-image_dimension.pixdim[4]", FLOAT, 
		G_STRUCT_OFFSET( struct dsr, dime.pixdim[4] ), 4 },
	{ "dsr-image_dimension.pixdim[5]", FLOAT, 
		G_STRUCT_OFFSET( struct dsr, dime.pixdim[5] ), 4 },
	{ "dsr-image_dimension.pixdim[6]", FLOAT, 
		G_STRUCT_OFFSET( struct dsr, dime.pixdim[6] ), 4 },
	{ "dsr-image_dimension.pixdim[7]", FLOAT, 
		G_STRUCT_OFFSET( struct dsr, dime.pixdim[7] ), 4 },
	{ "dsr-image_dimension.vox_offset", FLOAT, 
		G_STRUCT_OFFSET( struct dsr, dime.vox_offset ), 4 },
	{ "dsr-image_dimension.cal_max", FLOAT, 
		G_STRUCT_OFFSET( struct dsr, dime.cal_max ), 4 },
	{ "dsr-image_dimension.cal_min", FLOAT, 
		G_STRUCT_OFFSET( struct dsr, dime.cal_min ), 4 },
	{ "dsr-image_dimension.compressed", INT, 
		G_STRUCT_OFFSET( struct dsr, dime.compressed ), 4 },
	{ "dsr-image_dimension.verified", INT, 
		G_STRUCT_OFFSET( struct dsr, dime.verified ), 4 },
	{ "dsr-image_dimension.glmax", INT, 
		G_STRUCT_OFFSET( struct dsr, dime.glmax ), 4 },
	{ "dsr-image_dimension.glmin", INT, 
		G_STRUCT_OFFSET( struct dsr, dime.glmin ), 4 },

	{ "dsr-data_history.descrip", STRING, 
		G_STRUCT_OFFSET( struct dsr, hist.descrip ), 80 },
	{ "dsr-data_history.aux_file", STRING, 
		G_STRUCT_OFFSET( struct dsr, hist.aux_file ), 24 },
	{ "dsr-data_history.orient", BYTE, 
		G_STRUCT_OFFSET( struct dsr, hist.orient ), 1 },
	{ "dsr-data_history.originator", STRING, 
		G_STRUCT_OFFSET( struct dsr, hist.originator ), 10 },
	{ "dsr-data_history.generated", STRING, 
		G_STRUCT_OFFSET( struct dsr, hist.generated ), 10 },
	{ "dsr-data_history.scannum", STRING, 
		G_STRUCT_OFFSET( struct dsr, hist.scannum ), 10 },
	{ "dsr-data_history.patient_id", STRING, 
		G_STRUCT_OFFSET( struct dsr, hist.patient_id ), 10 },
	{ "dsr-data_history.exp_date", STRING, 
		G_STRUCT_OFFSET( struct dsr, hist.exp_date ), 10 },
	{ "dsr-data_history.exp_time", STRING, 
		G_STRUCT_OFFSET( struct dsr, hist.exp_time ), 10 },
	{ "dsr-data_history.hist_un0", STRING, 
		G_STRUCT_OFFSET( struct dsr, hist.hist_un0 ), 3 },
	{ "dsr-data_history.views", INT, 
		G_STRUCT_OFFSET( struct dsr, hist.views ), 4 },
	{ "dsr-data_history.vols_added", INT, 
		G_STRUCT_OFFSET( struct dsr, hist.vols_added ), 4 },
	{ "dsr-data_history.start_field", INT, 
		G_STRUCT_OFFSET( struct dsr, hist.start_field ), 4 },
	{ "dsr-data_history.field_skip", INT, 
		G_STRUCT_OFFSET( struct dsr, hist.field_skip ), 4 },
	{ "dsr-data_history.omax", INT, 
		G_STRUCT_OFFSET( struct dsr, hist.omax ), 4 },
	{ "dsr-data_history.omin", INT, 
		G_STRUCT_OFFSET( struct dsr, hist.omin ), 4 },
	{ "dsr-data_history.smax", INT, 
		G_STRUCT_OFFSET( struct dsr, hist.smax ), 4 },
	{ "dsr-data_history.smin", INT, 
		G_STRUCT_OFFSET( struct dsr, hist.smin ), 4 }
};

/* Given a filename, generate the names for the header and the image data.
 *
 * Eg. 
 * 	"fred" 		-> "fred.hdr", "fred.img"
 * 	"fred.img" 	-> "fred.hdr", "fred.img"
 */
static void
generate_filenames( const char *path, char *header, char *image )
{
	char name[FILENAME_MAX];
	char mode[FILENAME_MAX];

	const char *olds[] = { ".img", ".hdr" };

	/* Take off any modifiers.
	 */
	im_filename_split( path, name, mode );

	im__change_suffix( name, header, FILENAME_MAX, ".hdr", olds, 2 );
	im__change_suffix( name, image, FILENAME_MAX, ".img", olds, 2 );
}

/* str is a str which may not be NULL-terminated. Return a pointer to a static
 * buffer with a NULL-terminated version so we can safely printf() the string.
 * Also, make sure the string is plain ascii.
 */
static char *
getstr( int mx, const char *str )
{
	static char buf[256];
	int i;

	assert( mx < 256 );

	strncpy( buf, str, mx );
	buf[mx]= '\0';

	/* How annoying, patient_id has some funny ctrlchars in that mess up
	 * xml encode later.
	 */
	for( i = 0; i < mx && buf[i]; i++ ) 
		if( !isascii( buf[i] ) || buf[i] < 32 )
			buf[i] = '@';

	return( buf );
}

#ifdef DEBUG
static void
print_dsr( struct dsr *d )
{
	int i;

	for( i = 0; i < IM_NUMBER( dsr_header ); i++ ) {
		printf( "%s = ", dsr_header[i].name );

		switch( dsr_header[i].type ) {
		case BYTE:
			printf( "%d\n", G_STRUCT_MEMBER( char, d, 
				dsr_header[i].offset ) );
			break;

		case SHORT:
			printf( "%d\n", G_STRUCT_MEMBER( short, d, 
				dsr_header[i].offset ) );
			break;

		case INT:
			printf( "%d\n", G_STRUCT_MEMBER( int, d, 
				dsr_header[i].offset ) );
			break;

		case FLOAT: 
			printf( "%g\n", G_STRUCT_MEMBER( float, d, 
				dsr_header[i].offset ) );
			break;

		case STRING:
			printf( "\"%s\"\n", getstr( dsr_header[i].len, 
				&G_STRUCT_MEMBER( char, d, 
					dsr_header[i].offset ) ) );
			break;

		default:
			assert( 0 );
		}
	}
}
#endif /*DEBUG*/

static struct dsr *
read_header( const char *header )
{
	struct dsr *d;
	unsigned int len;

	if( !(d = (struct dsr *) im__file_read_name( header, NULL, &len )) )
		return( NULL );
	if( len != sizeof( struct dsr ) ) {
		im_free( d );
		im_error( "im_analyze2vips", 
			"%s", _( "header file size incorrect" ) );
		return( NULL );
	}

	/* Ouch! Should check at configure time I guess.
	 */
	g_assert( sizeof( struct dsr ) == 348 );

	/* dsr headers are always SPARC byte order (MSB first). Do we need to 
	 * swap?
	 */
	if( !im_amiMSBfirst() ) {
		int i;

		for( i = 0; i < IM_NUMBER( dsr_header ); i++ ) {
			unsigned char *p;


			switch( dsr_header[i].type ) {
			case SHORT:
				p = &G_STRUCT_MEMBER( unsigned char, d, 
					dsr_header[i].offset );
				im__read_2byte( 1, p, &p );
				break;

			case INT:
			case FLOAT: 
				p = &G_STRUCT_MEMBER( unsigned char, d, 
					dsr_header[i].offset );
				im__read_4byte( 1, p, &p );
				break;

			case BYTE:
			case STRING:
				break;

			default:
				assert( 0 );
			}
		}
	}

	if( (int) len != d->hk.sizeof_hdr ) {
		im_free( d );
		return( NULL );
	}

	return( d );
}

/* Try to get VIPS header properties from a dsr.
 */
static int
get_vips_properties( struct dsr *d,
	int *width, int *height, int *bands, int *fmt )
{
	int i;

	if( d->dime.dim[0] < 2 || d->dime.dim[0] > 7 ) {
		im_error( "im_analyze2vips", 
			_( "%d-dimensional images not supported" ), 
			d->dime.dim[0] );
		return( -1 );
	}

	/* Size of base 2d images.
	 */
	*width = d->dime.dim[1];
	*height = d->dime.dim[2];

	for( i = 3; i <= d->dime.dim[0]; i++ )
		*height *= d->dime.dim[i];

	/* Check it's a datatype we can handle.
	 */
	switch( d->dime.datatype ) {
	case DT_UNSIGNED_CHAR:
		*bands = 1;
		*fmt = IM_BANDFMT_UCHAR;
		break;

	case DT_SIGNED_SHORT:
		*bands = 1;
		*fmt = IM_BANDFMT_SHORT;
		break;

	case DT_SIGNED_INT:
		*bands = 1;
		*fmt = IM_BANDFMT_INT;
		break;

	case DT_FLOAT:
		*bands = 1;
		*fmt = IM_BANDFMT_FLOAT;
		break;

	case DT_COMPLEX:
		*bands = 1;
		*fmt = IM_BANDFMT_COMPLEX;
		break;

	case DT_DOUBLE:
		*bands = 1;
		*fmt = IM_BANDFMT_DOUBLE;
		break;

	case DT_RGB:
		*bands = 3;
		*fmt = IM_BANDFMT_UCHAR;
		break;

	default:
		im_error( "im_analyze2vips", 
			_( "datatype %d not supported" ), d->dime.datatype );
		return( -1 );
	}

#ifdef DEBUG
	printf( "get_vips_properties: width = %d\n", *width );
	printf( "get_vips_properties: height = %d\n", *height );
	printf( "get_vips_properties: bands = %d\n", *bands );
	printf( "get_vips_properties: fmt = %d\n", *fmt );
#endif /*DEBUG*/

	return( 0 );
}

static void
attach_meta( IMAGE *out, struct dsr *d )
{
	int i;

	im_meta_set_blob( out, "dsr", 
		(im_callback_fn) im_free, d, d->hk.sizeof_hdr );

	for( i = 0; i < IM_NUMBER( dsr_header ); i++ ) {
		switch( dsr_header[i].type ) {
		case BYTE:
			im_meta_set_int( out, dsr_header[i].name,
				G_STRUCT_MEMBER( char, d, 
					dsr_header[i].offset ) );
			break;

		case SHORT:
			im_meta_set_int( out, dsr_header[i].name,
				G_STRUCT_MEMBER( short, d, 
					dsr_header[i].offset ) );
			break;

		case INT:
			im_meta_set_int( out, dsr_header[i].name,
				G_STRUCT_MEMBER( int, d, 
					dsr_header[i].offset ) );
			break;

		case FLOAT: 
			im_meta_set_double( out, dsr_header[i].name,
				G_STRUCT_MEMBER( float, d, 
					dsr_header[i].offset ) );
			break;

		case STRING:
			im_meta_set_string( out, dsr_header[i].name,
				getstr( dsr_header[i].len, 
					&G_STRUCT_MEMBER( char, d, 
						dsr_header[i].offset ) ) );
			break;

		default:
			assert( 0 );
		}
	}
}

static int
isanalyze( const char *filename )
{
	char header[FILENAME_MAX];
	char image[FILENAME_MAX];
	struct dsr *d;
	int width, height;
	int bands;
	int fmt;

	generate_filenames( filename, header, image );
	if( !im_existsf( "%s", header ) )
		return( 0 );
	if( !(d = read_header( header )) )
		return( 0 );

#ifdef DEBUG
	print_dsr( d );
#endif /*DEBUG*/

	if( get_vips_properties( d, &width, &height, &bands, &fmt ) ) {
		im_free( d );
		return( 0 );
	}
	im_free( d );

	return( 1 );
}

static int
analyze2vips_header( const char *filename, IMAGE *out )
{
	char header[FILENAME_MAX];
	char image[FILENAME_MAX];
	struct dsr *d;
	int width, height;
	int bands;
	int fmt;

	generate_filenames( filename, header, image );
	if( !(d = read_header( header )) ) 
		return( -1 );

#ifdef DEBUG
	print_dsr( d );
#endif /*DEBUG*/

	if( get_vips_properties( d, &width, &height, &bands, &fmt ) ) {
		im_free( d );
		return( -1 );
	}

	im_initdesc( out, width, height, bands, im_bits_of_fmt( fmt ), fmt, 
		IM_CODING_NONE, 
		bands == 1 ? IM_TYPE_B_W : IM_TYPE_sRGB, 
		1.0, 1.0,
		0, 0 );

	attach_meta( out, d );

	return( 0 );
}

/**
 * im_analyze2vips:
 * @filename: file to load
 * @out: image to write to
 *
 * Load an Analyze 6.0 file. If @filename is "fred.img", this will look for
 * an image header called "fred.hdr" and pixel data in "fred.img". You can
 * also load "fred" or "fred.hdr".
 *
 * Images are
 * loaded lazilly and byte-swapped, if necessary. The Analyze metadata is read
 * and attached.
 *
 * See also: #VipsFormat, im_meta_get(), im_grid().
 *
 * Returns: 0 on success, -1 on error.
 */
int
im_analyze2vips( const char *filename, IMAGE *out )
{
	char header[FILENAME_MAX];
	char image[FILENAME_MAX];
	struct dsr *d;
	IMAGE *t[2];
	int width, height;
	int bands;
	int fmt;

	generate_filenames( filename, header, image );
	if( !(d = read_header( header )) ) 
		return( -1 );

#ifdef DEBUG
	print_dsr( d );
#endif /*DEBUG*/

	if( get_vips_properties( d, &width, &height, &bands, &fmt ) ||
		im_open_local_array( out, t, 2, "im_analyze2vips", "p" ) ||
		im_raw2vips( image, t[0], width, height,
			bands * im_bits_of_fmt( fmt ) / 8, 0 ) || 
		im_copy_morph( t[0], t[1], bands, fmt, IM_CODING_NONE ) ||
		im_copy_native( t[1], out, TRUE ) ) {
		im_free( d );
		return( -1 );
	}

	attach_meta( out, d );

	return( 0 );
}

static const char *analyze_suffs[] = { ".img", ".hdr", NULL };

static VipsFormatFlags
analyze_flags( const char *filename )
{
	return( VIPS_FORMAT_PARTIAL );
}

/* analyze format adds no new members.
 */
typedef VipsFormat VipsFormatAnalyze;
typedef VipsFormatClass VipsFormatAnalyzeClass;

static void
vips_format_analyze_class_init( VipsFormatAnalyzeClass *class )
{
	VipsObjectClass *object_class = (VipsObjectClass *) class;
	VipsFormatClass *format_class = (VipsFormatClass *) class;

	object_class->nickname = "analyze";
	object_class->description = _( "Analyze 6.0" );

	format_class->is_a = isanalyze;
	format_class->header = analyze2vips_header;
	format_class->load = im_analyze2vips;
	format_class->get_flags = analyze_flags;
	format_class->suffs = analyze_suffs;
}

static void
vips_format_analyze_init( VipsFormatAnalyze *object )
{
}

G_DEFINE_TYPE( VipsFormatAnalyze, vips_format_analyze, VIPS_TYPE_FORMAT );

