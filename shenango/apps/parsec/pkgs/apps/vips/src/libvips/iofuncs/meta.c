/* Meta information on an IMAGE. Extra fields added by the user/client/etc.
 *
 * 6/6/05 
 *	- hacked from code from Markus Wollgarten
 * 30/6/05 JC
 *	- take a copy of field names so we work for sprintf()'d fields
 *	- separate GType for refstring so we can spot it from im_header_map()
 * 13/7/05
 *	- added BLOB too (ie. can be saved to xml)
 * 26/8/05
 * 	- get_ funcs set im_error()
 * 29/8/05
 * 	- added im__meta_destroy()
 * 1/9/05
 * 	- oop, hash table insert/replace confusion fixed
 * 24/1/07
 * 	- oop, im_save_string_setf() was leaking
 * 26/8/08
 * 	- added string <-> refstring transforms
 * 29/8/09
 * 	- im_meta_get_type() renamed as im_meta_get_typeof() to prevent
 * 	  confusion with GObject-style type definers
 * 1/10/09
 * 	- gtkdoc comments
 */

/*

    Copyright (C) 1991-2005 The National Gallery

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
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
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif /*HAVE_UNISTD_H*/

#include <vips/vips.h>
#include <vips/internal.h>

#include "base64.h"

#ifdef WITH_DMALLOC
#include <dmalloc.h>
#endif /*WITH_DMALLOC*/

/**
 * SECTION: meta
 * @short_description: get and set image metadata
 * @stability: Stable
 * @see_also: <link linkend="libvips-header">header</link>
 * @include: vips/vips.h
 *
 * You can attach arbitrary metadata to images. Metadata is copied as images
 * are processed, so all images which used this image as input, directly or
 * indirectly, will have this same bit of metadata attached to them. Copying
 * is implemented with reference-counted pointers, so it is efficient, even for
 * large items of data. This does however mean that metadata items need to be
 * immutable. Metadata
 * is handy for things like ICC profiles or EXIF data.
 *
 * Various convenience functions (eg. im_meta_set_int()) let you easily attach 
 * simple types like
 * numbers, strings and memory blocks to images. Use im_header_map() to loop
 * over an image's fields, including all metadata.
 *
 * Items of metadata are identified by strings. Some strings are reserved, for
 * example the ICC pofile for an image is known by convention as
 * "icc-profile-data".
 *
 * If you save an image in VIPS format, all metadata (with a restriction, see
 * below) is automatically saved for you in a block of XML at the end of the
 * file. When you load a VIPS image, the metadata is restored. You can use the
 * 'edvips' command-line tool to extract or replace this block of XML.
 *
 * VIPS metadata is based on GValue. See the docs for that system if you want
 * to do fancy stuff such as defining a new metadata type.
 * VIPS defines a new GValue called "im_save_string", a variety of string. If 
 * your GValue can be transformed to im_save_string, it will be saved and
 * loaded to and from VIPS files for you.
 *
 * VIPS provides a couple of base classes which implement
 * reference-counted areas of memory. If you base your metadata on one of
 * these types, it can be copied between images efficiently.
 */

/* 

	The GValue we store can be a number, mutable string, ref-counted 
	immutable string (eg. large chunk of XML), etc. Convenience
	functions make it less painful. See im_meta_get_int() etc.

 */

#ifdef DEBUG
/* Check that this meta is on the hash table.
 */
static void *
meta_sanity_on_hash( Meta *meta, IMAGE *im )
{
	Meta *found;

	if( meta->im != im )
		printf( "*** field \"%s\" has incorrect im\n", 
			meta->field );

	if( !(found = g_hash_table_lookup( im->Meta, meta->field )) )
		printf( "*** field \"%s\" is on traverse but not in hash\n", 
			meta->field );

	if( found != meta )
		printf(  "*** meta \"%s\" on traverse and hash do not match\n", 
			meta->field );

	return( NULL );
}

static void
meta_sanity_on_traverse( const char *field, Meta *meta, IMAGE *im )
{
	if( meta->field != field )
		printf( "*** field \"%s\" has incorrect field\n", 
			meta->field );

	if( meta->im != im )
		printf( "*** field \"%s\" has incorrect im\n", 
			meta->field );

	if( !g_slist_find( im->Meta_traverse, meta ) )
		printf( "*** field \"%s\" is in hash but not on traverse\n", 
			meta->field );
}

