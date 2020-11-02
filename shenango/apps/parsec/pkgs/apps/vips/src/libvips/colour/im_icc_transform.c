/* Transform images with little cms
 *
 * 26/4/02 JC
 * 26/8/05
 * 	- attach profiles and intents to output images
 * 	- added im_icc_import_embedded() to import with an embedded profile
 * 12/5/06
 * 	- lock around cmsDoTransform
 * 23/1/07
 * 	- set RGB16 on 16-bit RGB export
 * 6/4/09
 * 	- catch lcms error messages
 * 2/11/09
 * 	- gtkdoc
 * 	- small cleanups
 * 	- call attach_profile() before im_wrapone() so the profile will get
 * 	  written if we are wrinting to a file
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

#ifndef HAVE_LCMS

#include <vips/vips.h>
#include <vips/thread.h>

int
im_icc_present( void )
{
	return( 0 );
}

int
im_icc_transform( IMAGE *in, IMAGE *out, 
	const char *input_profile_filename,
	const char *output_profile_filename,
	VipsIntent intent )
{
	im_error( "im_icc_transform", "%s",
		_( "lcms library not linked to this VIPS" ) );

	return( -1 );
}

int
im_icc_import( IMAGE *in, IMAGE *out, 
	const char *input_profile_filename, VipsIntent intent )
{
	im_error( "im_icc_import", "%s",
		_( "lmcs library not linked to this VIPS" ) );

	return( -1 );
}

int
im_icc_import_embedded( IMAGE *in, IMAGE *out, VipsIntent intent )
{
	im_error( "im_icc_import", "%s",
		_( "lmcs library not linked to this VIPS" ) );

	return( -1 );
}

int
im_icc_export_depth( IMAGE *in, IMAGE *out, int depth,
	const char *output_profile_filename, VipsIntent intent )
{
	im_error( "im_icc_export_depth", "%s", 
		_( "lmcs library not linked to this VIPS" ) );

	return( -1 );
}

int
im_icc_ac2rc( IMAGE *in, IMAGE *out, const char *profile_filename )
{
	im_error( "im_icc_ac2rc", "%s", 
		_( "lmcs library not linked to this VIPS" ) );

	return( -1 );
}

#else

#include <stdio.h>
#include <math.h>
#include <assert.h>

/* Has to be before VIPS to avoid nameclashes.
 */
#include <lcms.h>
#include <icc34.h>

#include <vips/vips.h>
#include <vips/colour.h>
#include <vips/region.h>
#include <vips/util.h>

#ifdef WITH_DMALLOC
#include <dmalloc.h>
#endif /*WITH_DMALLOC*/

/**
 * VipsIntent:
 * @IM_INTENT_PERCEPTUAL:
 * @IM_INTENT_RELATIVE_COLORIMETRIC:
 * @IM_INTENT_SATURATION:
 * @IM_INTENT_ABSOLUTE_COLORIMETRIC:
 *
 * The rendering intent. #IM_INTENT_ABSOLUTE_COLORIMETRIC is best for
 * scientific work, #IM_INTENT_RELATIVE_COLORIMETRIC is usually best for 
 * accurate communication with other imaging libraries.
 */

/* Call lcms with up to this many pixels at once.
 */
#define PIXEL_BUFFER_SIZE (10000)

static const char *
decode_intent( VipsIntent intent )
{
	switch( intent ) {
	case IM_INTENT_PERCEPTUAL:		return( "PERCEPTUAL" );
	case IM_INTENT_RELATIVE_COLORIMETRIC:	return( "RELATIVE" );
	case IM_INTENT_SATURATION:		return( "SATURATION" );
	case IM_INTENT_ABSOLUTE_COLORIMETRIC:	return( "ABSOLUTE" );
	default:				return( "<unknown>" );
	}
}

/**
 * im_icc_present:
 *
 * VIPS can optionally be built without the ICC library. Use this function to
 * test for its availability. 
 *
 * Returns: non-zero if the ICC library is present.
 */
int
im_icc_present( void )
{
	return( 1 );
}

/* Global state for a transform.
 */
typedef struct {
	IMAGE *in;
	IMAGE *out;
	const char *input_profile_filename;
	const char *output_profile_filename;
	VipsIntent intent;

	cmsHPROFILE in_profile;
	cmsHPROFILE out_profile;
	cmsHTRANSFORM trans;

	/* We need to single-thread calls to LCMS.
	 */
	GMutex *lock;
} Icc;

