/* Function dispatch tables for arithmetic.
 *
 * J. Cupitt, 8/4/93.
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

#include <vips/vips.h>

#ifdef WITH_DMALLOC
#include <dmalloc.h>
#endif /*WITH_DMALLOC*/

/**
 * SECTION: colour
 * @short_description: colour operators
 * @stability: Stable
 * @see_also: <link linkend="libvips-arithmetic">arithmetic</link>
 * @include: vips/vips.h
 *
 * These operators let you transform coordinates and images between colour 
 * spaces, calculate colour differences, and move 
 * to and from device spaces.
 *
 * Radiance images have four 8-bits bands and store 8 bits of R, G and B and
 * another 8 bits of exponent, common to all channels. They are widely used in
 * the HDR imaging community.
 *
 *
 * The colour functions can be divided into three main groups. First, 
 * functions to transform images between the different colour spaces supported 
 * by VIPS: <emphasis>RGB</emphasis> (also referred to as
 * <emphasis>disp</emphasis>), <emphasis>sRGB</emphasis>,  
 * <emphasis>XYZ</emphasis>, <emphasis>Yxy</emphasis>, 
 * <emphasis>Lab</emphasis>, <emphasis>LabQ</emphasis>, 
 * <emphasis>LabS</emphasis>, <emphasis>LCh</emphasis> and
 * <emphasis>UCS</emphasis>). Secondly, there are a set of operations for 
 * calculating colour difference metrics. Finally, VIPS wraps LittleCMS and
 * uses it to provide a set of operations for reading and writing images with
 * ICC profiles.
 *
 * This figure shows how the VIPS colour spaces interconvert:
 *
 * <inlinegraphic fileref="interconvert.png" format="PNG" />
 *
 * The colour spaces supported by VIPS are:
 *
 * <itemizedlist>
 *   <listitem>
 *     <para>
 *       <emphasis><code>LabQ</code></emphasis>
 *
 *	 This is the principal VIPS colorimetric storage format. 
 * 	 LabQ images have four 8-bit bands and store 10 bits of L and 11 bits 
 * 	 of a and b.
 *
 * 	 You cannot perform calculations on <code>LabQ</code> images (they are
 * 	 tagged with %IM_CODING_LABQ), though a few operations such as
 * 	 im_extract_area() will work directly with them.
 *     </para>
 *   </listitem>
 *   <listitem>
 *     <para>
 *       <emphasis><code>LabS</code></emphasis>
 *
 *	 This format represents coordinates in CIELAB space as a 
 *	 three-band #IM_BANDFMT_SHORT image, scaled to fit the full range of 
 *	 bits. It is the best format for computation, being relatively 
 *	 compact, quick, and accurate. Colour values expressed in this way 
 *	 are hard to visualise.
 *     </para>
 *   </listitem>
 *   <listitem>
 *     <para>
 *       <emphasis><code>Lab</code></emphasis>
 *
 * 	 Lab colourspace represents CIELAB colour values with a three-band
 *	 #IM_BANDFMT_FLOAT image. This is the simplest format for general 
 *	 work: adding the constant 50 to the L channel, for example, has the 
 *	 expected result.
 *
 *	 VIPS uses D65 LAB, but you can use other colour temperatures with a
 *	 little effort, see im_XYZ2Lab_temp().
 *     </para>
 *   </listitem>
 *   <listitem>
 *     <para>
 *       <emphasis><code>XYZ</code></emphasis>
 *
 * 	 CIE XYZ colour space represented as a three-band %IM_BANDFMT_FLOAT
 *	 image.
 *     </para>
 *   </listitem>
 *   <listitem>
 *     <para>
 *       <emphasis><code>Yxy</code></emphasis>
 *
 * 	 CIE Yxy colour space represented as a three-band %IM_BANDFMT_FLOAT
 *	 image.
 *     </para>
 *   </listitem>
 *   <listitem>
 *     <para>
 *       <emphasis><code>RGB</code></emphasis>
 *
 *	(also refered to as <code>disp</code>+) This is a generic 8-bit RGB
 *	image. VIPS has a system for going to and from RGB with a simple
 *	display structure, but it's mostly deprecated. See 
 *	<link linkend="libvips-disp">disp</link>.
 *
 *	Use im_icc_export() and friends as a modern replacement.
 *     </para>
 *   </listitem>
 *   <listitem>
 *     <para>
 *       <emphasis><code>LCh</code></emphasis>
 *
 * 	 Like <code>Lab</code>, but rectangular <code>ab</code> coordinates 
 * 	 are replaced with 
 * 	 polar <code>Ch</code> (Chroma and hue) coordinates. 
 * 	 Hue angles are expressed in degrees.
 *     </para>
 *   </listitem>
 *   <listitem>
 *     <para>
 *       <emphasis><code>UCS</code></emphasis>
 *
 *       A colour space based on the CMC(1:1) colour difference measurement. 
 *       This is a highly uniform colour space, much better than CIELAB for 
 *       expressing small differences. Conversions to and from 
 *       <code>UCS</code> are extremely slow.
 *     </para>
 *   </listitem>
 * </itemizedlist>
 */

