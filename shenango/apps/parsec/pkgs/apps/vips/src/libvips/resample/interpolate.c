/* vipsinterpolate ... abstract base class for various interpolators
 *
 * J. Cupitt, 15/10/08
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

#include <vips/vips.h>
#include <vips/internal.h>

#ifdef WITH_DMALLOC
#include <dmalloc.h>
#endif /*WITH_DMALLOC*/

/**
 * SECTION: interpolate
 * @short_description: various interpolators: nearest, bilinear, bicubic, and
 * some non-linear
 * @stability: Stable
 * @include: vips/vips.h
 *
 * A number of image interpolators.
 */

/*
 * FAST_PSEUDO_FLOOR is a floor and floorf replacement which has been
 * found to be faster on several linux boxes than the library
 * version. It returns the floor of its argument unless the argument
 * is a negative integer, in which case it returns one less than the
 * floor. For example:
 *
 * FAST_PSEUDO_FLOOR(0.5) = 0
 *
 * FAST_PSEUDO_FLOOR(0.) = 0
 *
 * FAST_PSEUDO_FLOOR(-.5) = -1
 *
 * as expected, but
 *
 * FAST_PSEUDO_FLOOR(-1.) = -2
 *
 * The locations of the discontinuities of FAST_PSEUDO_FLOOR are the
 * same as floor and floorf; it is just that at negative integers the
 * function is discontinuous on the right instead of the left.
 */
#define FAST_PSEUDO_FLOOR(x) ( (int)(x) - ( (x) < 0. ) )

G_DEFINE_ABSTRACT_TYPE( VipsInterpolate, vips_interpolate, VIPS_TYPE_OBJECT );

#ifdef DEBUG
static void
vips_interpolate_finalize( GObject *gobject )
{
	printf( "vips_interpolate_finalize: " );
	vips_object_print( VIPS_OBJECT( gobject ) );

	G_OBJECT_CLASS( vips_interpolate_parent_class )->finalize( gobject );
}
#endif /*DEBUG*/

static int
vips_interpolate_real_get_window_size( VipsInterpolate *interpolate )
{
	VipsInterpolateClass *class = VIPS_INTERPOLATE_GET_CLASS( interpolate );

	g_assert( class->window_size != -1 );

	return( class->window_size );
}

static int
vips_interpolate_real_get_window_offset( VipsInterpolate *interpolate )
{
	VipsInterpolateClass *class = VIPS_INTERPOLATE_GET_CLASS( interpolate );

	g_assert( class->window_offset != -1 );

	return( class->window_offset );

/* 	/\* Default to half window size. */
/* 	 *\/ */
/* 	return( vips_interpolate_get_window_size( interpolate ) / 2 ); */
}

static void
vips_interpolate_class_init( VipsInterpolateClass *class )
{
#ifdef DEBUG
	GObjectClass *gobject_class = G_OBJECT_CLASS( class );
#endif /*DEBUG*/

#ifdef DEBUG
	gobject_class->finalize = vips_interpolate_finalize;
#endif /*DEBUG*/
	class->interpolate = NULL;
	class->get_window_size = vips_interpolate_real_get_window_size;
	class->get_window_offset = vips_interpolate_real_get_window_offset;
	class->window_size = -1;
	class->window_offset = -1;
}

static void
vips_interpolate_init( VipsInterpolate *interpolate )
{
#ifdef DEBUG
	printf( "vips_interpolate_init: " );
	vips_object_print( VIPS_OBJECT( interpolate ) );
#endif /*DEBUG*/
}

/* Set the point out_x, out_y in REGION out to be the point interpolated at
 * in_x, in_y in REGION in. Don't do this as a signal for speed.
 */
void
vips_interpolate( VipsInterpolate *interpolate,
	PEL *out, REGION *in, double x, double y )
{
	VipsInterpolateClass *class = VIPS_INTERPOLATE_GET_CLASS( interpolate );

	g_assert( class->interpolate );

	class->interpolate( interpolate, out, in, x, y );
}