/* Error from lcms.
 */
static int 
icc_error( int code, const char *text )
{
	if( code == LCMS_ERRC_WARNING )
		im_warn( "im_icc", "%s", text );
	else
		im_error( "im_icc", "%s", text );

	return( 0 );
}

static int
icc_destroy( Icc *icc )
{
	IM_FREEF( cmsDeleteTransform, icc->trans );
	IM_FREEF( cmsCloseProfile, icc->in_profile );
	IM_FREEF( cmsCloseProfile, icc->out_profile );
	IM_FREEF( g_mutex_free, icc->lock );

	return( 0 );
}

static Icc *
icc_new( IMAGE *in, IMAGE *out, VipsIntent intent )
{
	Icc *icc;

	/* Ask lcms not to abort on error.
	 */
	cmsErrorAction( LCMS_ERROR_IGNORE );
	cmsSetErrorHandler( icc_error );

	if( !(icc = IM_NEW( out, Icc )) )
		 return( NULL );

	icc->in = in;
	icc->out = out;
	icc->input_profile_filename = NULL;
	icc->output_profile_filename = NULL;
	icc->intent = intent;
	icc->in_profile = 0;
	icc->out_profile = 0;
	icc->trans = 0;
	icc->lock = g_mutex_new();

	if( im_add_close_callback( out, 
		(im_callback_fn) icc_destroy, icc, NULL ) )
		return( NULL );

	return( icc );
}

static Icc *
icc_new_file( IMAGE *in, IMAGE *out, 
	const char *input_profile_filename, 
	const char *output_profile_filename,
	VipsIntent intent )
{
	Icc *icc;

	if( !(icc = icc_new( in, out, intent )) )
		 return( NULL );

	if( input_profile_filename ) {
		icc->input_profile_filename = 
			im_strdup( out, input_profile_filename );
		if( !(icc->in_profile = cmsOpenProfileFromFile(
			input_profile_filename, "r" )) )
			im_error( "im_icc_transform", 
				_( "unable to open profile \"%s\"" ), 
				input_profile_filename );
	}

	if( output_profile_filename ) {
		icc->output_profile_filename = 
			im_strdup( out, output_profile_filename );
		if( !(icc->out_profile = cmsOpenProfileFromFile(
			output_profile_filename, "r" )) )
			im_error( "im_icc_transform", 
				_( "unable to open profile \"%s\"" ), 
				output_profile_filename );
	}

	if( !output_profile_filename )
		icc->out_profile = cmsCreateLabProfile( NULL );
	if( !input_profile_filename )
		icc->in_profile = cmsCreateLabProfile( NULL );

	if( !icc->in_profile || !icc->out_profile ) {
		im_error( "im_icc_transform", 
			"%s", _( "unable to create profiles" ) );
		return( NULL );
	}

	return( icc );
}

static Icc *
icc_new_mem( IMAGE *in, IMAGE *out, 
	void *data, int data_length, 
	VipsIntent intent )
{
	Icc *icc;

	if( !(icc = icc_new( in, out, intent )) )
		 return( NULL );

	if( !(icc->in_profile = cmsOpenProfileFromMem( data, data_length )) ) {
		im_error( "im_icc_transform", 
			"%s", _( "unable to read profile" ) );
		return( NULL );
	}
	icc->out_profile = cmsCreateLabProfile( NULL );

	return( icc );
}

/* Pack a buffer of floats into lcms's fixed-point formats. Cut from
 * lcms-1.0.8.
 */
static void 
encode_lab( float *lab, WORD *fixed, int n )
{
	int i;

	for( i = 0; i < n; i++ ) {
		float L = lab[0];
		float a = lab[1];
		float b = lab[2];

		if( L < 0 ) 
			L = 0;
		if( L > 100. ) 
			L = 100.;

		if( a < -128. ) 
			a = -128;
		if( a > 127.9961 ) 
			a = 127.9961;
		if( b < -128. ) 
			b = -128;
		if( b > 127.9961 ) 
			b = 127.9961;

		fixed[0] = L *  652.800 + 0.5;
		fixed[1] = (a + 128.0) * 256.0 + 0.5;
		fixed[2] = (b + 128.0) * 256.0 + 0.5;

		lab += 3;
		fixed += 3;
	}
}