/* Areas under curves for Dxx. 2 degree observer.
 */

/**
 * IM_D93_X0:
 *
 * Areas under curves for D93, 2 degree observer.
 */

/**
 * IM_D75_X0:
 *
 * Areas under curves for D75, 2 degree observer.
 */

/**
 * IM_D65_X0:
 *
 * Areas under curves for D65, 2 degree observer.
 */

/**
 * IM_D55_X0:
 *
 * Areas under curves for D55, 2 degree observer.
 */

/**
 * IM_D50_X0:
 *
 * Areas under curves for D50, 2 degree observer.
 */

/**
 * IM_A_X0:
 *
 * Areas under curves for illuminant A (2856K), 2 degree observer.
 */

/**
 * IM_B_X0:
 *
 * Areas under curves for illuminant B (4874K), 2 degree observer.
 */

/**
 * IM_C_X0:
 *
 * Areas under curves for illuminant C (6774K), 2 degree observer.
 */

/**
 * IM_E_X0:
 *
 * Areas under curves for equal energy illuminant E.
 */

/**
 * IM_D3250_X0:
 *
 * Areas under curves for black body at 3250K, 2 degree observer.
 */

/* One image in, one out.
 */
static im_arg_desc one_in_one_out[] = {
	IM_INPUT_IMAGE( "in" ),
	IM_OUTPUT_IMAGE( "out" )
};

/* Two images in, one out.
 */
static im_arg_desc two_in_one_out[] = {
	IM_INPUT_IMAGE( "in1" ),
	IM_INPUT_IMAGE( "in2" ),
	IM_OUTPUT_IMAGE( "out" )
};

/* Call im_sRGB2XYZ via arg vector.
 */
static int
sRGB2XYZ_vec( im_object *argv )
{
	return( im_sRGB2XYZ( argv[0], argv[1] ) );
}

/* Description of im_sRGB2XYZ.
 */ 
static im_function sRGB2XYZ_desc = {
	"im_sRGB2XYZ", 			/* Name */
	"convert sRGB to XYZ",		/* Description */
	IM_FN_PIO,			/* Flags */
	sRGB2XYZ_vec, 			/* Dispatch function */
	IM_NUMBER( one_in_one_out ), 	/* Size of arg list */
	one_in_one_out 			/* Arg list */
};

/* Call im_XYZ2sRGB via arg vector.
 */
static int
XYZ2sRGB_vec( im_object *argv )
{
	return( im_XYZ2sRGB( argv[0], argv[1] ) );
}

/* Description of im_XYZ2sRGB.
 */ 
static im_function XYZ2sRGB_desc = {
	"im_XYZ2sRGB", 			/* Name */
	"convert XYZ to sRGB",		/* Description */
	IM_FN_PIO,			/* Flags */
	XYZ2sRGB_vec, 			/* Dispatch function */
	IM_NUMBER( one_in_one_out ), 	/* Size of arg list */
	one_in_one_out 			/* Arg list */
};

/* Call im_LCh2Lab via arg vector.
 */
static int
LCh2Lab_vec( im_object *argv )
{
	return( im_LCh2Lab( argv[0], argv[1] ) );
}

/* Description of im_LCh2Lab.
 */ 
static im_function LCh2Lab_desc = {
	"im_LCh2Lab", 			/* Name */
	"convert LCh to Lab",		/* Description */
	IM_FN_PIO,			/* Flags */
	LCh2Lab_vec, 			/* Dispatch function */
	IM_NUMBER( one_in_one_out ), 	/* Size of arg list */
	one_in_one_out 			/* Arg list */
};

/* Call im_LabQ2XYZ via arg vector.
 */
static int
LabQ2XYZ_vec( im_object *argv )
{
	return( im_LabQ2XYZ( argv[0], argv[1] ) );
}

/* Description of im_LabQ2XYZ.
 */ 
static im_function LabQ2XYZ_desc = {
	"im_LabQ2XYZ", 			/* Name */
	"convert LabQ to XYZ",		/* Description */
	IM_FN_PIO,			/* Flags */
	LabQ2XYZ_vec, 			/* Dispatch function */
	IM_NUMBER( one_in_one_out ), 	/* Size of arg list */
	one_in_one_out 			/* Arg list */
};

/* Call im_LCh2UCS via arg vector.
 */
static int
LCh2UCS_vec( im_object *argv )
{
	return( im_LCh2UCS( argv[0], argv[1] ) );
}

/* Description of im_LCh2UCS.
 */ 
static im_function LCh2UCS_desc = {
	"im_LCh2UCS", 			/* Name */
	"convert LCh to UCS",		/* Description */
	IM_FN_PIO,			/* Flags */
	LCh2UCS_vec, 			/* Dispatch function */
	IM_NUMBER( one_in_one_out ), 	/* Size of arg list */
	one_in_one_out 			/* Arg list */
};