/* As above, but return the function pointer. Use this to cache method
 * dispatch.
 */
VipsInterpolateMethod
vips_interpolate_get_method( VipsInterpolate *interpolate )
{
	VipsInterpolateClass *class = VIPS_INTERPOLATE_GET_CLASS( interpolate );

	g_assert( class->interpolate );

	return( class->interpolate );
}

/* Get this interpolator's required window size.
 */
int
vips_interpolate_get_window_size( VipsInterpolate *interpolate )
{
	VipsInterpolateClass *class = VIPS_INTERPOLATE_GET_CLASS( interpolate );

	g_assert( class->get_window_size );

	return( class->get_window_size( interpolate ) );
}

/* Get this interpolator's required window offset.
 */
int
vips_interpolate_get_window_offset( VipsInterpolate *interpolate )
{
	VipsInterpolateClass *class = VIPS_INTERPOLATE_GET_CLASS( interpolate );

	g_assert( class->get_window_offset );

	return( class->get_window_offset( interpolate ) );
}

/* VipsInterpolateNearest class
 */

#define VIPS_TYPE_INTERPOLATE_NEAREST (vips_interpolate_nearest_get_type())
#define VIPS_INTERPOLATE_NEAREST( obj ) \
	(G_TYPE_CHECK_INSTANCE_CAST( (obj), \
	VIPS_TYPE_INTERPOLATE_NEAREST, VipsInterpolateNearest ))
#define VIPS_INTERPOLATE_NEAREST_CLASS( klass ) \
	(G_TYPE_CHECK_CLASS_CAST( (klass), \
	VIPS_TYPE_INTERPOLATE_NEAREST, VipsInterpolateNearestClass))
#define VIPS_IS_INTERPOLATE_NEAREST( obj ) \
	(G_TYPE_CHECK_INSTANCE_TYPE( (obj), VIPS_TYPE_INTERPOLATE_NEAREST ))
#define VIPS_IS_INTERPOLATE_NEAREST_CLASS( klass ) \
	(G_TYPE_CHECK_CLASS_TYPE( (klass), VIPS_TYPE_INTERPOLATE_NEAREST ))
#define VIPS_INTERPOLATE_NEAREST_GET_CLASS( obj ) \
	(G_TYPE_INSTANCE_GET_CLASS( (obj), \
	VIPS_TYPE_INTERPOLATE_NEAREST, VipsInterpolateNearestClass ))

/* No new members.
 */
typedef VipsInterpolate VipsInterpolateNearest;
typedef VipsInterpolateClass VipsInterpolateNearestClass;

G_DEFINE_TYPE( VipsInterpolateNearest, vips_interpolate_nearest,
	VIPS_TYPE_INTERPOLATE );

static void
vips_interpolate_nearest_interpolate( VipsInterpolate *interpolate,
	PEL *out, REGION *in, double x, double y )
{
	/* Pel size and line size.
	 */
	const int ps = IM_IMAGE_SIZEOF_PEL( in->im );
	int z;

	/* Top left corner we interpolate from.
	 */
	const int xi = FAST_PSEUDO_FLOOR( x );
	const int yi = FAST_PSEUDO_FLOOR( y );

	const PEL *p = (PEL *) IM_REGION_ADDR( in, xi, yi );

	for( z = 0; z < ps; z++ )
		out[z] = p[z];
}

static void
vips_interpolate_nearest_class_init( VipsInterpolateNearestClass *class )
{
	VipsObjectClass *object_class = VIPS_OBJECT_CLASS( class );
	VipsInterpolateClass *interpolate_class =
		VIPS_INTERPOLATE_CLASS( class );

	object_class->nickname = "nearest";
	object_class->description = _( "Nearest-neighbour interpolation" );

	interpolate_class->interpolate   = vips_interpolate_nearest_interpolate;
	interpolate_class->window_size   = 1;
	interpolate_class->window_offset = 0;
}