static void 
decode_lab( WORD *fixed, float *lab, int n )
{
	int i;

        for( i = 0; i < n; i++ ) {
                lab[0] = (double) fixed[0] / 652.800;
                lab[1] = ((double) fixed[1] / 256.0) - 128.0;
                lab[2] = ((double) fixed[2] / 256.0) - 128.0;

                lab += 3;
                fixed += 3;
        }
}

static void
transform_buf( PEL *in, PEL *out, int n, Icc *icc )
{
	g_mutex_lock( icc->lock );
	cmsDoTransform( icc->trans, in, out, n );
	g_mutex_unlock( icc->lock );
}

static int
attach_profile( IMAGE *im, const char *filename )
{
	char *data;
	unsigned int data_length;

	if( !(data = im__file_read_name( filename, VIPS_ICC_DIR, 
		&data_length )) ) 
		return( -1 );
	if( im_meta_set_blob( im, IM_META_ICC_NAME, 
		(im_callback_fn) im_free, data, data_length ) ) {
		im_free( data );
		return( -1 );
	}

	return( 0 );
}

/**
 * im_icc_transform:
 * @in: input image
 * @out: output image
 * @input_profile_filename: get the input profile from here
 * @output_profile_filename: get the output profile from here
 * @intent: transform with this intent
 *
 * Transform an image with the ICC library. The input image is moved to
 * profile-connection space with the input profile and then to the output
 * space with the output profile.
 *
 * Use im_icc_import() and im_icc_export_depth() to do either the first or 
 * second half of this operation in isolation.
 *
 * See also: im_icc_import(), im_icc_export_depth().
 *
 * Returns: 0 on success, -1 on error.
 */
int
im_icc_transform( IMAGE *in, IMAGE *out, 
	const char *input_profile_filename,
	const char *output_profile_filename,
	VipsIntent intent )
{
	Icc *icc;
	DWORD in_icc_format;
	DWORD out_icc_format;

	if( im_check_uncoded( "im_icc_transform", in ) )
		return( -1 ); 

	if( !(icc = icc_new_file( in, out, 
		input_profile_filename, output_profile_filename, intent )) )
		return( -1 );

	if( !cmsIsIntentSupported( icc->in_profile, 
		intent, LCMS_USED_AS_INPUT ) )
		im_warn( "im_icc_transform", 
			_( "intent %d (%s) not supported by "
			"profile \"%s\"; falling back to default intent "
			"(usually PERCEPTUAL)" ), 
			intent, decode_intent( intent ),
			input_profile_filename );

	if( !cmsIsIntentSupported( icc->out_profile, 
		intent, LCMS_USED_AS_OUTPUT ) )
		im_warn( "im_icc_transform", 
			_( "intent %d (%s) not supported by "
			"profile \"%s\"; falling back to default intent "
			"(usually PERCEPTUAL)" ), 
			intent, decode_intent( intent ),
			output_profile_filename );

	switch( cmsGetColorSpace( icc->in_profile ) ) {
	case icSigCmykData:
		if( in->Bands != 4 ) {
			im_error( "im_icc_transform", 
				"%s", _( "CMYK input profile "
				"needs a 4 band input image" ) );
			return( -1 );
		}
		in_icc_format = COLORSPACE_SH( PT_CMYK ) | CHANNELS_SH( 4 );
		break;

	case icSigRgbData:
		if( in->Bands != 3 ) {
			im_error( "im_icc_transform", 
				"%s", _( "RGB input profile "
				"needs a 3 band input image" ) );
			return( -1 );
		}
		in_icc_format = COLORSPACE_SH( PT_RGB ) | CHANNELS_SH( 3 );
		break;

	default:
		im_error( "im_icc_transform", 
			_( "unimplemented input color space 0x%x" ), 
			cmsGetColorSpace( icc->in_profile ) );
		return( -1 );
	}

	/* Prepare the output image.
	 */
	if( im_cp_desc( out, in ) )
		return( -1 );

	switch( cmsGetColorSpace( icc->out_profile ) ) {
	case icSigCmykData:
		out->Type = IM_TYPE_CMYK;
		out->BandFmt = IM_BANDFMT_UCHAR;
		out->Bands = 4;
		out_icc_format = TYPE_CMYK_8;
		break;

	case icSigRgbData:
		out->Type = IM_TYPE_RGB;
		out->BandFmt = IM_BANDFMT_UCHAR;
		out->Bands = 3;
		out_icc_format = TYPE_RGB_8;
		break;

	default:
		im_error( "im_icc_transform", 
			_( "unimplemented output color space 0x%x" ), 
			cmsGetColorSpace( icc->out_profile ) );
		return( -1 );
	}

	switch( in->BandFmt ) {
	case IM_BANDFMT_UCHAR:
		in_icc_format |= BYTES_SH( 1 );
		break;
	
	case IM_BANDFMT_USHORT:
		in_icc_format |= BYTES_SH( 2 );
		break;

	default:
		im_error( "im_icc_transform", 
			"%s", _( "uchar or ushort input only" ) );
		return( -1 );
	}

	if( !(icc->trans = cmsCreateTransform( icc->in_profile, in_icc_format,
		icc->out_profile, out_icc_format, intent, 0 )) )
		return( -1 );

	if( attach_profile( out, output_profile_filename ) )
		return( -1 );

	/* Process!
	 */
	if( im_wrapone( in, out, 
		(im_wrapone_fn) transform_buf, icc, NULL ) )
		return( -1 );

	return( 0 );
}