static void
meta_sanity( const IMAGE *im )
{
	if( im->Meta )
		g_hash_table_foreach( im->Meta, 
			(GHFunc) meta_sanity_on_traverse, (void *) im );
	im_slist_map2( im->Meta_traverse, 
		(VSListMap2Fn) meta_sanity_on_hash, (void *) im, NULL );
}
#endif /*DEBUG*/

static void
meta_free( Meta *meta )
{
#ifdef DEBUG
{
	char *str_value;

	str_value = g_strdup_value_contents( &meta->value );
	printf( "meta_free: field %s, value = %s\n", 
		meta->field, str_value );
	g_free( str_value );
}
#endif /*DEBUG*/

	if( meta->im )
		meta->im->Meta_traverse = 
			g_slist_remove( meta->im->Meta_traverse, meta );

	g_value_unset( &meta->value );
	IM_FREE( meta->field );
	im_free( meta );
}

static Meta *
meta_new( IMAGE *im, const char *field, GValue *value )
{
	Meta *meta;

	if( !(meta = IM_NEW( NULL, Meta )) )
		return( NULL );
	meta->im = im;
	meta->field = NULL;
	memset( &meta->value, 0, sizeof( GValue ) );

	if( !(meta->field = im_strdup( NULL, field )) ) {
		meta_free( meta );
		return( NULL );
	}

	g_value_init( &meta->value, G_VALUE_TYPE( value ) );
	g_value_copy( value, &meta->value );

	im->Meta_traverse = g_slist_append( im->Meta_traverse, meta );
	g_hash_table_replace( im->Meta, meta->field, meta ); 

#ifdef DEBUG
{
	char *str_value;

	str_value = g_strdup_value_contents( value );
	printf( "meta_new: field %s, value = %s\n", 
		field, str_value );
	g_free( str_value );
}
#endif /*DEBUG*/

	return( meta );
}

/* Destroy all the meta on an image.
 */
void
im__meta_destroy( IMAGE *im )
{
	IM_FREEF( g_hash_table_destroy, im->Meta );
	g_assert( !im->Meta_traverse );
}

static void
meta_init( IMAGE *im )
{
	if( !im->Meta ) {
		g_assert( !im->Meta_traverse );
		im->Meta = g_hash_table_new_full( g_str_hash, g_str_equal,
			NULL, (GDestroyNotify) meta_free );
	}
}

static void *
meta_cp_field( Meta *meta, IMAGE *dst )
{
	Meta *meta_copy;

#ifdef DEBUG
{
	char *str_value;

	str_value = g_strdup_value_contents( &meta->value );
	printf( "im__meta_cp: copying field %s, value = %s\n", 
		meta->field, str_value );
	g_free( str_value );
}
#endif /*DEBUG*/

	/* No way to return error here, sadly.
	 */
	meta_copy = meta_new( dst, meta->field, &meta->value );

#ifdef DEBUG
	meta_sanity( dst );
#endif /*DEBUG*/

	return( NULL );
}

/* Copy meta on to dst. Called from im_cp_desc().
 */
int
im__meta_cp( IMAGE *dst, const IMAGE *src )
{
	if( src->Meta ) {
		/* Loop, copying fields.
		 */
		meta_init( dst );
		im_slist_map2( src->Meta_traverse,
			(VSListMap2Fn) meta_cp_field, dst, NULL );
	}

	return( 0 );
}

/** 
 * im_meta_set:
 * @im: image to set the metadata on
 * @field: the name to give the metadata
 * @value: the GValue to copy into the image
 *
 * Set a piece of metadata on @im. Any old metadata with that name is
 * destroyed. The GValue is copied into the image, so you need to unset the
 * value when you're done with it.
 *
 * For example, to set an integer on an image (though you would use the
 * convenience function im_meta_set_int() in practice), you would need:
 *
 * |[
 * GValue value = { 0 };
 *
 * g_value_init( &value, G_TYPE_INT );
 * g_value_set_int( &value, 42 );
 *
 * if( im_meta_set( im, field, &value ) ) {
 *   g_value_unset( &value );
 *   return( -1 );
 * }
 * g_value_unset( &value );
 *
 * return( 0 );
 * ]|
 *
 * See also: im_meta_get().
 *
 * Returns: 0 on success, -1 otherwise.
 */