/* Call im_Lab2LCh via arg vector.
 */
static int
Lab2LCh_vec( im_object *argv )
{
	return( im_Lab2LCh( argv[0], argv[1] ) );
}

/* Description of im_Lab2LCh.
 */ 
static im_function Lab2LCh_desc = {
	"im_Lab2LCh", 			/* Name */
	"convert Lab to LCh",		/* Description */
	IM_FN_PIO,			/* Flags */
	Lab2LCh_vec, 			/* Dispatch function */
	IM_NUMBER( one_in_one_out ), 	/* Size of arg list */
	one_in_one_out 			/* Arg list */
};

/* Call im_Lab2LabQ() via arg vector.
 */
static int
Lab2LabQ_vec( im_object *argv )
{
	return( im_Lab2LabQ( argv[0], argv[1] ) );
}

/* Description of im_Lab2LabQ.
 */ 
static im_function Lab2LabQ_desc = {
	"im_Lab2LabQ", 			/* Name */
	"convert Lab to LabQ",		/* Description */
	IM_FN_PIO,			/* Flags */
	Lab2LabQ_vec, 			/* Dispatch function */
	IM_NUMBER( one_in_one_out ), 	/* Size of arg list */
	one_in_one_out 			/* Arg list */
};

/* Call im_Lab2XYZ() via arg vector.
 */
static int
Lab2XYZ_vec( im_object *argv )
{
	return( im_Lab2XYZ( argv[0], argv[1] ) );
}

/* Description of im_Lab2XYZ.
 */ 
static im_function Lab2XYZ_desc = {
	"im_Lab2XYZ", 			/* Name */
	"convert D65 Lab to XYZ",	/* Description */
	IM_FN_PIO,			/* Flags */
	Lab2XYZ_vec, 			/* Dispatch function */
	IM_NUMBER( one_in_one_out ), 	/* Size of arg list */
	one_in_one_out 			/* Arg list */
};

static int
icc_present_vec( im_object *argv )
{
	int *present = ((int *) argv[0]);

	*present = im_icc_present();

	return( 0 );
}

static im_arg_desc icc_present_args[] = {
        IM_OUTPUT_INT( "present" )
};

/* Description of im_icc_present.
 */ 
static im_function icc_present_desc = {
	"im_icc_present", 		/* Name */
	"test for presence of ICC library", /* Description */
	0,				/* Flags */
	icc_present_vec, 		/* Dispatch function */
	IM_NUMBER( icc_present_args ), 	/* Size of arg list */
	icc_present_args 		/* Arg list */
};

static int
icc_transform_vec( im_object *argv )
{
	int intent = *((int *) argv[4]);

	return( im_icc_transform( argv[0], argv[1], 
		argv[2], argv[3], intent ) );
}

static im_arg_desc icc_transform_args[] = {
        IM_INPUT_IMAGE( "in" ),
        IM_OUTPUT_IMAGE( "out" ),
	IM_INPUT_STRING( "input_profile" ),
	IM_INPUT_STRING( "output_profile" ),
	IM_INPUT_INT( "intent" )
};

/* Description of im_icc_transform.
 */ 
static im_function icc_transform_desc = {
	"im_icc_transform", 		/* Name */
	"convert between two device images with a pair of ICC profiles",
					/* Description */
	IM_FN_PIO,			/* Flags */
	icc_transform_vec, 		/* Dispatch function */
	IM_NUMBER( icc_transform_args ), 	/* Size of arg list */
	icc_transform_args 		/* Arg list */
};

static int
icc_import_embedded_vec( im_object *argv )
{
	int intent = *((int *) argv[2]);

	return( im_icc_import_embedded( argv[0], argv[1], intent ) );
}

static im_arg_desc icc_import_embedded_args[] = {
        IM_INPUT_IMAGE( "in" ),
        IM_OUTPUT_IMAGE( "out" ),
	IM_INPUT_INT( "intent" )
};

/* Description of im_icc_import_embedded.
 */ 
static im_function icc_import_embedded_desc = {
	"im_icc_import_embedded", 	/* Name */
	"convert a device image to float LAB using the embedded profile",	
					/* Description */
	IM_FN_PIO,			/* Flags */
	icc_import_embedded_vec, 	/* Dispatch function */
	IM_NUMBER( icc_import_embedded_args ), 	/* Size of arg list */
	icc_import_embedded_args 	/* Arg list */
};

static int
icc_import_vec( im_object *argv )
{
	int intent = *((int *) argv[3]);

	return( im_icc_import( argv[0], argv[1], 
		argv[2], intent ) );
}

static im_arg_desc icc_import_args[] = {
        IM_INPUT_IMAGE( "in" ),
        IM_OUTPUT_IMAGE( "out" ),
	IM_INPUT_STRING( "input_profile" ),
	IM_INPUT_INT( "intent" )
};