static void
vips_interpolate_nearest_init( VipsInterpolateNearest *nearest )
{
#ifdef DEBUG
	printf( "vips_interpolate_nearest_init: " );
	vips_object_print( VIPS_OBJECT( nearest ) );
#endif /*DEBUG*/
}

VipsInterpolate *
vips_interpolate_nearest_new( void )
{

	return( VIPS_INTERPOLATE( vips_object_new(
		VIPS_TYPE_INTERPOLATE_NEAREST, NULL, NULL, NULL ) ) );
}

/* Convenience: return a static nearest you don't need to free.
 */
VipsInterpolate *
vips_interpolate_nearest_static( void )
{
	static VipsInterpolate *interpolate = NULL;

	if( !interpolate )
		interpolate = vips_interpolate_nearest_new();

	return( interpolate );
}

/* VipsInterpolateBilinear class
 */

#define VIPS_TYPE_INTERPOLATE_BILINEAR (vips_interpolate_bilinear_get_type())
#define VIPS_INTERPOLATE_BILINEAR( obj ) \
	(G_TYPE_CHECK_INSTANCE_CAST( (obj), \
	VIPS_TYPE_INTERPOLATE_BILINEAR, VipsInterpolateBilinear ))
#define VIPS_INTERPOLATE_BILINEAR_CLASS( klass ) \
	(G_TYPE_CHECK_CLASS_CAST( (klass), \
	VIPS_TYPE_INTERPOLATE_BILINEAR, VipsInterpolateBilinearClass))
#define VIPS_IS_INTERPOLATE_BILINEAR( obj ) \
	(G_TYPE_CHECK_INSTANCE_TYPE( (obj), VIPS_TYPE_INTERPOLATE_BILINEAR ))
#define VIPS_IS_INTERPOLATE_BILINEAR_CLASS( klass ) \
	(G_TYPE_CHECK_CLASS_TYPE( (klass), VIPS_TYPE_INTERPOLATE_BILINEAR ))
#define VIPS_INTERPOLATE_BILINEAR_GET_CLASS( obj ) \
	(G_TYPE_INSTANCE_GET_CLASS( (obj), \
	VIPS_TYPE_INTERPOLATE_BILINEAR, VipsInterpolateBilinearClass ))

typedef VipsInterpolate VipsInterpolateBilinear;
typedef VipsInterpolateClass VipsInterpolateBilinearClass;

G_DEFINE_TYPE( VipsInterpolateBilinear, vips_interpolate_bilinear,
	VIPS_TYPE_INTERPOLATE );

/* Precalculated interpolation matricies. int (used for pel sizes up
 * to short), and float (for all others). We go to scale + 1 so
 * we can round-to-nearest safely. Don't bother with double, since
 * this is an approximation anyway.
 */
static int vips_bilinear_matrixi
	[VIPS_TRANSFORM_SCALE + 1][VIPS_TRANSFORM_SCALE + 1][4];
static float vips_bilinear_matrixd
	[VIPS_TRANSFORM_SCALE + 1][VIPS_TRANSFORM_SCALE + 1][4];

/* in this class, name vars in the 2x2 grid as eg.
 * p1  p2
 * p3  p4
 */

/* Interpolate a section ... int8/16 types, lookup tables for interpolation 
 * factors, fixed-point arithmetic.
 */
#define BILINEAR_INT( TYPE ) { \
	TYPE *tq = (TYPE *) out; \
 	\
	const int c1 = vips_bilinear_matrixi[tx][ty][0]; \
	const int c2 = vips_bilinear_matrixi[tx][ty][1]; \
	const int c3 = vips_bilinear_matrixi[tx][ty][2]; \
	const int c4 = vips_bilinear_matrixi[tx][ty][3]; \
 	\
	const TYPE *tp1 = (TYPE *) p1; \
	const TYPE *tp2 = (TYPE *) p2; \
	const TYPE *tp3 = (TYPE *) p3; \
	const TYPE *tp4 = (TYPE *) p4; \
	\
	for( z = 0; z < b; z++ ) \
		tq[z] = (c1 * tp1[z] + c2 * tp2[z] + \
			 c3 * tp3[z] + c4 * tp4[z]) >> VIPS_INTERPOLATE_SHIFT; \
}