static void
import_buf( PEL *in, float *out, int n, Icc *icc )
{
	/* Buffer of encoded 16-bit pixels we write to.
	 */
	WORD encoded[3 * PIXEL_BUFFER_SIZE];

	while( n > 0 ) {
		const int chunk = IM_MIN( n, PIXEL_BUFFER_SIZE );

		g_mutex_lock( icc->lock );
		cmsDoTransform( icc->trans, in, encoded, chunk );
		g_mutex_unlock( icc->lock );
		decode_lab( encoded, out, chunk );

		in += chunk * IM_IMAGE_SIZEOF_PEL( icc->in );
		out += chunk * 3;
		n -= chunk;
	}
}

static int
icc_import( IMAGE *in, IMAGE *out, Icc *icc )
{
	DWORD icc_format;

	if( im_check_uncoded( "im_icc_import", in ) )
		return( -1 ); 

	if( !cmsIsIntentSupported( icc->in_profile, 
		icc->intent, LCMS_USED_AS_INPUT ) )
		im_warn( "im_icc_import", 
			_( "intent %d (%s) not supported by "
			"profile; falling back to default intent "
			"(usually PERCEPTUAL)" ), 
			icc->intent, decode_intent( icc->intent ) );

	/* Prepare the output image.
	 */
	if( im_cp_desc( out, in ) )
		return( -1 );
	out->Type = IM_TYPE_LAB;
	out->BandFmt = IM_BANDFMT_FLOAT;
	out->Bands = 3;

	switch( cmsGetColorSpace( icc->in_profile ) ) {
	case icSigCmykData:
		if( in->Bands != 4 ) {
			im_error( "im_icc_import", 
				"%s", _( "CMYK profile needs a "
				"4 band input image" ) );
			return( -1 );
		}
		icc_format = COLORSPACE_SH( PT_CMYK ) | CHANNELS_SH( 4 );
		break;

	case icSigRgbData:
		if( in->Bands != 3 ) {
			im_error( "im_icc_import", 
				"%s", _( "RGB profile needs a "
				"3 band input image" ) );
			return( -1 );
		}
		icc_format = COLORSPACE_SH( PT_RGB ) | CHANNELS_SH( 3 );
		break;

	default:
		im_error( "im_icc_import", _( "unimplemented input color "
			"space 0x%x" ), cmsGetColorSpace( icc->in_profile ) );
		return( -1 );
	}

	switch( in->BandFmt ) {
	case IM_BANDFMT_UCHAR:
		icc_format |= BYTES_SH( 1 );
		break;

	case IM_BANDFMT_USHORT:
		icc_format |= BYTES_SH( 2 );
		break;

	default:
		im_error( "im_icc_transform",
			"%s", _( "uchar or ushort input only" ) );
		return( -1 );
	}

	if( !(icc->trans = cmsCreateTransform( icc->in_profile, icc_format,
		icc->out_profile, TYPE_Lab_16, icc->intent, 0 )) )
		return( -1 );

	/* Process!
	 */
	if( im_wrapone( in, out, (im_wrapone_fn) import_buf, icc, NULL ) )
		return( -1 );

	return( 0 );
}