/* Description of im_icc_import.
 */ 
static im_function icc_import_desc = {
	"im_icc_import", 		/* Name */
	"convert a device image to float LAB with an ICC profile",	
					/* Description */
	IM_FN_PIO,			/* Flags */
	icc_import_vec, 		/* Dispatch function */
	IM_NUMBER( icc_import_args ), 	/* Size of arg list */
	icc_import_args 		/* Arg list */
};

static int
icc_export_depth_vec( im_object *argv )
{
	int intent = *((int *) argv[4]);
	int depth = *((int *) argv[2]);

	return( im_icc_export_depth( argv[0], argv[1], 
		depth, argv[3], intent ) );
}

static im_arg_desc icc_export_depth_args[] = {
        IM_INPUT_IMAGE( "in" ),
        IM_OUTPUT_IMAGE( "out" ),
	IM_INPUT_INT( "depth" ),
	IM_INPUT_STRING( "output_profile" ),
	IM_INPUT_INT( "intent" )
};

/* Description of im_icc_export_depth.
 */ 
static im_function icc_export_depth_desc = {
	"im_icc_export_depth", 		/* Name */
	"convert a float LAB to device space with an ICC profile",	
					/* Description */
	IM_FN_PIO,			/* Flags */
	icc_export_depth_vec, 		/* Dispatch function */
	IM_NUMBER( icc_export_depth_args ),	/* Size of arg list */
	icc_export_depth_args 		/* Arg list */
};

static int
icc_ac2rc_vec( im_object *argv )
{
	return( im_icc_ac2rc( argv[0], argv[1], argv[2] ) );
}

static im_arg_desc icc_ac2rc_args[] = {
        IM_INPUT_IMAGE( "in" ),
        IM_OUTPUT_IMAGE( "out" ),
	IM_INPUT_STRING( "profile" )
};

/* Description of im_icc_ac2rc.
 */ 
static im_function icc_ac2rc_desc = {
	"im_icc_ac2rc", 		/* Name */
	"convert LAB from AC to RC using an ICC profile",	
					/* Description */
	IM_FN_PIO,			/* Flags */
	icc_ac2rc_vec, 			/* Dispatch function */
	IM_NUMBER( icc_ac2rc_args ), 	/* Size of arg list */
	icc_ac2rc_args 			/* Arg list */
};

static int
Lab2XYZ_temp_vec( im_object *argv )
{
	double X0 = *((double *) argv[2]);
	double Y0 = *((double *) argv[3]);
	double Z0 = *((double *) argv[4]);

	return( im_Lab2XYZ_temp( argv[0], argv[1], X0, Y0, Z0 ) );
}

static im_arg_desc temp_args[] = {
        IM_INPUT_IMAGE( "in" ),
        IM_OUTPUT_IMAGE( "out" ),
	IM_INPUT_DOUBLE( "X0" ),
	IM_INPUT_DOUBLE( "Y0" ),
	IM_INPUT_DOUBLE( "Z0" )
};

/* Description of im_Lab2XYZ_temp.
 */ 
static im_function Lab2XYZ_temp_desc = {
	"im_Lab2XYZ_temp", 		/* Name */
	"convert Lab to XYZ, with a specified colour temperature",
					/* Description */
	IM_FN_PIO,			/* Flags */
	Lab2XYZ_temp_vec, 		/* Dispatch function */
	IM_NUMBER( temp_args ), 		/* Size of arg list */
	temp_args 			/* Arg list */
};

/* Call im_Lab2UCS() via arg vector.
 */
static int
Lab2UCS_vec( im_object *argv )
{
	return( im_Lab2UCS( argv[0], argv[1] ) );
}

/* Description of im_Lab2UCS.
 */ 
static im_function Lab2UCS_desc = {
	"im_Lab2UCS", 			/* Name */
	"convert Lab to UCS",		/* Description */
	IM_FN_PIO,			/* Flags */
	Lab2UCS_vec, 			/* Dispatch function */
	IM_NUMBER( one_in_one_out ), 	/* Size of arg list */
	one_in_one_out 			/* Arg list */
};

/* Call im_LabQ2Lab() via arg vector.
 */
static int
LabQ2Lab_vec( im_object *argv )
{
	return( im_LabQ2Lab( argv[0], argv[1] ) );
}

/* Description of im_LabQ2Lab.
 */ 
static im_function LabQ2Lab_desc = {
	"im_LabQ2Lab", 			/* Name */
	"convert LabQ to Lab",		/* Description */
	IM_FN_PIO,			/* Flags */
	LabQ2Lab_vec, 			/* Dispatch function */
	IM_NUMBER( one_in_one_out ), 	/* Size of arg list */
	one_in_one_out 			/* Arg list */
};

/* Call im_rad2float() via arg vector.
 */
static int
rad2float_vec( im_object *argv )
{
	return( im_rad2float( argv[0], argv[1] ) );
}