int
im_meta_set( IMAGE *im, const char *field, GValue *value )
{
	Meta *meta;

	g_assert( field );
	g_assert( value );

	meta_init( im );
	if( !(meta = meta_new( im, field, value )) )
		return( -1 );

#ifdef DEBUG
	meta_sanity( im );
#endif /*DEBUG*/

	return( 0 );
}

/**
 * im_meta_remove:
 * @im: image to test
 * @field: the name to search for
 *
 * Find and remove an item of metadata. Return %FALSE if no metadata of that
 * name was found.
 *
 * See also: im_meta_set(), im_meta_get_typeof().
 *
 * Returns: %TRUE if an item of metadata of that name was found and removed
 */
gboolean
im_meta_remove( IMAGE *im, const char *field )
{
	if( im->Meta && 
		g_hash_table_remove( im->Meta, field ) )
		return( TRUE );

	return( FALSE );
}

/**
 * im_meta_get:
 * @im: image to get the metadata from
 * @field: the name to give the metadata
 * @value_copy: the GValue is copied into this
 *
 * Fill @value_copy with a copy of the metadata. @value_copy must be zeroed 
 * but uninitialised.
 *
 * This will return -1 and add a message to the error buffer if the item
 * of metadata does not exist. Use im_meta_get_typeof() to test for the 
 * existence
 * of a piece of metadata first if you are not certain it will be there.
 *
 * For example, to read a double from an image (though of course you would use
 * im_meta_get_double() in practice):
 *
 * |[
 * GValue value = { 0 };
 * double d;
 *
 * if( im_meta_get( im, field, &value ) )
 *   return( -1 );
 *
 * if( G_VALUE_TYPE( &value ) != G_TYPE_DOUBLE ) {
 *   im_error( "mydomain", _( "field \"%s\" is of type %s, not double" ),
 *     field, g_type_name( G_VALUE_TYPE( &value ) ) );
 *   g_value_unset( &value );
 *   return( -1 );
 * }
 *
 * d = g_value_get_double( &value );
 * g_value_unset( &value );
 *
 * return( 0 );
 * ]|
 *
 * See also: im_meta_get_typeof(), im_meta_set().
 *
 * Returns: 0 on success, -1 otherwise.
 */
int
im_meta_get( IMAGE *im, const char *field, GValue *value_copy )
{
	Meta *meta;

	g_assert( field );
	g_assert( value_copy );

	/* Defined?
	 */
	if( !im->Meta || !(meta = g_hash_table_lookup( im->Meta, field )) ) {
		im_error( "im_meta_get", _( "field \"%s\" not found" ), field );
		return( -1 );
	}

	g_value_init( value_copy, G_VALUE_TYPE( &meta->value ) );
	g_value_copy( &meta->value, value_copy );

	return( 0 );
}

/**
 * im_meta_get_typeof:
 * @im: image to test
 * @field: the name to search for
 *
 * Read the GType for an item of metadata. Returns zero if there is no
 * metadata of that name.
 *
 * See also: im_meta_get().
 *
 * Returns: the GType of the metadata, or zero if there is no
 * metadata of that name.
 */
GType
im_meta_get_typeof( IMAGE *im, const char *field )
{
	Meta *meta;

	g_assert( field );

	/* Defined?
	 */
	if( !im->Meta || !(meta = g_hash_table_lookup( im->Meta, field )) ) 
		return( 0 );

	return( G_VALUE_TYPE( &meta->value ) );
}

/* Helpers for set/get. Write a value and destroy it.
 */
static int
meta_set_value( IMAGE *im, const char *field, GValue *value )
{
	if( im_meta_set( im, field, value ) ) {
		g_value_unset( value );
		return( -1 );
	}
	g_value_unset( value );

	return( 0 );
}

static int
meta_get_value( IMAGE *im, const char *field, GType type, GValue *value_copy )
{
	if( im_meta_get( im, field, value_copy ) )
		return( -1 );
	if( G_VALUE_TYPE( value_copy ) != type ) {
		im_error( "im_meta_get", _( "field \"%s\" "
			"is of type %s, not %s" ),
			field, 
			g_type_name( G_VALUE_TYPE( value_copy ) ),
			g_type_name( type ) );
		g_value_unset( value_copy );
		return( -1 );
	}

	return( 0 );
}