/**
 * im_icc_import:
 * @in: input image
 * @out: output image
 * @input_profile_filename: get the input profile from here
 * @intent: transform with this intent
 *
 * Import an image with the ICC library. The input image in device space 
 * is moved to D65 LAB with the input profile.
 *
 * See also: im_icc_transform(), im_icc_import_embedded().
 *
 * Returns: 0 on success, -1 on error.
 */
int
im_icc_import( IMAGE *in, IMAGE *out, 
	const char *input_profile_filename, VipsIntent intent )
{
	Icc *icc;

	if( !(icc = icc_new_file( in, out, 
		input_profile_filename, NULL, intent )) ||
		icc_import( in, out, icc ) )
		return( -1 );

	return( 0 );
}

/**
 * im_icc_import_embedded:
 * @in: input image
 * @out: output image
 * @intent: transform with this intent
 *
 * Import an image with the ICC library. The input image in device space 
 * is moved to D65 LAB with the input profile attached to the image under the
 * name #IM_META_ICC_NAME.
 *
 * See also: im_icc_transform(), im_icc_import().
 *
 * Returns: 0 on success, -1 on error.
 */
int
im_icc_import_embedded( IMAGE *in, IMAGE *out, VipsIntent intent )
{
	Icc *icc;
	void *data;
	size_t data_length;

	if( im_header_get_typeof( in, IM_META_ICC_NAME ) == 0 ) {
		im_error( "im_icc_import_embedded", 
			"%s", _( "no embedded profile" ) );
		return( -1 );
	}

	if( im_meta_get_blob( in, IM_META_ICC_NAME, &data, &data_length ) ||
		!(icc = icc_new_mem( in, out, data, data_length, intent )) ||
		icc_import( in, out, icc ) )
		return( -1 );

	return( 0 );
}

static void
export_buf( float *in, PEL *out, int n, Icc *icc )
{
	/* Buffer of encoded 16-bit pixels we transform.
	 */
	WORD encoded[3 * PIXEL_BUFFER_SIZE];

	while( n > 0 ) {
		const int chunk = IM_MIN( n, PIXEL_BUFFER_SIZE );

		encode_lab( in, encoded, chunk );
		g_mutex_lock( icc->lock );
		cmsDoTransform( icc->trans, encoded, out, chunk );
		g_mutex_unlock( icc->lock );

		in += chunk * 3;
		out += chunk * IM_IMAGE_SIZEOF_PEL( icc->out );
		n -= chunk;
	}
}

/**
 * im_icc_export_depth:
 * @in: input image
 * @out: output image
 * @depth: depth to export at
 * @output_profile_filename: use this profile
 * @intent: transform with this intent
 *
 * Export an image with the ICC library. The input image in 
 * D65 LAB is transformed to device space using the supplied profile.
 * @depth can be 8 or 16, for 8 or 16-bit image export.
 *
 * See also: im_icc_transform(), im_icc_import().
 *
 * Returns: 0 on success, -1 on error.
 */