/* Description of im_rad2float.
 */ 
static im_function rad2float_desc = {
	"im_rad2float", 		/* Name */
	"convert Radiance packed to float",	/* Description */
	IM_FN_PIO,			/* Flags */
	rad2float_vec, 			/* Dispatch function */
	IM_NUMBER( one_in_one_out ), 	/* Size of arg list */
	one_in_one_out 			/* Arg list */
};

/* Call im_float2rad() via arg vector.
 */
static int
float2rad_vec( im_object *argv )
{
	return( im_float2rad( argv[0], argv[1] ) );
}

/* Description of im_float2rad
 */ 
static im_function float2rad_desc = {
	"im_float2rad", 		/* Name */
	"convert float to Radiance packed",	/* Description */
	IM_FN_PIO,			/* Flags */
	float2rad_vec, 			/* Dispatch function */
	IM_NUMBER( one_in_one_out ), 	/* Size of arg list */
	one_in_one_out 			/* Arg list */
};

/* Call im_LabQ2LabS() via arg vector.
 */
static int
LabQ2LabS_vec( im_object *argv )
{
	return( im_LabQ2LabS( argv[0], argv[1] ) );
}

/* Description of im_LabQ2LabS.
 */ 
static im_function LabQ2LabS_desc = {
	"im_LabQ2LabS", 		/* Name */
	"convert LabQ to LabS",		/* Description */
	IM_FN_PIO,			/* Flags */
	LabQ2LabS_vec, 			/* Dispatch function */
	IM_NUMBER( one_in_one_out ), 	/* Size of arg list */
	one_in_one_out 			/* Arg list */
};

/* Call im_Lab2LabS() via arg vector.
 */
static int
Lab2LabS_vec( im_object *argv )
{
	return( im_Lab2LabS( argv[0], argv[1] ) );
}

/* Description of im_Lab2LabS.
 */ 
static im_function Lab2LabS_desc = {
	"im_Lab2LabS", 			/* Name */
	"convert Lab to LabS",		/* Description */
	IM_FN_PIO,			/* Flags */
	Lab2LabS_vec, 			/* Dispatch function */
	IM_NUMBER( one_in_one_out ), 	/* Size of arg list */
	one_in_one_out 			/* Arg list */
};

/* Call im_LabS2Lab() via arg vector.
 */
static int
LabS2Lab_vec( im_object *argv )
{
	return( im_LabS2Lab( argv[0], argv[1] ) );
}

/* Description of im_LabS2Lab.
 */ 
static im_function LabS2Lab_desc = {
	"im_LabS2Lab", 			/* Name */
	"convert LabS to Lab",		/* Description */
	IM_FN_PIO,			/* Flags */
	LabS2Lab_vec, 			/* Dispatch function */
	IM_NUMBER( one_in_one_out ), 	/* Size of arg list */
	one_in_one_out 			/* Arg list */
};

/* Call im_LabS2LabQ() via arg vector.
 */
static int
LabS2LabQ_vec( im_object *argv )
{
	return( im_LabS2LabQ( argv[0], argv[1] ) );
}

/* Description of im_LabS2LabQ.
 */ 
static im_function LabS2LabQ_desc = {
	"im_LabS2LabQ", 		/* Name */
	"convert LabS to LabQ",		/* Description */
	IM_FN_PIO,			/* Flags */
	LabS2LabQ_vec, 			/* Dispatch function */
	IM_NUMBER( one_in_one_out ), 	/* Size of arg list */
	one_in_one_out 			/* Arg list */
};

/* Call im_UCS2XYZ() via arg vector.
 */
static int
UCS2XYZ_vec( im_object *argv )
{
	return( im_UCS2XYZ( argv[0], argv[1] ) );
}

/* Description of im_UCS2XYZ.
 */ 
static im_function UCS2XYZ_desc = {
	"im_UCS2XYZ", 			/* Name */
	"convert UCS to XYZ",		/* Description */
	IM_FN_PIO,			/* Flags */
	UCS2XYZ_vec, 			/* Dispatch function */
	IM_NUMBER( one_in_one_out ), 	/* Size of arg list */
	one_in_one_out 			/* Arg list */
};

/* Call im_UCS2LCh() via arg vector.
 */
static int
UCS2LCh_vec( im_object *argv )
{
	return( im_UCS2LCh( argv[0], argv[1] ) );
}

/* Description of im_UCS2LCh.
 */ 
static im_function UCS2LCh_desc = {
	"im_UCS2LCh", 			/* Name */
	"convert UCS to LCh",		/* Description */
	IM_FN_PIO,			/* Flags */
	UCS2LCh_vec, 			/* Dispatch function */
	IM_NUMBER( one_in_one_out ), 	/* Size of arg list */
	one_in_one_out 			/* Arg list */
};

/* Call im_UCS2Lab() via arg vector.
 */
static int
UCS2Lab_vec( im_object *argv )
{
	return( im_UCS2Lab( argv[0], argv[1] ) );
}