/* Save meta fields to the header. We have a new string type for header fields
 * to save to XML and define transform functions to go from our meta types to
 * this string type.
 */
GType
im_save_string_get_type( void )
{
	static GType type = 0;

	if( !type ) {
		type = g_boxed_type_register_static( "im_save_string",
			(GBoxedCopyFunc) g_strdup, 
			(GBoxedFreeFunc) g_free );
	}

	return( type );
}

/** 
 * im_save_string_get:
 * @value: GValue to get from
 *
 * Get the C string held internally by the GValue.
 *
 * Returns: The C string held by @value. This must not be freed.
 */
const char *
im_save_string_get( const GValue *value )
{
	return( (char *) g_value_get_boxed( value ) );
}

/** 
 * im_save_string_set:
 * @value: GValue to set
 * @str: C string to copy into the GValue
 *
 * Copies the C string into @value.
 */
void
im_save_string_set( GValue *value, const char *str )
{
	g_assert( G_VALUE_TYPE( value ) == IM_TYPE_SAVE_STRING );

	g_value_set_boxed( value, str );
}

/** 
 * im_save_string_setf:
 * @value: GValue to set
 * @fmt: printf()-style format string
 * @Varargs: arguments to printf()-formatted @fmt
 *
 * Generates a string and copies it into @value.
 */
void
im_save_string_setf( GValue *value, const char *fmt, ... )
{
	va_list ap;
	char *str;

	g_assert( G_VALUE_TYPE( value ) == IM_TYPE_SAVE_STRING );

	va_start( ap, fmt );
	str = g_strdup_vprintf( fmt, ap );
	va_end( ap );
	im_save_string_set( value, str );
	g_free( str );
}

/** 
 * im_meta_set_int:
 * @im: image to attach the metadata to
 * @field: metadata name
 * @i: metadata value
 *
 * Attaches @i as a metadata item on @im under the name @field. A convenience
 * function over im_meta_set().
 *
 * See also: im_meta_get_int(), im_meta_set()
 *
 * Returns: 0 on success, -1 otherwise.
 */
int
im_meta_set_int( IMAGE *im, const char *field, int i )
{
	GValue value = { 0 };

	g_value_init( &value, G_TYPE_INT );
	g_value_set_int( &value, i );

	return( meta_set_value( im, field, &value ) );
}

/** 
 * im_meta_get_int:
 * @im: image to get the metadata from
 * @field: metadata name
 * @i: return metadata value
 *
 * Gets @i from @im under the name @field. A convenience
 * function over im_meta_get(). Use im_meta_get_typeof() to test for the 
 * existance
 * of a piece of metadata.
 *
 * See also: im_meta_set_int(), im_meta_get(), im_meta_get_typeof()
 *
 * Returns: 0 on success, -1 otherwise.
 */
int
im_meta_get_int( IMAGE *im, const char *field, int *i )
{
	GValue value_copy = { 0 };

	if( meta_get_value( im, field, G_TYPE_INT, &value_copy ) )
		return( -1 );
	*i = g_value_get_int( &value_copy );
	g_value_unset( &value_copy );

	return( 0 );
}

/** 
 * im_meta_set_double:
 * @im: image to attach the metadata to
 * @field: metadata name
 * @d: metadata value
 *
 * Attaches @d as a metadata item on @im under the name @field. A convenience
 * function over im_meta_set().
 *
 * See also: im_meta_get_double(), im_meta_set()
 *
 * Returns: 0 on success, -1 otherwise.
 */
int
im_meta_set_double( IMAGE *im, const char *field, double d )
{
	GValue value = { 0 };

	g_value_init( &value, G_TYPE_DOUBLE );
	g_value_set_double( &value, d );

	return( meta_set_value( im, field, &value ) );
}

/** 
 * im_meta_get_double:
 * @im: image to get the metadata from
 * @field: metadata name
 * @d: return metadata value
 *
 * Gets @d from @im under the name @field. A convenience
 * function over im_meta_get(). Use im_meta_get_typeof() to test for the 
 * existance
 * of a piece of metadata.
 *
 * See also: im_meta_set_double(), im_meta_get(), im_meta_get_typeof()
 *
 * Returns: 0 on success, -1 otherwise.
 */