/* Interpolate a pel ... int32 and float types, lookup tables, float 
 * arithmetic.
 */
#define BILINEAR_FLOAT( TYPE ) { \
	TYPE *tq = (TYPE *) out; \
 	\
	const double c1 = vips_bilinear_matrixd[tx][ty][0]; \
	const double c2 = vips_bilinear_matrixd[tx][ty][1]; \
	const double c3 = vips_bilinear_matrixd[tx][ty][2]; \
	const double c4 = vips_bilinear_matrixd[tx][ty][3]; \
	\
	const TYPE *tp1 = (TYPE *) p1; \
	const TYPE *tp2 = (TYPE *) p2; \
	const TYPE *tp3 = (TYPE *) p3; \
	const TYPE *tp4 = (TYPE *) p4; \
	\
	for( z = 0; z < b; z++ ) \
		tq[z] = c1 * tp1[z] + c2 * tp2[z] + \
			c3 * tp3[z] + c4 * tp4[z]; \
}

/* Expand for band types. with a fixed-point interpolator and a float
 * interpolator.
 */
#define SWITCH_INTERPOLATE( FMT, INT, FLOAT ) { \
	switch( (FMT) ) { \
	case IM_BANDFMT_UCHAR:	INT( unsigned char ); break; \
	case IM_BANDFMT_CHAR: 	INT( char ); break;  \
	case IM_BANDFMT_USHORT: INT( unsigned short ); break;  \
	case IM_BANDFMT_SHORT: 	INT( short ); break;  \
	case IM_BANDFMT_UINT: 	FLOAT( unsigned int ); break;  \
	case IM_BANDFMT_INT: 	FLOAT( int );  break;  \
	case IM_BANDFMT_FLOAT: 	FLOAT( float ); break;  \
	case IM_BANDFMT_DOUBLE:	FLOAT( double ); break;  \
	default: \
		g_assert( FALSE ); \
	} \
}

static void
vips_interpolate_bilinear_interpolate( VipsInterpolate *interpolate,
	PEL *out, REGION *in, double x, double y )
{
	/* Pel size and line size.
	 */
	const int ps = IM_IMAGE_SIZEOF_PEL( in->im );
	const int ls = IM_REGION_LSKIP( in );
	const int b = in->im->Bands;

	/* Find the mask index. We round-to-nearest, so we need to generate 
	 * indexes in 0 to VIPS_TRANSFORM_SCALE, 2^n + 1 values. We multiply 
	 * by 2 more than we need to, add one, mask, then shift down again to 
	 * get the extra range.
	 */
	const int sx = x * VIPS_TRANSFORM_SCALE * 2;
	const int sy = y * VIPS_TRANSFORM_SCALE * 2;

	const int six = sx & (VIPS_TRANSFORM_SCALE * 2 - 1);
	const int siy = sy & (VIPS_TRANSFORM_SCALE * 2 - 1);

	const int tx = (six + 1) >> 1;
	const int ty = (siy + 1) >> 1;

	/* We know x/y are always positive, so we can just (int) them. 
	 */
	const int ix = (int) x;
	const int iy = (int) y;

	const PEL *p1 = (PEL *) IM_REGION_ADDR( in, ix, iy );
	const PEL *p2 = p1 + ps;
	const PEL *p3 = p1 + ls;
	const PEL *p4 = p3 + ps;

	int z;

	SWITCH_INTERPOLATE( in->im->BandFmt,
		BILINEAR_INT, BILINEAR_FLOAT );
}