/* Description of im_UCS2Lab.
 */ 
static im_function UCS2Lab_desc = {
	"im_UCS2Lab", 			/* Name */
	"convert UCS to Lab",		/* Description */
	IM_FN_PIO,			/* Flags */
	UCS2Lab_vec, 			/* Dispatch function */
	IM_NUMBER( one_in_one_out ), 	/* Size of arg list */
	one_in_one_out 			/* Arg list */
};

/* Call im_Yxy2XYZ via arg vector.
 */
static int
Yxy2XYZ_vec( im_object *argv )
{
	return( im_Yxy2XYZ( argv[0], argv[1] ) );
}

/* Description of im_Yxy2XYZ.
 */ 
static im_function Yxy2XYZ_desc = {
	"im_Yxy2XYZ", 			/* Name */
	"convert Yxy to XYZ",		/* Description */
	IM_FN_PIO,			/* Flags */
	Yxy2XYZ_vec, 			/* Dispatch function */
	IM_NUMBER( one_in_one_out ), 	/* Size of arg list */
	one_in_one_out 			/* Arg list */
};

/* Call im_XYZ2Yxy via arg vector.
 */
static int
XYZ2Yxy_vec( im_object *argv )
{
	return( im_XYZ2Yxy( argv[0], argv[1] ) );
}

/* Description of im_XYZ2Yxy.
 */ 
static im_function XYZ2Yxy_desc = {
	"im_XYZ2Yxy", 			/* Name */
	"convert XYZ to Yxy",		/* Description */
	IM_FN_PIO,			/* Flags */
	XYZ2Yxy_vec, 			/* Dispatch function */
	IM_NUMBER( one_in_one_out ), 	/* Size of arg list */
	one_in_one_out 			/* Arg list */
};

/* Call im_XYZ2Lab via arg vector.
 */
static int
XYZ2Lab_vec( im_object *argv )
{
	return( im_XYZ2Lab( argv[0], argv[1] ) );
}

/* Description of im_XYZ2Lab.
 */ 
static im_function XYZ2Lab_desc = {
	"im_XYZ2Lab", 			/* Name */
	"convert D65 XYZ to Lab",	/* Description */
	IM_FN_PIO,			/* Flags */
	XYZ2Lab_vec, 			/* Dispatch function */
	IM_NUMBER( one_in_one_out ), 	/* Size of arg list */
	one_in_one_out 			/* Arg list */
};

static int
XYZ2Lab_temp_vec( im_object *argv )
{
	double X0 = *((double *) argv[2]);
	double Y0 = *((double *) argv[3]);
	double Z0 = *((double *) argv[4]);

	return( im_XYZ2Lab_temp( argv[0], argv[1], X0, Y0, Z0 ) );
}

/* Description of im_XYZ2Lab_temp.
 */ 
static im_function XYZ2Lab_temp_desc = {
	"im_XYZ2Lab_temp", 		/* Name */
	"convert XYZ to Lab, with a specified colour temperature",	
					/* Description */
	IM_FN_PIO,			/* Flags */
	XYZ2Lab_temp_vec, 		/* Dispatch function */
	IM_NUMBER( temp_args ), 		/* Size of arg list */
	temp_args 			/* Arg list */
};

/* Call im_XYZ2UCS() via arg vector.
 */
static int
XYZ2UCS_vec( im_object *argv )
{
	return( im_XYZ2UCS( argv[0], argv[1] ) );
}

/* Description of im_XYZ2UCS.
 */ 
static im_function XYZ2UCS_desc = {
	"im_XYZ2UCS", 			/* Name */
	"convert XYZ to UCS",		/* Description */
	IM_FN_PIO,			/* Flags */
	XYZ2UCS_vec, 			/* Dispatch function */
	IM_NUMBER( one_in_one_out ), 	/* Size of arg list */
	one_in_one_out 			/* Arg list */
};

/* Args to XYZ2disp and disp2XYZ.
 */
static im_arg_desc XYZ2disp_args[] = {
	IM_INPUT_IMAGE( "in" ),
	IM_OUTPUT_IMAGE( "out" ),
	IM_INPUT_DISPLAY( "disp" )
};

/* Call im_XYZ2disp() via arg vector.
 */
static int
XYZ2disp_vec( im_object *argv )
{
	return( im_XYZ2disp( argv[0], argv[1], argv[2] ) );
}

/* Description of im_XYZ2disp.
 */ 
static im_function XYZ2disp_desc = {
	"im_XYZ2disp", 			/* Name */
	"convert XYZ to displayble",	/* Description */
	IM_FN_PIO,			/* Flags */
	XYZ2disp_vec, 			/* Dispatch function */
	IM_NUMBER( XYZ2disp_args ), 	/* Size of arg list */
	XYZ2disp_args 			/* Arg list */
};

/* Call im_Lab2disp() via arg vector.
 */