int
im_meta_get_double( IMAGE *im, const char *field, double *d )
{
	GValue value_copy = { 0 };

	if( meta_get_value( im, field, G_TYPE_DOUBLE, &value_copy ) )
		return( -1 );
	*d = g_value_get_double( &value_copy );
	g_value_unset( &value_copy );

	return( 0 );
}

/* Transform funcs for builtin types to SAVE_STRING.
 */
static void
transform_int_save_string( const GValue *src_value, GValue *dest_value )
{
	im_save_string_setf( dest_value, "%d", g_value_get_int( src_value ) );
}

static void
transform_save_string_int( const GValue *src_value, GValue *dest_value )
{
	g_value_set_int( dest_value, atoi( im_save_string_get( src_value ) ) );
}

static void
transform_double_save_string( const GValue *src_value, GValue *dest_value )
{
	char buf[G_ASCII_DTOSTR_BUF_SIZE];

	/* Need to be locale independent.
	 */
	g_ascii_dtostr( buf, G_ASCII_DTOSTR_BUF_SIZE, 
		g_value_get_double( src_value ) );
	im_save_string_set( dest_value, buf );
}

static void
transform_save_string_double( const GValue *src_value, GValue *dest_value )
{
	g_value_set_double( dest_value, 
		g_ascii_strtod( im_save_string_get( src_value ), NULL ) );
}

/* A GType for a ref-counted area of memory.
 */
typedef struct _Area {
	int count;
	size_t length;		/* 0 if not known */
	void *data;
	im_callback_fn free_fn;
} Area;

#ifdef DEBUG
static int area_number = 0;
#endif /*DEBUG*/

/* An area of mem with a free func. (eg. \0-terminated string, or a struct).
 * Inital count == 1, so _unref() after attaching somewhere.
 */
static Area *
area_new( im_callback_fn free_fn, void *data )
{
	Area *area;

	if( !(area = IM_NEW( NULL, Area )) )
		return( NULL );
	area->count = 1;
	area->length = 0;
	area->data = data;
	area->free_fn = free_fn;

#ifdef DEBUG
	area_number += 1;
	printf( "area_new: %p count = %d (%d in total)\n", 
		area, area->count, area_number );
#endif /*DEBUG*/

	return( area );
}

/* An area of mem with a free func and a length (some sort of binary object,
 * like an ICC profile).
 */
static Area *
area_new_blob( im_callback_fn free_fn, void *blob, size_t blob_length )
{
	Area *area;

	if( !(area = area_new( free_fn, blob )) )
		return( NULL );
	area->length = blob_length;

	return( area );
}

static Area *
area_copy( Area *area )
{
	g_assert( area->count >= 0 );

	area->count += 1;

#ifdef DEBUG
	printf( "area_copy: %p count = %d\n", area, area->count );
#endif /*DEBUG*/

	return( area );
}

static void
area_unref( Area *area )
{
	g_assert( area->count > 0 );

	area->count -= 1;

#ifdef DEBUG
	printf( "area_unref: %p count = %d\n", area, area->count );
#endif /*DEBUG*/

	if( area->count == 0 && area->free_fn ) {
		(void) area->free_fn( area->data, NULL );
		area->free_fn = NULL;
		im_free( area );

#ifdef DEBUG
		area_number -= 1;
		printf( "area_unref: free .. total = %d\n", area_number );
#endif /*DEBUG*/
	}
}

/* Transform an area to a G_TYPE_STRING.
 */
static void
transform_area_g_string( const GValue *src_value, GValue *dest_value )
{
	Area *area;
	char buf[256];

	area = g_value_get_boxed( src_value );
	im_snprintf( buf, 256, "IM_TYPE_AREA, count = %d, data = %p",
		area->count, area->data );
	g_value_set_string( dest_value, buf );
}

GType
im_area_get_type( void )
{
	static GType type = 0;

	if( !type ) {
		type = g_boxed_type_register_static( "im_area",
			(GBoxedCopyFunc) area_copy, 
			(GBoxedFreeFunc) area_unref );
		g_value_register_transform_func( 
			type,
			G_TYPE_STRING,
			transform_area_g_string );
	}

	return( type );
}

/* Set value to be a ref-counted area of memory with a free function.
 */