int
im_icc_export_depth( IMAGE *in, IMAGE *out, int depth,
	const char *output_profile_filename, VipsIntent intent )
{ 
	Icc *icc;
	DWORD icc_format;

	/* Do IM_CODING_LABQ too.
	 */
	if( in->Coding == IM_CODING_LABQ ) {
		IMAGE *t1 = im_open_local( out, "im_icc_export:1", "p" );

		if( !t1 || im_LabQ2Lab( in, t1 ) )
			return( -1 );

		in = t1;
	}

	/* Do IM_CODING_RAD.
	 */
	if( in->Coding == IM_CODING_RAD ) {
		IMAGE *t1 = im_open_local( out, "im_icc_export:1", "p" );

		if( !t1 || im_rad2float( in, t1 ) )
			return( -1 );

		in = t1;
	}

	/* Check input image.
	 */
	if( im_check_uncoded( "im_icc_export", in ) ||
		im_check_bands( "im_icc_export", in, 3 ) ||
		im_check_format( "im_icc_export", in, IM_BANDFMT_FLOAT ) )
		return( -1 );

	if( depth != 8 && depth != 16 ) {
		im_error( "im_icc_export", "%s", _( "unsupported bit depth" ) );
		return( -1 ); 
	}

	if( !(icc = icc_new_file( in, out, 
		NULL, output_profile_filename, intent )) )
		return( -1 );

	if( !cmsIsIntentSupported( icc->out_profile, 
		intent, LCMS_USED_AS_OUTPUT ) )
		im_warn( "im_icc_export", 
			_( "intent %d (%s) not supported by "
			"profile \"%s\"; falling back to default intent "
			"(usually PERCEPTUAL)" ), 
			intent, decode_intent( intent ),
			output_profile_filename );

	/* Prepare the output image.
	 */
	if( im_cp_desc( out, in ) )
		return( -1 );

	switch( cmsGetColorSpace( icc->out_profile ) ) {
	case icSigCmykData:
		out->Type = IM_TYPE_CMYK;
		out->BandFmt = depth == 8 ? 
			IM_BANDFMT_UCHAR : IM_BANDFMT_USHORT;
		out->Bands = 4;
		icc_format = depth == 8 ? TYPE_CMYK_8 : TYPE_CMYK_16;
		break;

	case icSigRgbData:
		out->Type = depth == 8 ? 
			IM_TYPE_RGB : IM_TYPE_RGB16;
		out->BandFmt = depth == 8 ? 
			IM_BANDFMT_UCHAR : IM_BANDFMT_USHORT;
		out->Bands = 3;
		icc_format = depth == 8 ? TYPE_RGB_8 : TYPE_RGB_16;
		break;

	default:
		im_error( "im_icc_export", _( "unimplemented output color "
			"space 0x%x" ), cmsGetColorSpace( icc->out_profile ) );
		return( -1 );
	}

	if( !(icc->trans = cmsCreateTransform( icc->in_profile, TYPE_Lab_16,
		icc->out_profile, icc_format, intent, 0 )) )
		return( -1 );

	if( attach_profile( out, output_profile_filename ) )
		return( -1 );

	/* Process!
	 */
	if( im_wrapone( in, out, (im_wrapone_fn) export_buf, icc, NULL ) )
		return( -1 );

	return( 0 );
}

/**
 * im_icc_ac2rc:
 * @in: input image
 * @out: output image
 * @profile_filename: use this profile
 *
 * Transform an image from absolute to relative colorimetry using the
 * MediaWhitePoint stored in the ICC profile.
 *
 * See also: im_icc_transform(), im_icc_import().
 *
 * Returns: 0 on success, -1 on error.
 */
int
im_icc_ac2rc( IMAGE *in, IMAGE *out, const char *profile_filename )
{
	cmsHPROFILE profile;
	cmsCIEXYZ media;

	double add[3];
	double mul[3];

	IMAGE *t[2];

	if( !(profile = cmsOpenProfileFromFile( profile_filename, "r" )) )
		return( -1 );

	if( !cmsTakeMediaWhitePoint( &media, profile ) ) {
		im_error( "im_icc_ac2rc", "%s", _( "unable to get media "
			"white point" ) );
		return( -1 );
	}

	cmsCloseProfile( profile );

	add[0] = 0.0;
	add[1] = 0.0;
	add[2] = 0.0;

	mul[0] = IM_D50_X0 / (media.X * 100.0);
	mul[1] = IM_D50_Y0 / (media.Y * 100.0);
	mul[2] = IM_D50_Z0 / (media.Z * 100.0);

	/* Do IM_CODING_LABQ too.
	 */
	if( in->Coding == IM_CODING_LABQ ) {
		IMAGE *t1 = im_open_local( out, "im_icc_ac2rc-1", "p" );

		if( !t1 || im_LabQ2Lab( in, t1 ) )
			return( -1 );

		in = t1;
	}

	/* Do IM_CODING_RAD.
	 */
	if( in->Coding == IM_CODING_RAD ) {
		IMAGE *t1 = im_open_local( out, "im_icc_export:1", "p" );

		if( !t1 || im_rad2float( in, t1 ) )
			return( -1 );

		in = t1;
	}

	if( im_open_local_array( out, t, 2, "im_icc_ac2rc-2", "p" ) ||
		im_Lab2XYZ_temp( in, t[0], IM_D50_X0, IM_D50_Y0, IM_D50_Z0 ) ||
		im_lintra_vec( 3, mul, t[0], add, t[1] ) || 
		im_XYZ2Lab_temp( t[1], out, IM_D50_X0, IM_D50_Y0, IM_D50_Z0 ) )
		return( -1 );

	return( 0 );
}

#endif /*HAVE_LCMS*/