static int
Lab2disp_vec( im_object *argv )
{
	return( im_Lab2disp( argv[0], argv[1], argv[2] ) );
}

/* Description of im_Lab2disp.
 */ 
static im_function Lab2disp_desc = {
	"im_Lab2disp", 			/* Name */
	"convert Lab to displayable",	/* Description */
	IM_FN_PIO,			/* Flags */
	Lab2disp_vec, 			/* Dispatch function */
	IM_NUMBER( XYZ2disp_args ), 	/* Size of arg list */
	XYZ2disp_args 			/* Arg list */
};

/* Call im_LabQ2disp() via arg vector.
 */
static int
LabQ2disp_vec( im_object *argv )
{
	return( im_LabQ2disp( argv[0], argv[1], argv[2] ) );
}

/* Description of im_LabQ2disp.
 */ 
static im_function LabQ2disp_desc = {
	"im_LabQ2disp", 		/* Name */
	"convert LabQ to displayable",	/* Description */
	IM_FN_PIO,			/* Flags */
	LabQ2disp_vec, 			/* Dispatch function */
	IM_NUMBER( XYZ2disp_args ), 	/* Size of arg list */
	XYZ2disp_args 			/* Arg list */
};

/* Call im_dE00_fromLab() via arg vector.
 */
static int
dE00_fromLab_vec( im_object *argv )
{
	return( im_dE00_fromLab( argv[0], argv[1], argv[2] ) );
}

/* Description of im_dE00_fromLab.
 */ 
static im_function dE00_fromLab_desc = {
	"im_dE00_fromLab", 		/* Name */
	"calculate delta-E CIE2000 for two Lab images",
	IM_FN_PIO,			/* Flags */
	dE00_fromLab_vec, 		/* Dispatch function */
	IM_NUMBER( two_in_one_out ), 	/* Size of arg list */
	two_in_one_out 			/* Arg list */
};

/* Call im_dECMC_fromLab() via arg vector.
 */
static int
dECMC_fromLab_vec( im_object *argv )
{
	return( im_dECMC_fromLab( argv[0], argv[1], argv[2] ) );
}

/* Description of im_dECMC_fromLab.
 */ 
static im_function dECMC_fromLab_desc = {
	"im_dECMC_fromLab", 		/* Name */
	"calculate delta-E CMC(1:1) for two Lab images",
	IM_FN_PIO,			/* Flags */
	dECMC_fromLab_vec, 		/* Dispatch function */
	IM_NUMBER( two_in_one_out ), 	/* Size of arg list */
	two_in_one_out 			/* Arg list */
};

/* Call im_dE_fromXYZ() via arg vector.
 */
static int
dE_fromXYZ_vec( im_object *argv )
{
	return( im_dE_fromXYZ( argv[0], argv[1], argv[2] ) );
}

/* Description of im_dE_fromXYZ.
 */ 
static im_function dE_fromXYZ_desc = {
	"im_dE_fromXYZ", 		/* Name */
	"calculate delta-E for two XYZ images",
	IM_FN_PIO,			/* Flags */
	dE_fromXYZ_vec, 		/* Dispatch function */
	IM_NUMBER( two_in_one_out ), 	/* Size of arg list */
	two_in_one_out 			/* Arg list */
};

/* Call im_dE_fromLab() via arg vector.
 */
static int
dE_fromLab_vec( im_object *argv )
{
	return( im_dE_fromLab( argv[0], argv[1], argv[2] ) );
}

/* Description of im_dE_fromLab.
 */ 
static im_function dE_fromLab_desc = {
	"im_dE_fromLab", 		/* Name */
	"calculate delta-E for two Lab images",
	IM_FN_PIO,			/* Flags */
	dE_fromLab_vec, 		/* Dispatch function */
	IM_NUMBER( two_in_one_out ), 	/* Size of arg list */
	two_in_one_out 			/* Arg list */
};

/* Two images in, one out.
 */
static im_arg_desc dE_fromdisp_args[] = {
	IM_INPUT_IMAGE( "in1" ),
	IM_INPUT_IMAGE( "in2" ),
	IM_OUTPUT_IMAGE( "out" ),
	IM_INPUT_DISPLAY( "disp" )
};

/* Call im_dE_fromdisp() via arg vector.
 */
static int
dE_fromdisp_vec( im_object *argv )
{
	return( im_dE_fromdisp( argv[0], argv[1], argv[2], argv[3] ) );
}

/* Description of im_dE_fromdisp.
 */ 
static im_function dE_fromdisp_desc = {
	"im_dE_fromdisp", 		/* Name */
	"calculate delta-E for two displayable images",
	IM_FN_PIO,			/* Flags */
	dE_fromdisp_vec, 		/* Dispatch function */
	IM_NUMBER( dE_fromdisp_args ), 	/* Size of arg list */
	dE_fromdisp_args 		/* Arg list */
};

/* Call im_dECMC_fromdisp() via arg vector.
 */