static int
value_set_area( im_callback_fn free_fn, void *data, GValue *value )
{
	Area *area;

	if( !(area = area_new( free_fn, data )) )
		return( -1 );

	g_value_init( value, IM_TYPE_AREA );
	g_value_set_boxed( value, area );
	area_unref( area );

	return( 0 );
}

/* Don't touch count (area is static).
 */
static void *
value_get_area_data( const GValue *value )
{
	Area *area;

	area = g_value_get_boxed( value );

	return( area->data );
}

static size_t
value_get_area_length( const GValue *value )
{
	Area *area;

	area = g_value_get_boxed( value );

	return( area->length );
}

/** 
 * im_meta_set_area:
 * @im: image to attach the metadata to
 * @field: metadata name
 * @free_fn: free function for @data
 * @data: pointer to area of memory
 *
 * Attaches @data as a metadata item on @im under the name @field. When VIPS
 * no longer needs the metadata, it will be freed with @free_fn.
 *
 * See also: im_meta_get_double(), im_meta_set()
 *
 * Returns: 0 on success, -1 otherwise.
 */
int
im_meta_set_area( IMAGE *im, const char *field, 
	im_callback_fn free_fn, void *data )
{
	GValue value = { 0 };

	value_set_area( free_fn, data, &value );

	return( meta_set_value( im, field, &value ) );
}

/** 
 * im_meta_get_area:
 * @im: image to get the metadata from
 * @field: metadata name
 * @data: return metadata value
 *
 * Gets @data from @im under the name @field. A convenience
 * function over im_meta_get(). Use im_meta_get_typeof() to test for the 
 * existance
 * of a piece of metadata.
 *
 * See also: im_meta_set_area(), im_meta_get(), im_meta_get_typeof()
 *
 * Returns: 0 on success, -1 otherwise.
 */
int
im_meta_get_area( IMAGE *im, const char *field, void **data )
{
	GValue value_copy = { 0 };

	if( meta_get_value( im, field, IM_TYPE_AREA, &value_copy ) )
		return( -1 );
	*data = value_get_area_data( &value_copy );
	g_value_unset( &value_copy );

	return( 0 );
}

/** 
 * im_ref_string_get:
 * @value: GValue to get from
 *
 * Get the C string held internally by the GValue.
 *
 * Returns: The C string held by @value. This must not be freed.
 */
const char *
im_ref_string_get( const GValue *value )
{
	return( value_get_area_data( value ) );
}

/** 
 * im_ref_string_get_length:
 * @value: GValue to get from
 *
 * Gets the cached string length held internally by the refstring.
 *
 * Returns: The length of the string.
 */
size_t
im_ref_string_get_length( const GValue *value )
{
	return( value_get_area_length( value ) );
}

/** 
 * im_ref_string_set:
 * @value: GValue to set
 * @str: C string to copy into the GValue
 *
 * Copies the C string @str into @value. 
 *
 * im_ref_string are immutable C strings that are copied between images by
 * copying reference-counted pointers, making the much more efficient than
 * regular GValue strings.
 *
 * Returns: 0 on success, -1 otherwise.
 */
int
im_ref_string_set( GValue *value, const char *str )
{
	Area *area;
	char *str_copy;

	g_assert( G_VALUE_TYPE( value ) == IM_TYPE_REF_STRING );

	if( !(str_copy = im_strdup( NULL, str )) )
		return( -1 );
	if( !(area = area_new( (im_callback_fn) im_free, str_copy )) ) {
		im_free( str_copy );
		return( -1 );
	}

	/* Handy place to cache this.
	 */
	area->length = strlen( str );

	g_value_set_boxed( value, area );
	area_unref( area );

	return( 0 );
}

/* Transform a refstring to a G_TYPE_STRING and back.
 */
static void
transform_ref_string_g_string( const GValue *src_value, GValue *dest_value )
{
	g_value_set_string( dest_value, im_ref_string_get( src_value ) );
}

static void
transform_g_string_ref_string( const GValue *src_value, GValue *dest_value )
{
	im_ref_string_set( dest_value, g_value_get_string( src_value ) );
}

/* To a save string.
 */
static void
transform_ref_string_save_string( const GValue *src_value, GValue *dest_value )
{
	im_save_string_setf( dest_value, "%s", im_ref_string_get( src_value ) );
}