static void
vips_interpolate_bilinear_class_init( VipsInterpolateBilinearClass *class )
{
	VipsObjectClass *object_class = VIPS_OBJECT_CLASS( class );
	VipsInterpolateClass *interpolate_class =
		(VipsInterpolateClass *) class;
	int x, y;

	object_class->nickname = "bilinear";
	object_class->description = _( "Bilinear interpolation" );

	interpolate_class->interpolate   = vips_interpolate_bilinear_interpolate;
	interpolate_class->window_size   = 2;
	interpolate_class->window_offset = 1;

	/* Calculate the interpolation matricies.
	 */
	for( x = 0; x < VIPS_TRANSFORM_SCALE + 1; x++ )
		for( y = 0; y < VIPS_TRANSFORM_SCALE + 1; y++ ) {
			double X, Y, Xd, Yd;
			double c1, c2, c3, c4;

			/* Interpolation errors.
			 */
			X = (double) x / VIPS_TRANSFORM_SCALE;
			Y = (double) y / VIPS_TRANSFORM_SCALE;
			Xd = 1.0 - X;
			Yd = 1.0 - Y;

			/* Weights.
			 */
			c1 = Xd * Yd;
			c2 = X * Yd;
			c3 = Xd * Y;
			c4 = X * Y;

			vips_bilinear_matrixd[x][y][0] = c1;
			vips_bilinear_matrixd[x][y][1] = c2;
			vips_bilinear_matrixd[x][y][2] = c3;
			vips_bilinear_matrixd[x][y][3] = c4;

			vips_bilinear_matrixi[x][y][0] = 
				c1 * VIPS_INTERPOLATE_SCALE;
			vips_bilinear_matrixi[x][y][1] = 
				c2 * VIPS_INTERPOLATE_SCALE;
			vips_bilinear_matrixi[x][y][2] = 
				c3 * VIPS_INTERPOLATE_SCALE;
			vips_bilinear_matrixi[x][y][3] = 
				c4 * VIPS_INTERPOLATE_SCALE;
		}
}

static void
vips_interpolate_bilinear_init( VipsInterpolateBilinear *bilinear )
{
#ifdef DEBUG
	printf( "vips_interpolate_bilinear_init: " );
	vips_object_print( VIPS_OBJECT( bilinear ) );
#endif /*DEBUG*/

}

VipsInterpolate *
vips_interpolate_bilinear_new( void )
{
	return( VIPS_INTERPOLATE( vips_object_new(
		VIPS_TYPE_INTERPOLATE_BILINEAR, NULL, NULL, NULL ) ) );
}

/* Convenience: return a static bilinear you don't need to free.
 */
VipsInterpolate *
vips_interpolate_bilinear_static( void )
{
	static VipsInterpolate *interpolate = NULL;

	if( !interpolate )
		interpolate = vips_interpolate_bilinear_new();

	return( interpolate );
}

/* Called on startup: register the base vips interpolators.
 */
void
vips__interpolate_init( void )
{
	extern GType vips_interpolate_bicubic_get_type( void );
	extern GType vips_interpolate_lbb_get_type( void );
	extern GType vips_interpolate_nohalo_get_type( void );
	extern GType vips_interpolate_vsqbs_get_type( void );

	vips_interpolate_nearest_get_type();
	vips_interpolate_bilinear_get_type();

#ifdef ENABLE_CXX
	vips_interpolate_bicubic_get_type();
	vips_interpolate_lbb_get_type();
	vips_interpolate_nohalo_get_type();
	vips_interpolate_vsqbs_get_type();
#endif /*ENABLE_CXX*/
}

/* Make an interpolator from a nickname.
 */
VipsInterpolate *
vips_interpolate_new( const char *nickname )
{
	GType type;

	if( !(type = vips_type_find( "VipsInterpolate", nickname )) )
		return( NULL );

	return( VIPS_INTERPOLATE( vips_object_new( type, NULL, NULL, NULL ) ) );
}