static int
dECMC_fromdisp_vec( im_object *argv )
{
	return( im_dECMC_fromdisp( argv[0], argv[1], argv[2], argv[3] ) );
}

/* Description of im_dECMC_fromdisp.
 */ 
static im_function dECMC_fromdisp_desc = {
	"im_dECMC_fromdisp", 		/* Name */
	"calculate delta-E CMC(1:1) for two displayable images",
	IM_FN_PIO,			/* Flags */
	dECMC_fromdisp_vec, 		/* Dispatch function */
	IM_NUMBER( dE_fromdisp_args ), 	/* Size of arg list */
	dE_fromdisp_args 		/* Arg list */
};

/* Call im_disp2XYZ() via arg vector.
 */
static int
disp2XYZ_vec( im_object *argv )
{
	return( im_disp2XYZ( argv[0], argv[1], argv[2] ) );
}

/* Description of im_disp2XYZ.
 */ 
static im_function disp2XYZ_desc = {
	"im_disp2XYZ", 			/* Name */
	"convert displayable to XYZ",	/* Description */
	IM_FN_PIO,			/* Flags */
	disp2XYZ_vec, 			/* Dispatch function */
	IM_NUMBER( XYZ2disp_args ), 	/* Size of arg list */
	XYZ2disp_args 			/* Arg list */
};

/* Call im_disp2Lab() via arg vector.
 */
static int
disp2Lab_vec( im_object *argv )
{
	return( im_disp2Lab( argv[0], argv[1], argv[2] ) );
}

/* Description of im_disp2Lab.
 */ 
static im_function disp2Lab_desc = {
	"im_disp2Lab", 			/* Name */
	"convert displayable to Lab",	/* Description */
	IM_FN_PIO,			/* Flags */
	disp2Lab_vec, 			/* Dispatch function */
	IM_NUMBER( XYZ2disp_args ), 	/* Size of arg list */
	XYZ2disp_args 			/* Arg list */
};

static im_arg_desc morph_args[] = {
        IM_INPUT_IMAGE( "in" ),
        IM_OUTPUT_IMAGE( "out" ),
	IM_INPUT_DMASK( "greyscale" ),
	IM_INPUT_DOUBLE( "L_offset" ),
	IM_INPUT_DOUBLE( "L_scale" ),
	IM_INPUT_DOUBLE( "a_scale" ),
	IM_INPUT_DOUBLE( "b_scale" )
};

static int
morph_vec( im_object *argv )
{
	im_mask_object *mo = argv[2];
	double L_offset = *((double *) argv[3]);
	double L_scale = *((double *) argv[4]);
	double a_scale = *((double *) argv[5]);
	double b_scale = *((double *) argv[6]);

        return( im_lab_morph( argv[0], argv[1], 
		mo->mask, L_offset, L_scale, a_scale, b_scale ) );
}

static im_function morph_desc = {
        "im_lab_morph",                	/* Name */
        "morph colourspace of a LAB image",
        IM_FN_PIO | IM_FN_PTOP,  	/* Flags */
        morph_vec,            		/* Dispatch function */
        IM_NUMBER( morph_args ),      	/* Size of arg list */
        morph_args                 	/* Arg list */
};

/* Package up all these functions.
 */
static im_function *colour_list[] = {
	&LCh2Lab_desc,
	&LCh2UCS_desc,
	&Lab2LCh_desc,
	&Lab2LabQ_desc,
	&Lab2LabS_desc,
	&Lab2UCS_desc,
	&Lab2XYZ_desc,
	&Lab2XYZ_temp_desc,
	&Lab2disp_desc,
	&LabQ2LabS_desc,
	&LabQ2Lab_desc,
	&LabQ2XYZ_desc,
	&LabQ2disp_desc,
	&LabS2LabQ_desc,
	&LabS2Lab_desc,
	&UCS2LCh_desc,
	&UCS2Lab_desc,
	&UCS2XYZ_desc,
	&XYZ2Lab_desc,
	&XYZ2Lab_temp_desc,
	&XYZ2UCS_desc,
	&XYZ2Yxy_desc,
	&XYZ2disp_desc,
	&XYZ2sRGB_desc,
	&Yxy2XYZ_desc,
	&dE00_fromLab_desc,
	&dECMC_fromLab_desc,
	&dECMC_fromdisp_desc,
	&dE_fromLab_desc,
	&dE_fromXYZ_desc,
	&dE_fromdisp_desc,
	&disp2Lab_desc,
	&disp2XYZ_desc,
	&float2rad_desc,
	&icc_ac2rc_desc,
	&icc_export_depth_desc,
	&icc_import_desc,
	&icc_import_embedded_desc,
	&icc_present_desc,
	&icc_transform_desc,
	&morph_desc,
	&rad2float_desc,
	&sRGB2XYZ_desc
};

/* Package of functions.
 */
im_package im__colour = {
	"colour",
	IM_NUMBER( colour_list ),
	colour_list
};