static void
transform_save_string_ref_string( const GValue *src_value, GValue *dest_value )
{
	im_ref_string_set( dest_value, im_save_string_get( src_value ) );
}

GType
im_ref_string_get_type( void )
{
	static GType type = 0;

	if( !type ) {
		type = g_boxed_type_register_static( "im_ref_string",
			(GBoxedCopyFunc) area_copy, 
			(GBoxedFreeFunc) area_unref );
		g_value_register_transform_func( type, G_TYPE_STRING,
			transform_ref_string_g_string );
		g_value_register_transform_func( G_TYPE_STRING, type,
			transform_g_string_ref_string );
		g_value_register_transform_func( type, IM_TYPE_SAVE_STRING,
			transform_ref_string_save_string );
		g_value_register_transform_func( IM_TYPE_SAVE_STRING, type,
			transform_save_string_ref_string );
	}

	return( type );
}

/** 
 * im_meta_set_string:
 * @im: image to attach the metadata to
 * @field: metadata name
 * @str: metadata value
 *
 * Attaches @str as a metadata item on @im under the name @field. A convenience
 * function over im_meta_set() using an im_ref_string.
 *
 * See also: im_meta_get_double(), im_meta_set(), im_ref_string
 *
 * Returns: 0 on success, -1 otherwise.
 */
int
im_meta_set_string( IMAGE *im, const char *field, const char *str )
{
	GValue value = { 0 };

	g_value_init( &value, IM_TYPE_REF_STRING );
	im_ref_string_set( &value, str );

	return( meta_set_value( im, field, &value ) );
}

/** 
 * im_meta_get_string:
 * @im: image to get the metadata from
 * @field: metadata name
 * @str: return metadata value
 *
 * Gets @str from @im under the name @field. A convenience
 * function over im_meta_get(). Use im_meta_get_typeof() to test for the 
 * existance
 * of a piece of metadata.
 *
 * See also: im_meta_set_string(), im_meta_get(), im_meta_get_typeof(),
 * im_ref_string
 *
 * Returns: 0 on success, -1 otherwise.
 */
int
im_meta_get_string( IMAGE *im, const char *field, char **str )
{
	GValue value_copy = { 0 };
	Area *area;

	if( meta_get_value( im, field, IM_TYPE_REF_STRING, &value_copy ) )
		return( -1 );
	area = g_value_get_boxed( &value_copy );
	*str = area->data;
	g_value_unset( &value_copy );

	return( 0 );
}

/** 
 * im_blob_get:
 * @value: GValue to get from
 * @blob_length: return the blob length here, optionally
 *
 * Get the address of the blob (binary large object) being held in @value and
 * optionally return its length in @blob_length.
 *
 * See also: im_blob_set()
 *
 * Returns: The blob address.
 */
void *
im_blob_get( const GValue *value, size_t *blob_length )
{
	Area *area;

	/* Can't check value type, because we may get called from
	 * im_blob_get_type().
	 */

	area = g_value_get_boxed( value );
	if( blob_length )
		*blob_length = area->length;

	return( area->data );
}

/* Transform a blob to a G_TYPE_STRING.
 */
static void
transform_blob_g_string( const GValue *src_value, GValue *dest_value )
{
	void *blob;
	size_t blob_length;
	char buf[256];

	blob = im_blob_get( src_value, &blob_length );
	im_snprintf( buf, 256, "IM_TYPE_BLOB, data = %p, length = %zd",
		blob, blob_length );
	g_value_set_string( dest_value, buf );
}

/* Transform a blob to a save string and back.
 */
static void
transform_blob_save_string( const GValue *src_value, GValue *dest_value )
{
	void *blob;
	size_t blob_length;
	char *b64;

	blob = im_blob_get( src_value, &blob_length );
	if( (b64 = im__b64_encode( blob, blob_length )) ) {
		im_save_string_set( dest_value, b64 );
		im_free( b64 );
	}
}

static void
transform_save_string_blob( const GValue *src_value, GValue *dest_value )
{
	const char *b64;
	void *blob;
	size_t blob_length;

	b64 = im_save_string_get( src_value );
	if( (blob = im__b64_decode( b64, &blob_length )) )
		im_blob_set( dest_value, 
			(im_callback_fn) im_free, blob, blob_length );
}

GType
im_blob_get_type( void )
{
	static GType type = 0;

	if( !type ) {
		type = g_boxed_type_register_static( "im_blob",
			(GBoxedCopyFunc) area_copy, 
			(GBoxedFreeFunc) area_unref );
		g_value_register_transform_func( type, G_TYPE_STRING,
			transform_blob_g_string );
		g_value_register_transform_func( type, IM_TYPE_SAVE_STRING,
			transform_blob_save_string );
		g_value_register_transform_func( IM_TYPE_SAVE_STRING, type,
			transform_save_string_blob );
	}

	return( type );
}

/** 
 * im_blob_set:
 * @value: GValue to set
 * @free_fn: free function for @data
 * @blob: pointer to area of memory
 * @blob_length: length of memory area
 *
 * Sets @value to hold a pointer to @blob. When @value is freed, @blob will be
 * freed with @free_fn. @value also holds a note of the length of the memory
 * area.
 *
 * blobs are things like ICC profiles or EXIF data. They are relocatable, and
 * are saved to VIPS files for you coded as base64 inside the XML. They are
 * copied by copying reference-counted pointers.
 *
 * See also: im_blob_get()
 *
 * Returns: 0 on success, -1 otherwise.
 */
int
im_blob_set( GValue *value, 
	im_callback_fn free_fn, void *blob, size_t blob_length ) 
{
	Area *area;

	g_assert( G_VALUE_TYPE( value ) == IM_TYPE_BLOB );

	if( !(area = area_new_blob( free_fn, blob, blob_length )) )
		return( -1 );

	g_value_set_boxed( value, area );
	area_unref( area );

	return( 0 );
}

/** 
 * im_meta_set_blob:
 * @im: image to attach the metadata to
 * @field: metadata name
 * @free_fn: free function for @data
 * @blob: pointer to area of memory
 * @blob_length: length of memory area
 *
 * Attaches @blob as a metadata item on @im under the name @field. A convenience
 * function over im_meta_set() using an im_blob.
 *
 * See also: im_meta_get_blob(), im_meta_set(), im_blob
 *
 * Returns: 0 on success, -1 otherwise.
 */
int
im_meta_set_blob( IMAGE *im, const char *field, 
	im_callback_fn free_fn, void *blob, size_t blob_length )
{
	GValue value = { 0 };

	g_value_init( &value, IM_TYPE_BLOB );
	im_blob_set( &value, free_fn, blob, blob_length );

	return( meta_set_value( im, field, &value ) );
}

/** 
 * im_meta_get_blob:
 * @im: image to get the metadata from
 * @field: metadata name
 * @blob: pointer to area of memory
 * @blob_length: return the blob length here, optionally
 *
 * Gets @blob from @im under the name @field, optionally return its length in
 * @blob_length. A convenience
 * function over im_meta_get(). Use im_meta_get_typeof() to test for the 
 * existance
 * of a piece of metadata.
 *
 * See also: im_meta_get(), im_meta_get_typeof(), im_blob_get(), im_blob
 *
 * Returns: 0 on success, -1 otherwise.
 */
int
im_meta_get_blob( IMAGE *im, const char *field, 
	void **blob, size_t *blob_length )
{
	GValue value_copy = { 0 };

	if( meta_get_value( im, field, IM_TYPE_BLOB, &value_copy ) )
		return( -1 );
	*blob = im_blob_get( &value_copy, blob_length );
	g_value_unset( &value_copy );

	return( 0 );
}

/* Make the types we need for basic functioning. Called from init_world().
 */
void
im__meta_init_types( void )
{
	(void) im_save_string_get_type();
	(void) im_area_get_type();
	(void) im_ref_string_get_type();
	(void) im_blob_get_type();

	/* Register transform functions to go from built-in saveable types to 
	 * a save string. Transform functions for our own types are set 
	 * during type creation. 
	 */
	g_value_register_transform_func( G_TYPE_INT, IM_TYPE_SAVE_STRING,
		transform_int_save_string );
	g_value_register_transform_func( IM_TYPE_SAVE_STRING, G_TYPE_INT,
		transform_save_string_int );
	g_value_register_transform_func( G_TYPE_DOUBLE, IM_TYPE_SAVE_STRING,
		transform_double_save_string );
	g_value_register_transform_func( IM_TYPE_SAVE_STRING, G_TYPE_DOUBLE,
		transform_save_string_double );
}
