/*
 * Copyright © 2007, 2008 Ryan Lortie
 * Copyright © 2010 Codethink Limited
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the licence, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Ryan Lortie <desrt@desrt.ca>
 */

/* Prologue {{{1 */

#include "config.h"

#include <glib/gvariant-serialiser.h>
#include "gvariant-internal.h"
#include <glib/gvariant-core.h>
#include <glib/gtestutils.h>
#include <glib/gstrfuncs.h>
#include <glib/ghash.h>
#include <glib/gmem.h>

#include <string.h>

#include "galias.h"

/**
 * SECTION: gvariant
 * @title: GVariant
 * @short_description: strongly typed value datatype
 * @see_also: GVariantType
 *
 * #GVariant is a variant datatype; it stores a value along with
 * information about the type of that value.  The range of possible
 * values is determined by the type.  The type system used by #GVariant
 * is #GVariantType.
 *
 * #GVariant instances always have a type and a value (which are given
 * at construction time).  The type and value of a #GVariant instance
 * can never change other than by the #GVariant itself being
 * destroyed.  A #GVariant can not contain a pointer.
 *
 * #GVariant is reference counted using g_variant_ref() and
 * g_variant_unref().  #GVariant also has floating reference counts --
 * see g_variant_ref_sink().
 *
 * #GVariant is completely threadsafe.  A #GVariant instance can be
 * concurrently accessed in any way from any number of threads without
 * problems.
 *
 * #GVariant is heavily optimised for dealing with data in serialised
 * form.  It works particularly well with data located in memory-mapped
 * files.  It can perform nearly all deserialisation operations in a
 * small constant time, usually touching only a single memory page.
 * Serialised #GVariant data can also be sent over the network.
 *
 * #GVariant is largely compatible with DBus.  Almost all types of
 * #GVariant instances can be sent over DBus.  See #GVariantType for
 * exceptions.
 *
 * For convenience to C programmers, #GVariant features powerful
 * varargs-based value construction and destruction.  This feature is
 * designed to be embedded in other libraries.
 *
 * There is a Python-inspired text language for describing #GVariant
 * values.  #GVariant includes a printer for this language and a parser
 * with type inferencing.
 *
 * <refsect2>
 *  <title>Memory Use</title>
 *  <para>
 *   #GVariant tries to be quite efficient with respect to memory use.
 *   This section gives a rough idea of how much memory is used by the
 *   current implementation.  The information here is subject to change
 *   in the future.
 *  </para>
 *  <para>
 *   The memory allocated by #GVariant can be grouped into 4 broad
 *   purposes: memory for serialised data, memory for the type
 *   information cache, buffer management memory and memory for the
 *   #GVariant structure itself.
 *  </para>
 *  <refsect3>
 *   <title>Serialised Data Memory</title>
 *   <para>
 *    This is the memory that is used for storing GVariant data in
 *    serialised form.  This is what would be sent over the network or
 *    what would end up on disk.
 *   </para>
 *   <para>
 *    The amount of memory required to store a boolean is 1 byte.  16,
 *    32 and 64 bit integers and double precision floating point numbers
 *    use their "natural" size.  Strings (including object path and
 *    signature strings) are stored with a nul terminator, and as such
 *    use the length of the string plus 1 byte.
 *   </para>
 *   <para>
 *    Maybe types use no space at all to represent the null value and
 *    use the same amount of space (sometimes plus one byte) as the
 *    equivalent non-maybe-typed value to represent the non-null case.
 *   </para>
 *   <para>
 *    Arrays use the amount of space required to store each of their
 *    members, concatenated.  Additionally, if the items stored in an
 *    array are not of a fixed-size (ie: strings, other arrays, etc)
 *    then an additional framing offset is stored for each item.  The
 *    size of this offset is either 1, 2 or 4 bytes depending on the
 *    overall size of the container.  Additionally, extra padding bytes
 *    are added as required for alignment of child values.
 *   </para>
 *   <para>
 *    Tuples (including dictionary entries) use the amount of space
 *    required to store each of their members, concatenated, plus one
 *    framing offset (as per arrays) for each non-fixed-sized item in
 *    the tuple, except for the last one.  Additionally, extra padding
 *    bytes are added as required for alignment of child values.
 *   </para>
 *   <para>
 *    Variants use the same amount of space as the item inside of the
 *    variant, plus 1 byte, plus the length of the type string for the
 *    item inside the variant.
 *   </para>
 *   <para>
 *    As an example, consider a dictionary mapping strings to variants.
 *    In the case that the dictionary is empty, 0 bytes are required for
 *    the serialisation.
 *   </para>
 *   <para>
 *    If we add an item "width" that maps to the int32 value of 500 then
 *    we will use 4 byte to store the int32 (so 6 for the variant
 *    containing it) and 6 bytes for the string.  The variant must be
 *    aligned to 8 after the 6 bytes of the string, so that's 2 extra
 *    bytes.  6 (string) + 2 (padding) + 6 (variant) is 14 bytes used
 *    for the dictionary entry.  An additional 1 byte is added to the
 *    array as a framing offset making a total of 15 bytes.
 *   </para>
 *   <para>
 *    If we add another entry, "title" that maps to a nullable string
 *    that happens to have a value of null, then we use 0 bytes for the
 *    null value (and 3 bytes for the variant to contain it along with
 *    its type string) plus 6 bytes for the string.  Again, we need 2
 *    padding bytes.  That makes a total of 6 + 2 + 3 = 11 bytes.
 *   </para>
 *   <para>
 *    We now require extra padding between the two items in the array.
 *    After the 14 bytes of the first item, that's 2 bytes required.  We
 *    now require 2 framing offsets for an extra two bytes.  14 + 2 + 11
 *    + 2 = 29 bytes to encode the entire two-item dictionary.
 *   </para>
 *  </refsect3>
 *  <refsect3>
 *   <title>Type Information Cache</title>
 *   <para>
 *    For each GVariant type that currently exists in the program a type
 *    information structure is kept in the type information cache.  The
 *    type information structure is required for rapid deserialisation.
 *   </para>
 *   <para>
 *    Continuing with the above example, if a #GVariant exists with the
 *    type "a{sv}" then a type information struct will exist for
 *    "a{sv}", "{sv}", "s", and "v".  Multiple uses of the same type
 *    will share the same type information.  Additionally, all
 *    single-digit types are stored in read-only static memory and do
 *    not contribute to the writable memory footprint of a program using
 *    #GVariant.
 *   </para>
 *   <para>
 *    Aside from the type information structures stored in read-only
 *    memory, there are two forms of type information.  One is used for
 *    container types where there is a single element type: arrays and
 *    maybe types.  The other is used for container types where there
 *    are multiple element types: tuples and dictionary entries.
 *   </para>
 *   <para>
 *    Array type info structures are 6 * sizeof (void *), plus the
 *    memory required to store the type string itself.  This means that
 *    on 32bit systems, the cache entry for "a{sv}" would require 30
 *    bytes of memory (plus malloc overhead).
 *   </para>
 *   <para>
 *    Tuple type info structures are 6 * sizeof (void *), plus 4 *
 *    sizeof (void *) for each item in the tuple, plus the memory
 *    required to store the type string itself.  A 2-item tuple, for
 *    example, would have a type information structure that consumed
 *    writable memory in the size of 14 * sizeof (void *) (plus type
 *    string)  This means that on 32bit systems, the cache entry for
 *    "{sv}" would require 61 bytes of memory (plus malloc overhead).
 *   </para>
 *   <para>
 *    This means that in total, for our "a{sv}" example, 91 bytes of
 *    type information would be allocated.
 *   </para>
 *   <para>
 *    The type information cache, additionally, uses a #GHashTable to
 *    store and lookup the cached items and stores a pointer to this
 *    hash table in static storage.  The hash table is freed when there
 *    are zero items in the type cache.
 *   </para>
 *   <para>
 *    Although these sizes may seem large it is important to remember
 *    that a program will probably only have a very small number of
 *    different types of values in it and that only one type information
 *    structure is required for many different values of the same type.
 *   </para>
 *  </refsect3>
 *  <refsect3>
 *   <title>Buffer Management Memory</title>
 *   <para>
 *    #GVariant uses an internal buffer management structure to deal
 *    with the various different possible sources of serialised data
 *    that it uses.  The buffer is responsible for ensuring that the
 *    correct call is made when the data is no longer in use by
 *    #GVariant.  This may involve a g_free() or a g_slice_free() or
 *    even g_mapped_file_unref().
 *   </para>
 *   <para>
 *    One buffer management structure is used for each chunk of
 *    serialised data.  The size of the buffer management structure is 4
 *    * (void *).  On 32bit systems, that's 16 bytes.
 *   </para>
 *  </refsect3>
 *  <refsect3>
 *   <title>GVariant structure</title>
 *   <para>
 *    The size of a #GVariant structure is 6 * (void *).  On 32 bit
 *    systems, that's 24 bytes.
 *   </para>
 *   <para>
 *    #GVariant structures only exist if they are explicitly created
 *    with API calls.  For example, if a #GVariant is constructed out of
 *    serialised data for the example given above (with the dictionary)
 *    then although there are 9 individual values that comprise the
 *    entire dictionary (two keys, two values, two variants containing
 *    the values, two dictionary entries, plus the dictionary itself),
 *    only 1 #GVariant instance exists -- the one refering to the
 *    dictionary.
 *   </para>
 *   <para>
 *    If calls are made to start accessing the other values then
 *    #GVariant instances will exist for those values only for as long
 *    as they are in use (ie: until you call g_variant_unref()).  The
 *    type information is shared.  The serialised data and the buffer
 *    management structure for that serialised data is shared by the
 *    child.
 *   </para>
 *  </refsect3>
 *  <refsect3>
 *   <title>Summary</title>
 *   <para>
 *    To put the entire example together, for our dictionary mapping
 *    strings to variants (with two entries, as given above), we are
 *    using 91 bytes of memory for type information, 29 byes of memory
 *    for the serialised data, 16 bytes for buffer management and 24
 *    bytes for the #GVariant instance, or a total of 160 bytes, plus
 *    malloc overhead.  If we were to use g_variant_get_child_value() to
 *    access the two dictionary entries, we would use an additional 48
 *    bytes.  If we were to have other dictionaries of the same type, we
 *    would use more memory for the serialised data and buffer
 *    management for those dictionaries, but the type information would
 *    be shared.
 *   </para>
 *  </refsect3>
 * </refsect2>
 */

/* definition of GVariant structure is in gvariant-core.c */

/* this is a g_return_val_if_fail() for making
 * sure a (GVariant *) has the required type.
 */
#define TYPE_CHECK(value, TYPE, val) \
  if G_UNLIKELY (!g_variant_is_of_type (value, TYPE)) {           \
    g_return_if_fail_warning (G_LOG_DOMAIN, G_STRFUNC,            \
                              "g_variant_is_of_type (" #value     \
                              ", " #TYPE ")");                    \
    return val;                                                   \
  }

/* Numeric Type Constructor/Getters {{{1 */
/* < private >
 * g_variant_new_from_trusted:
 * @type: the #GVariantType
 * @data: the data to use
 * @size: the size of @data
 * @returns: a new floating #GVariant
 *
 * Constructs a new trusted #GVariant instance from the provided data.
 * This is used to implement g_variant_new_* for all the basic types.
 */
static GVariant *
g_variant_new_from_trusted (const GVariantType *type,
                            gconstpointer       data,
                            gsize               size)
{
  GVariant *value;
  GBuffer *buffer;

  buffer = g_buffer_new_from_data (data, size);
  value = g_variant_new_from_buffer (type, buffer, TRUE);
  g_buffer_unref (buffer);

  return value;
}

/**
 * g_variant_new_boolean:
 * @boolean: a #gboolean value
 * @returns: a new boolean #GVariant instance
 *
 * Creates a new boolean #GVariant instance -- either %TRUE or %FALSE.
 *
 * Since: 2.24
 **/
GVariant *
g_variant_new_boolean (gboolean value)
{
  guchar v = value;

  return g_variant_new_from_trusted (G_VARIANT_TYPE_BOOLEAN, &v, 1);
}

/**
 * g_variant_get_boolean:
 * @value: a boolean #GVariant instance
 * @returns: %TRUE or %FALSE
 *
 * Returns the boolean value of @value.
 *
 * It is an error to call this function with a @value of any type
 * other than %G_VARIANT_TYPE_BOOLEAN.
 *
 * Since: 2.24
 **/
gboolean
g_variant_get_boolean (GVariant *value)
{
  const guchar *data;

  TYPE_CHECK (value, G_VARIANT_TYPE_BOOLEAN, FALSE);

  data = g_variant_get_data (value);

  return data != NULL ? *data != 0 : FALSE;
}

/* the constructors and accessors for byte, int{16,32,64}, handles and
 * doubles all look pretty much exactly the same, so we reduce
 * copy/pasting here.
 */
#define NUMERIC_TYPE(TYPE, type, ctype) \
  GVariant *g_variant_new_##type (ctype value) {                \
    return g_variant_new_from_trusted (G_VARIANT_TYPE_##TYPE,   \
                                       &value, sizeof value);   \
  }                                                             \
  ctype g_variant_get_##type (GVariant *value) {                \
    const ctype *data;                                          \
    TYPE_CHECK (value, G_VARIANT_TYPE_ ## TYPE, 0);             \
    data = g_variant_get_data (value);                          \
    return data != NULL ? *data : 0;                            \
  }


/**
 * g_variant_new_byte:
 * @byte: a #guint8 value
 * @returns: a new byte #GVariant instance
 *
 * Creates a new byte #GVariant instance.
 *
 * Since: 2.24
 **/
/**
 * g_variant_get_byte:
 * @value: a byte #GVariant instance
 * @returns: a #guchar
 *
 * Returns the byte value of @value.
 *
 * It is an error to call this function with a @value of any type
 * other than %G_VARIANT_TYPE_BYTE.
 *
 * Since: 2.24
 **/
NUMERIC_TYPE (BYTE, byte, guchar)

/**
 * g_variant_new_int16:
 * @int16: a #gint16 value
 * @returns: a new int16 #GVariant instance
 *
 * Creates a new int16 #GVariant instance.
 *
 * Since: 2.24
 **/
/**
 * g_variant_get_int16:
 * @value: a int16 #GVariant instance
 * @returns: a #gint16
 *
 * Returns the 16-bit signed integer value of @value.
 *
 * It is an error to call this function with a @value of any type
 * other than %G_VARIANT_TYPE_INT16.
 *
 * Since: 2.24
 **/
NUMERIC_TYPE (INT16, int16, gint16)

/**
 * g_variant_new_uint16:
 * @uint16: a #guint16 value
 * @returns: a new uint16 #GVariant instance
 *
 * Creates a new uint16 #GVariant instance.
 *
 * Since: 2.24
 **/
/**
 * g_variant_get_uint16:
 * @value: a uint16 #GVariant instance
 * @returns: a #guint16
 *
 * Returns the 16-bit unsigned integer value of @value.
 *
 * It is an error to call this function with a @value of any type
 * other than %G_VARIANT_TYPE_UINT16.
 *
 * Since: 2.24
 **/
NUMERIC_TYPE (UINT16, uint16, guint16)

/**
 * g_variant_new_int32:
 * @int32: a #gint32 value
 * @returns: a new int32 #GVariant instance
 *
 * Creates a new int32 #GVariant instance.
 *
 * Since: 2.24
 **/
/**
 * g_variant_get_int32:
 * @value: a int32 #GVariant instance
 * @returns: a #gint32
 *
 * Returns the 32-bit signed integer value of @value.
 *
 * It is an error to call this function with a @value of any type
 * other than %G_VARIANT_TYPE_INT32.
 *
 * Since: 2.24
 **/
NUMERIC_TYPE (INT32, int32, gint32)

/**
 * g_variant_new_uint32:
 * @uint32: a #guint32 value
 * @returns: a new uint32 #GVariant instance
 *
 * Creates a new uint32 #GVariant instance.
 *
 * Since: 2.24
 **/
/**
 * g_variant_get_uint32:
 * @value: a uint32 #GVariant instance
 * @returns: a #guint32
 *
 * Returns the 32-bit unsigned integer value of @value.
 *
 * It is an error to call this function with a @value of any type
 * other than %G_VARIANT_TYPE_UINT32.
 *
 * Since: 2.24
 **/
NUMERIC_TYPE (UINT32, uint32, guint32)

/**
 * g_variant_new_int64:
 * @int64: a #gint64 value
 * @returns: a new int64 #GVariant instance
 *
 * Creates a new int64 #GVariant instance.
 *
 * Since: 2.24
 **/
/**
 * g_variant_get_int64:
 * @value: a int64 #GVariant instance
 * @returns: a #gint64
 *
 * Returns the 64-bit signed integer value of @value.
 *
 * It is an error to call this function with a @value of any type
 * other than %G_VARIANT_TYPE_INT64.
 *
 * Since: 2.24
 **/
NUMERIC_TYPE (INT64, int64, gint64)

/**
 * g_variant_new_uint64:
 * @uint64: a #guint64 value
 * @returns: a new uint64 #GVariant instance
 *
 * Creates a new uint64 #GVariant instance.
 *
 * Since: 2.24
 **/
/**
 * g_variant_get_uint64:
 * @value: a uint64 #GVariant instance
 * @returns: a #guint64
 *
 * Returns the 64-bit unsigned integer value of @value.
 *
 * It is an error to call this function with a @value of any type
 * other than %G_VARIANT_TYPE_UINT64.
 *
 * Since: 2.24
 **/
NUMERIC_TYPE (UINT64, uint64, guint64)

/**
 * g_variant_new_handle:
 * @handle: a #gint32 value
 * @returns: a new handle #GVariant instance
 *
 * Creates a new handle #GVariant instance.
 *
 * By convention, handles are indexes into an array of file descriptors
 * that are sent alongside a DBus message.  If you're not interacting
 * with DBus, you probably don't need them.
 *
 * Since: 2.24
 **/
/**
 * g_variant_get_handle:
 * @value: a handle #GVariant instance
 * @returns: a #gint32
 *
 * Returns the 32-bit signed integer value of @value.
 *
 * It is an error to call this function with a @value of any type other
 * than %G_VARIANT_TYPE_HANDLE.
 *
 * By convention, handles are indexes into an array of file descriptors
 * that are sent alongside a DBus message.  If you're not interacting
 * with DBus, you probably don't need them.
 *
 * Since: 2.24
 **/
NUMERIC_TYPE (HANDLE, handle, gint32)

/**
 * g_variant_new_double:
 * @floating: a #gdouble floating point value
 * @returns: a new double #GVariant instance
 *
 * Creates a new double #GVariant instance.
 *
 * Since: 2.24
 **/
/**
 * g_variant_get_double:
 * @value: a double #GVariant instance
 * @returns: a #gdouble
 *
 * Returns the double precision floating point value of @value.
 *
 * It is an error to call this function with a @value of any type
 * other than %G_VARIANT_TYPE_DOUBLE.
 *
 * Since: 2.24
 **/
NUMERIC_TYPE (DOUBLE, double, gdouble)

/* Container type Constructor / Deconstructors {{{1 */
/**
 * g_variant_new_maybe:
 * @child_type: the #GVariantType of the child
 * @child: the child value, or %NULL
 * @returns: a new #GVariant maybe instance
 *
 * Depending on if @value is %NULL, either wraps @value inside of a
 * maybe container or creates a Nothing instance for the given @type.
 *
 * At least one of @type and @value must be non-%NULL.  If @type is
 * non-%NULL then it must be a definite type.  If they are both
 * non-%NULL then @type must be the type of @value.
 *
 * Since: 2.24
 **/
GVariant *
g_variant_new_maybe (const GVariantType *child_type,
                     GVariant           *child)
{
  GVariantType *maybe_type;
  GVariant *value;

  g_return_val_if_fail (child_type == NULL || g_variant_type_is_definite
                        (child_type), 0);
  g_return_val_if_fail (child_type != NULL || child != NULL, NULL);
  g_return_val_if_fail (child_type == NULL || child == NULL ||
                        g_variant_is_of_type (child, child_type),
                        NULL);

  if (child_type == NULL)
    child_type = g_variant_get_type (child);

  maybe_type = g_variant_type_new_maybe (child_type);

  if (child != NULL)
    {
      GVariant **children;
      gboolean trusted;

      children = g_new (GVariant *, 1);
      children[0] = g_variant_ref_sink (child);
      trusted = g_variant_is_trusted (children[0]);

      value = g_variant_new_from_children (maybe_type, children, 1, trusted);
    }
  else
    value = g_variant_new_from_children (maybe_type, NULL, 0, TRUE);

  g_variant_type_free (maybe_type);

  return value;
}

/**
 * g_variant_get_maybe:
 * @value: a maybe-typed value
 * @returns: the contents of @value, or %NULL
 *
 * Given a maybe-typed #GVariant instance, extract its value.  If the
 * value is Nothing, then this function returns %NULL.
 *
 * Since: 2.24
 **/
GVariant *
g_variant_get_maybe (GVariant *value)
{
  TYPE_CHECK (value, G_VARIANT_TYPE_MAYBE, NULL);

  if (g_variant_n_children (value))
    return g_variant_get_child_value (value, 0);

  return NULL;
}

/**
 * g_variant_new_variant:
 * @value: a #GVariance instance
 * @returns: a new variant #GVariant instance
 *
 * Boxes @value.  The result is a #GVariant instance representing a
 * variant containing the original value.
 *
 * Since: 2.24
 **/
GVariant *
g_variant_new_variant (GVariant *value)
{
  g_return_val_if_fail (value != NULL, NULL);

  g_variant_ref_sink (value);

  return g_variant_new_from_children (G_VARIANT_TYPE_VARIANT,
                                      g_memdup (&value, sizeof value),
                                      1, g_variant_is_trusted (value));
}

/**
 * g_variant_get_variant:
 * @value: a variant #GVariance instance
 * @returns: the item contained in the variant
 *
 * Unboxes @value.  The result is the #GVariant instance that was
 * contained in @value.
 *
 * Since: 2.24
 **/
GVariant *
g_variant_get_variant (GVariant *value)
{
  TYPE_CHECK (value, G_VARIANT_TYPE_VARIANT, NULL);

  return g_variant_get_child_value (value, 0);
}

/**
 * g_variant_new_array:
 * @child_type: the element type of the new array
 * @children: an array of #GVariant pointers, the children
 * @n_children: the length of @children
 * @returns: a new #GVariant array
 *
 * Creates a new #GVariant array from @children.
 *
 * @child_type must be non-%NULL if @n_children is zero.  Otherwise, the
 * child type is determined by inspecting the first element of the
 * @children array.  If @child_type is non-%NULL then it must be a
 * definite type.
 *
 * The items of the array are taken from the @children array.  No entry
 * in the @children array may be %NULL.
 *
 * All items in the array must have the same type, which must be the
 * same as @child_type, if given.
 *
 * Since: 2.24
 **/
GVariant *
g_variant_new_array (const GVariantType *child_type,
                     GVariant * const   *children,
                     gsize               n_children)
{
  GVariantType *array_type;
  GVariant **my_children;
  gboolean trusted;
  GVariant *value;
  gsize i;

  g_return_val_if_fail (n_children > 0 || child_type != NULL, NULL);
  g_return_val_if_fail (n_children == 0 || children != NULL, NULL);
  g_return_val_if_fail (child_type == NULL ||
                        g_variant_type_is_definite (child_type), NULL);

  my_children = g_new (GVariant *, n_children);
  trusted = TRUE;

  if (child_type == NULL)
    child_type = g_variant_get_type (children[0]);
  array_type = g_variant_type_new_array (child_type);

  for (i = 0; i < n_children; i++)
    {
      TYPE_CHECK (children[i], child_type, NULL);
      my_children[i] = g_variant_ref_sink (children[i]);
      trusted &= g_variant_is_trusted (children[i]);
    }

  value = g_variant_new_from_children (array_type, my_children,
                                       n_children, trusted);
  g_variant_type_free (array_type);

  return value;
}

/*< private >
 * g_variant_make_tuple_type:
 * @children: an array of GVariant *
 * @n_children: the length of @children
 *
 * Return the type of a tuple containing @children as its items.
 **/
static GVariantType *
g_variant_make_tuple_type (GVariant * const *children,
                           gsize             n_children)
{
  const GVariantType **types;
  GVariantType *type;
  gsize i;

  types = g_new (const GVariantType *, n_children);

  for (i = 0; i < n_children; i++)
    types[i] = g_variant_get_type (children[i]);

  type = g_variant_type_new_tuple (types, n_children);
  g_free (types);

  return type;
}

/**
 * g_variant_new_tuple:
 * @children: the items to make the tuple out of
 * @n_children: the length of @children
 * @returns: a new #GVariant tuple
 *
 * Creates a new tuple #GVariant out of the items in @children.  The
 * type is determined from the types of @children.  No entry in the
 * @children array may be %NULL.
 *
 * If @n_children is 0 then the unit tuple is constructed.
 *
 * Since: 2.24
 **/
GVariant *
g_variant_new_tuple (GVariant * const *children,
                     gsize             n_children)
{
  GVariantType *tuple_type;
  GVariant **my_children;
  gboolean trusted;
  GVariant *value;
  gsize i;

  g_return_val_if_fail (n_children == 0 || children != NULL, NULL);

  my_children = g_new (GVariant *, n_children);
  trusted = TRUE;

  for (i = 0; i < n_children; i++)
    {
      my_children[i] = g_variant_ref_sink (children[i]);
      trusted &= g_variant_is_trusted (children[i]);
    }

  tuple_type = g_variant_make_tuple_type (children, n_children);
  value = g_variant_new_from_children (tuple_type, my_children,
                                       n_children, trusted);
  g_variant_type_free (tuple_type);

  return value;
}

/*< private >
 * g_variant_make_dict_entry_type:
 * @key: a #GVariant, the key
 * @val: a #GVariant, the value
 *
 * Return the type of a dictionary entry containing @key and @val as its
 * children.
 **/
static GVariantType *
g_variant_make_dict_entry_type (GVariant *key,
                                GVariant *val)
{
  return g_variant_type_new_dict_entry (g_variant_get_type (key),
                                        g_variant_get_type (val));
}

/**
 * g_variant_new_dict_entry:
 * @key: a basic #GVariant, the key
 * @value: a #GVariant, the value
 * @returns: a new dictionary entry #GVariant
 *
 * Creates a new dictionary entry #GVariant.  @key and @value must be
 * non-%NULL.
 *
 * @key must be a value of a basic type (ie: not a container).
 *
 * Since: 2.24
 **/
GVariant *
g_variant_new_dict_entry (GVariant *key,
                          GVariant *value)
{
  GVariantType *dict_type;
  GVariant **children;
  gboolean trusted;

  g_return_val_if_fail (key != NULL && value != NULL, NULL);
  g_return_val_if_fail (!g_variant_is_container (key), NULL);

  children = g_new (GVariant *, 2);
  children[0] = g_variant_ref_sink (key);
  children[1] = g_variant_ref_sink (value);
  trusted = g_variant_is_trusted (key) && g_variant_is_trusted (value);

  dict_type = g_variant_make_dict_entry_type (key, value);
  value = g_variant_new_from_children (dict_type, children, 2, trusted);
  g_variant_type_free (dict_type);

  return value;
}

/**
 * g_variant_get_fixed_array:
 * @value: a #GVariant array with fixed-sized elements
 * @n_elements: a pointer to the location to store the number of items
 * @element_size: the size of each element
 * @returns: a pointer to the fixed array
 *
 * Provides access to the serialised data for an array of fixed-sized
 * items.
 *
 * @value must be an array with fixed-sized elements.  Numeric types are
 * fixed-size as are tuples containing only other fixed-sized types.
 *
 * @element_size must be the size of a single element in the array.  For
 * example, if calling this function for an array of 32 bit integers,
 * you might say <code>sizeof (gint32)</code>.  This value isn't used
 * except for the purpose of a double-check that the form of the
 * seralised data matches the caller's expectation.
 *
 * @n_elements, which must be non-%NULL is set equal to the number of
 * items in the array.
 *
 * Since: 2.24
 **/
gconstpointer
g_variant_get_fixed_array (GVariant *value,
                           gsize    *n_elements,
                           gsize     element_size)
{
  GVariantTypeInfo *array_info;
  gsize array_element_size;
  gconstpointer data;
  gsize size;

  TYPE_CHECK (value, G_VARIANT_TYPE_ARRAY, NULL);

  g_return_val_if_fail (n_elements != NULL, NULL);
  g_return_val_if_fail (element_size > 0, NULL);

  array_info = g_variant_get_type_info (value);
  g_variant_type_info_query_element (array_info, NULL, &array_element_size);

  g_return_val_if_fail (array_element_size, NULL);

  if G_UNLIKELY (array_element_size != element_size)
    {
      if (array_element_size)
        g_critical ("g_variant_get_fixed_array: assertion "
                    "`g_variant_array_has_fixed_size (value, element_size)' "
                    "failed: array size %"G_GSIZE_FORMAT" does not match "
                    "given element_size %"G_GSIZE_FORMAT".",
                    array_element_size, element_size);
      else
        g_critical ("g_variant_get_fixed_array: assertion "
                    "`g_variant_array_has_fixed_size (value, element_size)' "
                    "failed: array does not have fixed size.");
    }

  data = g_variant_get_data (value);
  size = g_variant_get_size (value);

  if (size % element_size)
    *n_elements = 0;
  else
    *n_elements = size / element_size;

  if (*n_elements)
    return data;

  return NULL;
}

/* String type constructor/getters/validation {{{1 */
/**
 * g_variant_new_string:
 * @string: a normal C nul-terminated string
 * @returns: a new string #GVariant instance
 *
 * Creates a string #GVariant with the contents of @string.
 *
 * Since: 2.24
 **/
GVariant *
g_variant_new_string (const gchar *string)
{
  g_return_val_if_fail (string != NULL, NULL);

  return g_variant_new_from_trusted (G_VARIANT_TYPE_STRING,
                                     string, strlen (string) + 1);
}

/**
 * g_variant_new_object_path:
 * @object_path: a normal C nul-terminated string
 * @returns: a new object path #GVariant instance
 *
 * Creates a DBus object path #GVariant with the contents of @string.
 * @string must be a valid DBus object path.  Use
 * g_variant_is_object_path() if you're not sure.
 *
 * Since: 2.24
 **/
GVariant *
g_variant_new_object_path (const gchar *object_path)
{
  g_return_val_if_fail (g_variant_is_object_path (object_path), NULL);

  return g_variant_new_from_trusted (G_VARIANT_TYPE_OBJECT_PATH,
                                     object_path, strlen (object_path) + 1);
}

/**
 * g_variant_is_object_path:
 * @string: a normal C nul-terminated string
 * @returns: %TRUE if @string is a DBus object path
 *
 * Determines if a given string is a valid DBus object path.  You
 * should ensure that a string is a valid DBus object path before
 * passing it to g_variant_new_object_path().
 *
 * A valid object path starts with '/' followed by zero or more
 * sequences of characters separated by '/' characters.  Each sequence
 * must contain only the characters "[A-Z][a-z][0-9]_".  No sequence
 * (including the one following the final '/' character) may be empty.
 *
 * Since: 2.24
 **/
gboolean
g_variant_is_object_path (const gchar *string)
{
  g_return_val_if_fail (string != NULL, FALSE);

  return g_variant_serialiser_is_object_path (string, strlen (string) + 1);
}

/**
 * g_variant_new_signature:
 * @signature: a normal C nul-terminated string
 * @returns: a new signature #GVariant instance
 *
 * Creates a DBus type signature #GVariant with the contents of
 * @string.  @string must be a valid DBus type signature.  Use
 * g_variant_is_signature() if you're not sure.
 *
 * Since: 2.24
 **/
GVariant *
g_variant_new_signature (const gchar *signature)
{
  g_return_val_if_fail (g_variant_is_signature (signature), NULL);

  return g_variant_new_from_trusted (G_VARIANT_TYPE_SIGNATURE,
                                     signature, strlen (signature) + 1);
}

/**
 * g_variant_is_signature:
 * @string: a normal C nul-terminated string
 * @returns: %TRUE if @string is a DBus type signature
 *
 * Determines if a given string is a valid DBus type signature.  You
 * should ensure that a string is a valid DBus object path before
 * passing it to g_variant_new_signature().
 *
 * DBus type signatures consist of zero or more definite #GVariantType
 * strings in sequence.
 *
 * Since: 2.24
 **/
gboolean
g_variant_is_signature (const gchar *string)
{
  g_return_val_if_fail (string != NULL, FALSE);

  return g_variant_serialiser_is_signature (string, strlen (string) + 1);
}

/**
 * g_variant_get_string:
 * @value: a string #GVariant instance
 * @length: a pointer to a #gsize, to store the length
 * @returns: the constant string
 *
 * Returns the string value of a #GVariant instance with a string
 * type.  This includes the types %G_VARIANT_TYPE_STRING,
 * %G_VARIANT_TYPE_OBJECT_PATH and %G_VARIANT_TYPE_SIGNATURE.
 *
 * If @length is non-%NULL then the length of the string (in bytes) is
 * returned there.  For trusted values, this information is already
 * known.  For untrusted values, a strlen() will be performed.
 *
 * It is an error to call this function with a @value of any type
 * other than those three.
 *
 * The return value remains valid as long as @value exists.
 *
 * Since: 2.24
 **/
const gchar *
g_variant_get_string (GVariant *value,
                      gsize    *length)
{
  gconstpointer data;
  gsize size;

  g_return_val_if_fail (value != NULL, NULL);
  g_return_val_if_fail (
    g_variant_is_of_type (value, G_VARIANT_TYPE_STRING) ||
    g_variant_is_of_type (value, G_VARIANT_TYPE_OBJECT_PATH) ||
    g_variant_is_of_type (value, G_VARIANT_TYPE_SIGNATURE), NULL);

  data = g_variant_get_data (value);
  size = g_variant_get_size (value);

  if (!g_variant_is_trusted (value))
    {
      switch (g_variant_classify (value))
        {
        case G_VARIANT_CLASS_STRING:
          if (g_variant_serialiser_is_string (data, size))
            break;

          data = "";
          size = 1;
          break;

        case G_VARIANT_CLASS_OBJECT_PATH:
          if (g_variant_serialiser_is_object_path (data, size))
            break;

          data = "/";
          size = 2;
          break;

        case G_VARIANT_CLASS_SIGNATURE:
          if (g_variant_serialiser_is_signature (data, size))
            break;

          data = "";
          size = 1;
          break;

        default:
          g_assert_not_reached ();
        }
    }

  if (length)
    *length = size - 1;

  return data;
}

/**
 * g_variant_dup_string:
 * @value: a string #GVariant instance
 * @length: a pointer to a #gsize, to store the length
 * @returns: a newly allocated string
 *
 * Similar to g_variant_get_string() except that instead of returning
 * a constant string, the string is duplicated.
 *
 * The return value must be freed using g_free().
 *
 * Since: 2.24
 **/
gchar *
g_variant_dup_string (GVariant *value,
                      gsize    *length)
{
  return g_strdup (g_variant_get_string (value, length));
}

/**
 * g_variant_new_strv:
 * @strv: an array of strings
 * @length: the length of @strv, or -1
 * @returns: a new floating #GVariant instance
 *
 * Constructs an array of strings #GVariant from the given array of
 * strings.
 *
 * If @length is not -1 then it gives the maximum length of @strv.  In
 * any case, a %NULL pointer in @strv is taken as a terminator.
 *
 * Since: 2.24
 **/
GVariant *
g_variant_new_strv (const gchar * const *strv,
                    gssize               length)
{
  GVariant **strings;
  gsize i;

  g_return_val_if_fail (length == 0 || strv != NULL, NULL);

  if (length < 0)
    length = g_strv_length ((gchar **) strv);

  strings = g_new (GVariant *, length);
  for (i = 0; i < length; i++)
    strings[i] = g_variant_ref_sink (g_variant_new_string (strv[i]));

  return g_variant_new_from_children (G_VARIANT_TYPE ("as"),
                                      strings, length, TRUE);
}

/**
 * g_variant_get_strv:
 * @value: an array of strings #GVariant
 * @length: the length of the result, or %NULL
 * @returns: an array of constant strings
 *
 * Gets the contents of an array of strings #GVariant.  This call
 * makes a shallow copy; the return result should be released with
 * g_free(), but the individual strings must not be modified.
 *
 * If @length is non-%NULL then the number of elements in the result
 * is stored there.  In any case, the resulting array will be
 * %NULL-terminated.
 *
 * For an empty array, @length will be set to 0 and a pointer to a
 * %NULL pointer will be returned.
 *
 * Since: 2.24
 **/
const gchar **
g_variant_get_strv (GVariant *value,
                    gsize    *length)
{
  const gchar **strv;
  gsize n;
  gsize i;

  g_return_val_if_fail (g_variant_is_of_type (value, G_VARIANT_TYPE ("as")) ||
                        g_variant_is_of_type (value, G_VARIANT_TYPE ("ao")) ||
                        g_variant_is_of_type (value, G_VARIANT_TYPE ("ag")),
                        NULL);

  g_variant_get_data (value);
  n = g_variant_n_children (value);
  strv = g_new (const gchar *, n + 1);

  for (i = 0; i < n; i++)
    {
      GVariant *string;

      string = g_variant_get_child_value (value, i);
      strv[i] = g_variant_get_string (string, NULL);
      g_variant_unref (string);
    }
  strv[i] = NULL;

  if (length)
    *length = n;

  return strv;
}

/**
 * g_variant_dup_strv:
 * @value: an array of strings #GVariant
 * @length: the length of the result, or %NULL
 * @returns: an array of constant strings
 *
 * Gets the contents of an array of strings #GVariant.  This call
 * makes a deep copy; the return result should be released with
 * g_strfreev().
 *
 * If @length is non-%NULL then the number of elements in the result
 * is stored there.  In any case, the resulting array will be
 * %NULL-terminated.
 *
 * For an empty array, @length will be set to 0 and a pointer to a
 * %NULL pointer will be returned.
 *
 * Since: 2.24
 **/
gchar **
g_variant_dup_strv (GVariant *value,
                    gsize    *length)
{
  gchar **strv;
  gsize n;
  gsize i;

  g_return_val_if_fail (g_variant_is_of_type (value, G_VARIANT_TYPE ("as")) ||
                        g_variant_is_of_type (value, G_VARIANT_TYPE ("ao")) ||
                        g_variant_is_of_type (value, G_VARIANT_TYPE ("ag")),
                        NULL);

  n = g_variant_n_children (value);
  strv = g_new (gchar *, n + 1);

  for (i = 0; i < n; i++)
    {
      GVariant *string;

      string = g_variant_get_child_value (value, i);
      strv[i] = g_variant_dup_string (string, NULL);
      g_variant_unref (string);
    }
  strv[i] = NULL;

  if (length)
    *length = n;

  return strv;
}

/* Type checking and querying {{{1 */
/**
 * g_variant_get_type:
 * @value: a #GVariant
 * @returns: a #GVariantType
 *
 * Determines the type of @value.
 *
 * The return value is valid for the lifetime of @value and must not
 * be freed.
 *
 * Since: 2.24
 **/
const GVariantType *
g_variant_get_type (GVariant *value)
{
  GVariantTypeInfo *type_info;

  g_return_val_if_fail (value != NULL, NULL);

  type_info = g_variant_get_type_info (value);

  return (GVariantType *) g_variant_type_info_get_type_string (type_info);
}

/**
 * g_variant_get_type_string:
 * @value: a #GVariant
 * @returns: the type string for the type of @value
 *
 * Returns the type string of @value.  Unlike the result of calling
 * g_variant_type_peek_string(), this string is nul-terminated.  This
 * string belongs to #GVariant and must not be freed.
 *
 * Since: 2.24
 **/
const gchar *
g_variant_get_type_string (GVariant *value)
{
  GVariantTypeInfo *type_info;

  g_return_val_if_fail (value != NULL, NULL);

  type_info = g_variant_get_type_info (value);

  return g_variant_type_info_get_type_string (type_info);
}

/**
 * g_variant_is_of_type:
 * @value: a #GVariant instance
 * @type: a #GVariantType
 * @returns: %TRUE if the type of @value matches @type
 *
 * Checks if a value has a type matching the provided type.
 *
 * Since: 2.24
 **/
gboolean
g_variant_is_of_type (GVariant           *value,
                      const GVariantType *type)
{
  return g_variant_type_is_subtype_of (g_variant_get_type (value), type);
}

/**
 * g_variant_is_container:
 * @value: a #GVariant instance
 * @returns: %TRUE if @value is a container
 *
 * Checks if @value is a container.
 */
gboolean
g_variant_is_container (GVariant *value)
{
  return g_variant_type_is_container (g_variant_get_type (value));
}


/**
 * g_variant_classify:
 * @value: a #GVariant
 * @returns: the #GVariantClass of @value
 *
 * Classifies @value according to its top-level type.
 *
 * Since: 2.24
 **/
/**
 * GVariantClass:
 * @G_VARIANT_CLASS_BOOLEAN: The #GVariant is a boolean.
 * @G_VARIANT_CLASS_BYTE: The #GVariant is a byte.
 * @G_VARIANT_CLASS_INT16: The #GVariant is a signed 16 bit integer.
 * @G_VARIANT_CLASS_UINT16: The #GVariant is an unsigned 16 bit integer.
 * @G_VARIANT_CLASS_INT32: The #GVariant is a signed 32 bit integer.
 * @G_VARIANT_CLASS_UINT32: The #GVariant is an unsigned 32 bit integer.
 * @G_VARIANT_CLASS_INT64: The #GVariant is a signed 64 bit integer.
 * @G_VARIANT_CLASS_UINT64: The #GVariant is an unsigned 64 bit integer.
 * @G_VARIANT_CLASS_HANDLE: The #GVariant is a file handle index.
 * @G_VARIANT_CLASS_DOUBLE: The #GVariant is a double precision floating 
 *                          point value.
 * @G_VARIANT_CLASS_STRING: The #GVariant is a normal string.
 * @G_VARIANT_CLASS_OBJECT_PATH: The #GVariant is a DBus object path 
 *                               string.
 * @G_VARIANT_CLASS_SIGNATURE: The #GVariant is a DBus signature string.
 * @G_VARIANT_CLASS_VARIANT: The #GVariant is a variant.
 * @G_VARIANT_CLASS_MAYBE: The #GVariant is a maybe-typed value.
 * @G_VARIANT_CLASS_ARRAY: The #GVariant is an array.
 * @G_VARIANT_CLASS_TUPLE: The #GVariant is a tuple.
 * @G_VARIANT_CLASS_DICT_ENTRY: The #GVariant is a dictionary entry.
 *
 * The range of possible top-level types of #GVariant instances.
 *
 * Since: 2.24
 **/
GVariantClass
g_variant_classify (GVariant *value)
{
  g_return_val_if_fail (value != NULL, 0);

  return *g_variant_get_type_string (value);
}

/* Pretty printer {{{1 */
/**
 * g_variant_print_string:
 * @value: a #GVariant
 * @string: a #GString, or %NULL
 * @type_annotate: %TRUE if type information should be included in
 *                 the output
 * @returns: a #GString containing the string
 *
 * Behaves as g_variant_print(), but operates on a #GString.
 *
 * If @string is non-%NULL then it is appended to and returned.  Else,
 * a new empty #GString is allocated and it is returned.
 *
 * Since: 2.24
 **/
GString *
g_variant_print_string (GVariant *value,
                        GString  *string,
                        gboolean  type_annotate)
{
  if G_UNLIKELY (string == NULL)
    string = g_string_new (NULL);

  switch (g_variant_classify (value))
    {
    case G_VARIANT_CLASS_MAYBE:
      if (type_annotate)
        g_string_append_printf (string, "@%s ",
                                g_variant_get_type_string (value));

      if (g_variant_n_children (value))
        {
          gchar *printed_child;
          GVariant *element;

          /* Nested maybes:
           *
           * Consider the case of the type "mmi".  In this case we could
           * write "just just 4", but "4" alone is totally unambiguous,
           * so we try to drop "just" where possible.
           *
           * We have to be careful not to always drop "just", though,
           * since "nothing" needs to be distinguishable from "just
           * nothing".  The case where we need to ensure we keep the
           * "just" is actually exactly the case where we have a nested
           * Nothing.
           *
           * Instead of searching for that nested Nothing, we just print
           * the contained value into a separate string and see if we
           * end up with "nothing" at the end of it.  If so, we need to
           * add "just" at our level.
           */
          element = g_variant_get_child_value (value, 0);
          printed_child = g_variant_print (element, FALSE);
          g_variant_unref (element);

          if (g_str_has_suffix (printed_child, "nothing"))
            g_string_append (string, "just ");
          g_string_append (string, printed_child);
          g_free (printed_child);
        }
      else
        g_string_append (string, "nothing");

      break;

    case G_VARIANT_CLASS_ARRAY:
      /* it's an array so the first character of the type string is 'a'
       *
       * if the first two characters are 'a{' then it's an array of
       * dictionary entries (ie: a dictionary) so we print that
       * differently.
       */
      if (g_variant_get_type_string (value)[1] == '{')
        /* dictionary */
        {
          const gchar *comma = "";
          gsize n, i;

          if ((n = g_variant_n_children (value)) == 0)
            {
              if (type_annotate)
                g_string_append_printf (string, "@%s ",
                                        g_variant_get_type_string (value));
              g_string_append (string, "{}");
              break;
            }

          g_string_append_c (string, '{');
          for (i = 0; i < n; i++)
            {
              GVariant *entry, *key, *val;

              g_string_append (string, comma);
              comma = ", ";

              entry = g_variant_get_child_value (value, i);
              key = g_variant_get_child_value (entry, 0);
              val = g_variant_get_child_value (entry, 1);
              g_variant_unref (entry);

              g_variant_print_string (key, string, type_annotate);
              g_variant_unref (key);
              g_string_append (string, ": ");
              g_variant_print_string (val, string, type_annotate);
              g_variant_unref (val);
              type_annotate = FALSE;
            }
          g_string_append_c (string, '}');
        }
      else
        /* normal (non-dictionary) array */
        {
          const gchar *comma = "";
          gsize n, i;

          if ((n = g_variant_n_children (value)) == 0)
            {
              if (type_annotate)
                g_string_append_printf (string, "@%s ",
                                        g_variant_get_type_string (value));
              g_string_append (string, "[]");
              break;
            }

          g_string_append_c (string, '[');
          for (i = 0; i < n; i++)
            {
              GVariant *element;

              g_string_append (string, comma);
              comma = ", ";

              element = g_variant_get_child_value (value, i);

              g_variant_print_string (element, string, type_annotate);
              g_variant_unref (element);
              type_annotate = FALSE;
            }
          g_string_append_c (string, ']');
        }

      break;

    case G_VARIANT_CLASS_TUPLE:
      {
        gsize n, i;

        n = g_variant_n_children (value);

        g_string_append_c (string, '(');
        for (i = 0; i < n; i++)
          {
            GVariant *element;

            element = g_variant_get_child_value (value, i);
            g_variant_print_string (element, string, type_annotate);
            g_string_append (string, ", ");
            g_variant_unref (element);
          }

        /* for >1 item:  remove final ", "
         * for 1 item:   remove final " ", but leave the ","
         * for 0 items:  there is only "(", so remove nothing
         */
        g_string_truncate (string, string->len - (n > 0) - (n > 1));
        g_string_append_c (string, ')');
      }
      break;

    case G_VARIANT_CLASS_DICT_ENTRY:
      {
        GVariant *element;

        g_string_append_c (string, '{');

        element = g_variant_get_child_value (value, 0);
        g_variant_print_string (element, string, type_annotate);
        g_variant_unref (element);

        g_string_append (string, ", ");

        element = g_variant_get_child_value (value, 1);
        g_variant_print_string (element, string, type_annotate);
        g_variant_unref (element);

        g_string_append_c (string, '}');
      }
      break;

    case G_VARIANT_CLASS_VARIANT:
      {
        GVariant *child = g_variant_get_variant (value);

        /* Always annotate types in nested variants, because they are
         * (by nature) of variable type.
         */
        g_string_append_c (string, '<');
        g_variant_print_string (child, string, TRUE);
        g_string_append_c (string, '>');

        g_variant_unref (child);
      }
      break;

    case G_VARIANT_CLASS_BOOLEAN:
      if (g_variant_get_boolean (value))
        g_string_append (string, "true");
      else
        g_string_append (string, "false");
      break;

    case G_VARIANT_CLASS_STRING:
      {
        const gchar *str = g_variant_get_string (value, NULL);
        gchar *escaped = g_strescape (str, NULL);

        /* use double quotes only if a ' is in the string */
        if (strchr (str, '\''))
          g_string_append_printf (string, "\"%s\"", escaped);
        else
          g_string_append_printf (string, "'%s'", escaped);

        g_free (escaped);
      }
      break;

    case G_VARIANT_CLASS_BYTE:
      if (type_annotate)
        g_string_append (string, "byte ");
      g_string_append_printf (string, "0x%02x",
                              g_variant_get_byte (value));
      break;

    case G_VARIANT_CLASS_INT16:
      if (type_annotate)
        g_string_append (string, "int16 ");
      g_string_append_printf (string, "%"G_GINT16_FORMAT,
                              g_variant_get_int16 (value));
      break;

    case G_VARIANT_CLASS_UINT16:
      if (type_annotate)
        g_string_append (string, "uint16 ");
      g_string_append_printf (string, "%"G_GUINT16_FORMAT,
                              g_variant_get_uint16 (value));
      break;

    case G_VARIANT_CLASS_INT32:
      /* Never annotate this type because it is the default for numbers
       * (and this is a *pretty* printer)
       */
      g_string_append_printf (string, "%"G_GINT32_FORMAT,
                              g_variant_get_int32 (value));
      break;

    case G_VARIANT_CLASS_HANDLE:
      if (type_annotate)
        g_string_append (string, "handle ");
      g_string_append_printf (string, "%"G_GINT32_FORMAT,
                              g_variant_get_handle (value));
      break;

    case G_VARIANT_CLASS_UINT32:
      if (type_annotate)
        g_string_append (string, "uint32 ");
      g_string_append_printf (string, "%"G_GUINT32_FORMAT,
                              g_variant_get_uint32 (value));
      break;

    case G_VARIANT_CLASS_INT64:
      if (type_annotate)
        g_string_append (string, "int64 ");
      g_string_append_printf (string, "%"G_GINT64_FORMAT,
                              g_variant_get_int64 (value));
      break;

    case G_VARIANT_CLASS_UINT64:
      if (type_annotate)
        g_string_append (string, "uint64 ");
      g_string_append_printf (string, "%"G_GUINT64_FORMAT,
                              g_variant_get_uint64 (value));
      break;

    case G_VARIANT_CLASS_DOUBLE:
      {
        gchar buffer[100];
        gint i;

        g_ascii_dtostr (buffer, sizeof buffer, g_variant_get_double (value));

        for (i = 0; buffer[i]; i++)
          if (buffer[i] == '.' || buffer[i] == 'e' ||
              buffer[i] == 'n' || buffer[i] == 'N')
            break;

        /* if there is no '.' or 'e' in the float then add one */
        if (buffer[i] == '\0')
          {
            buffer[i++] = '.';
            buffer[i++] = '0';
            buffer[i++] = '\0';
          }

        g_string_append (string, buffer);
      }
      break;

    case G_VARIANT_CLASS_OBJECT_PATH:
      if (type_annotate)
        g_string_append (string, "objectpath ");
      g_string_append_printf (string, "\'%s\'",
                              g_variant_get_string (value, NULL));
      break;

    case G_VARIANT_CLASS_SIGNATURE:
      if (type_annotate)
        g_string_append (string, "signature ");
      g_string_append_printf (string, "\'%s\'",
                              g_variant_get_string (value, NULL));
      break;

    default:
      g_assert_not_reached ();
  }

  return string;
}

/**
 * g_variant_print:
 * @value: a #GVariant
 * @type_annotate: %TRUE if type information should be included in
 *                 the output
 * @returns: a newly-allocated string holding the result.
 *
 * Pretty-prints @value in the format understood by g_variant_parse().
 *
 * If @type_annotate is %TRUE, then type information is included in
 * the output.
 */
gchar *
g_variant_print (GVariant *value,
                 gboolean  type_annotate)
{
  return g_string_free (g_variant_print_string (value, NULL, type_annotate),
                        FALSE);
};

/* Hash, Equal {{{1 */
/**
 * g_variant_hash:
 * @value: a basic #GVariant value as a #gconstpointer
 * @returns: a hash value corresponding to @value
 *
 * Generates a hash value for a #GVariant instance.
 *
 * The output of this function is guaranteed to be the same for a given
 * value only per-process.  It may change between different processor
 * architectures or even different versions of GLib.  Do not use this
 * function as a basis for building protocols or file formats.
 *
 * The type of @value is #gconstpointer only to allow use of this
 * function with #GHashTable.  @value must be a #GVariant.
 *
 * Since: 2.24
 **/
guint
g_variant_hash (gconstpointer value_)
{
  GVariant *value = (GVariant *) value_;

  switch (g_variant_classify (value))
    {
    case G_VARIANT_CLASS_STRING:
    case G_VARIANT_CLASS_OBJECT_PATH:
    case G_VARIANT_CLASS_SIGNATURE:
      return g_str_hash (g_variant_get_string (value, NULL));

    case G_VARIANT_CLASS_BOOLEAN:
      /* this is a very odd thing to hash... */
      return g_variant_get_boolean (value);

    case G_VARIANT_CLASS_BYTE:
      return g_variant_get_byte (value);

    case G_VARIANT_CLASS_INT16:
    case G_VARIANT_CLASS_UINT16:
      {
        const guint16 *ptr;

        ptr = g_variant_get_data (value);

        if (ptr)
          return *ptr;
        else
          return 0;
      }

    case G_VARIANT_CLASS_INT32:
    case G_VARIANT_CLASS_UINT32:
    case G_VARIANT_CLASS_HANDLE:
      {
        const guint *ptr;

        ptr = g_variant_get_data (value);

        if (ptr)
          return *ptr;
        else
          return 0;
      }

    case G_VARIANT_CLASS_INT64:
    case G_VARIANT_CLASS_UINT64:
    case G_VARIANT_CLASS_DOUBLE:
      /* need a separate case for these guys because otherwise
       * performance could be quite bad on big endian systems
       */
      {
        const guint *ptr;

        ptr = g_variant_get_data (value);

        if (ptr)
          return ptr[0] + ptr[1];
        else
          return 0;
      }

    default:
      g_return_val_if_fail (!g_variant_is_container (value), 0);
      g_assert_not_reached ();
    }
}

/**
 * g_variant_equal:
 * @one: a #GVariant instance
 * @two: a #GVariant instance
 * @returns: %TRUE if @one and @two are equal
 *
 * Checks if @one and @two have the same type and value.
 *
 * The types of @one and @two are #gconstpointer only to allow use of
 * this function with #GHashTable.  They must each be a #GVariant.
 *
 * Since: 2.24
 **/
gboolean
g_variant_equal (gconstpointer one,
                 gconstpointer two)
{
  gboolean equal;

  g_return_val_if_fail (one != NULL && two != NULL, FALSE);

  if (g_variant_get_type_info ((GVariant *) one) !=
      g_variant_get_type_info ((GVariant *) two))
    return FALSE;

  /* if both values are trusted to be in their canonical serialised form
   * then a simple memcmp() of their serialised data will answer the
   * question.
   *
   * if not, then this might generate a false negative (since it is
   * possible for two different byte sequences to represent the same
   * value).  for now we solve this by pretty-printing both values and
   * comparing the result.
   */
  if (g_variant_is_trusted ((GVariant *) one) &&
      g_variant_is_trusted ((GVariant *) two))
    {
      gconstpointer data_one, data_two;
      gsize size_one, size_two;

      size_one = g_variant_get_size ((GVariant *) one);
      size_two = g_variant_get_size ((GVariant *) two);

      if (size_one != size_two)
        return FALSE;

      data_one = g_variant_get_data ((GVariant *) one);
      data_two = g_variant_get_data ((GVariant *) two);

      equal = memcmp (data_one, data_two, size_one) == 0;
    }
  else
    {
      gchar *strone, *strtwo;

      strone = g_variant_print ((GVariant *) one, FALSE);
      strtwo = g_variant_print ((GVariant *) two, FALSE);
      equal = strcmp (strone, strtwo) == 0;
      g_free (strone);
      g_free (strtwo);
    }

  return equal;
}

/* GVariantIter {{{1 */
/**
 * GVariantIter:
 *
 * #GVariantIter is an opaque data structure and can only be accessed
 * using the following functions.
 **/
struct stack_iter
{
  GVariant *value;
  gssize n, i;

  const gchar *loop_format;

  gsize padding[3];
  gsize magic;
};

G_STATIC_ASSERT (sizeof (struct stack_iter) <= sizeof (GVariantIter));

struct heap_iter
{
  struct stack_iter iter;

  GVariant *value_ref;
  gsize magic;
};

#define GVSI(i)                 ((struct stack_iter *) (i))
#define GVHI(i)                 ((struct heap_iter *) (i))
#define GVSI_MAGIC              ((gsize) 3579507750u)
#define GVHI_MAGIC              ((gsize) 1450270775u)
#define is_valid_iter(i)        (i != NULL && \
                                 GVSI(i)->magic == GVSI_MAGIC)
#define is_valid_heap_iter(i)   (GVHI(i)->magic == GVHI_MAGIC && \
                                 is_valid_iter(i))

/**
 * g_variant_iter_new:
 * @value: a container #GVariant
 * @returns: a new heap-allocated #GVariantIter
 *
 * Creates a heap-allocated #GVariantIter for iterating over the items
 * in @value.
 *
 * Use g_variant_iter_free() to free the return value when you no longer
 * need it.
 *
 * A reference is taken to @value and will be released only when
 * g_variant_iter_free() is called.
 *
 * Since: 2.24
 **/
GVariantIter *
g_variant_iter_new (GVariant *value)
{
  GVariantIter *iter;

  iter = (GVariantIter *) g_slice_new (struct heap_iter);
  GVHI(iter)->value_ref = g_variant_ref (value);
  GVHI(iter)->magic = GVHI_MAGIC;

  g_variant_iter_init (iter, value);

  return iter;
}

/**
 * g_variant_iter_init:
 * @iter: a pointer to a #GVariantIter
 * @value: a container #GVariant
 * @returns: the number of items in @value
 *
 * Initialises (without allocating) a #GVariantIter.  @iter may be
 * completely uninitialised prior to this call; its old value is
 * ignored.
 *
 * The iterator remains valid for as long as @value exists, and need not
 * be freed in any way.
 *
 * Since: 2.24
 **/
gsize
g_variant_iter_init (GVariantIter *iter,
                     GVariant     *value)
{
  GVSI(iter)->magic = GVSI_MAGIC;
  GVSI(iter)->value = value;
  GVSI(iter)->n = g_variant_n_children (value);
  GVSI(iter)->i = -1;
  GVSI(iter)->loop_format = NULL;

  return GVSI(iter)->n;
}

/**
 * g_variant_iter_copy:
 * @iter: a #GVariantIter
 * @returns: a new heap-allocated #GVariantIter
 *
 * Creates a new heap-allocated #GVariantIter to iterate over the
 * container that was being iterated over by @iter.  Iteration begins on
 * the new iterator from the current position of the old iterator but
 * the two copies are independent past that point.
 *
 * Use g_variant_iter_free() to free the return value when you no longer
 * need it.
 *
 * A reference is taken to the container that @iter is iterating over
 * and will be releated only when g_variant_iter_free() is called.
 *
 * Since: 2.24
 **/
GVariantIter *
g_variant_iter_copy (GVariantIter *iter)
{
  GVariantIter *copy;

  g_return_val_if_fail (is_valid_iter (iter), 0);

  copy = g_variant_iter_new (GVSI(iter)->value);
  GVSI(copy)->i = GVSI(iter)->i;

  return copy;
}

/**
 * g_variant_iter_n_children:
 * @iter: a #GVariantIter
 * @returns: the number of children in the container
 *
 * Queries the number of child items in the container that we are
 * iterating over.  This is the total number of items -- not the number
 * of items remaining.
 *
 * This function might be useful for preallocation of arrays.
 *
 * Since: 2.24
 **/
gsize
g_variant_iter_n_children (GVariantIter *iter)
{
  g_return_val_if_fail (is_valid_iter (iter), 0);

  return GVSI(iter)->n;
}

/**
 * g_variant_iter_free:
 * @iter: a heap-allocated #GVariantIter
 *
 * Frees a heap-allocated #GVariantIter.  Only call this function on
 * iterators that were returned by g_variant_iter_new() or
 * g_variant_iter_copy().
 *
 * Since: 2.24
 **/
void
g_variant_iter_free (GVariantIter *iter)
{
  g_return_if_fail (is_valid_heap_iter (iter));

  g_variant_unref (GVHI(iter)->value_ref);
  GVHI(iter)->magic = 0;

  g_slice_free (struct heap_iter, GVHI(iter));
}

/**
 * g_variant_iter_next_value:
 * @iter: a #GVariantIter
 * @returns: a #GVariant, or %NULL
 *
 * Gets the next item in the container.  If no more items remain then
 * %NULL is returned.
 *
 * Use g_variant_unref() to drop your reference on the return value when
 * you no longer need it.
 *
 * <example>
 *  <title>Iterating with g_variant_iter_next_value()</title>
 *  <programlisting>
 *   /<!-- -->* recursively iterate a container *<!-- -->/
 *   void
 *   iterate_container_recursive (GVariant *container)
 *   {
 *     GVariantIter iter;
 *     GVariant *child;
 *
 *     g_variant_iter_init (&iter, dictionary);
 *     while ((child = g_variant_iter_next_value (&iter)))
 *       {
 *         g_print ("type '%s'\n", g_variant_get_type_string (child));
 *
 *         if (g_variant_is_container (child))
 *           iterate_container_recursive (child);
 *
 *         g_variant_unref (child);
 *       }
 *   }
 * </programlisting>
 * </example>
 *
 * Since: 2.24
 **/
GVariant *
g_variant_iter_next_value (GVariantIter *iter)
{
  g_return_val_if_fail (is_valid_iter (iter), FALSE);

  if G_UNLIKELY (GVSI(iter)->i >= GVSI(iter)->n)
    {
      g_critical ("g_variant_iter_next_value: must not be called again "
                  "after NULL has already been returned.");
      return NULL;
    }

  GVSI(iter)->i++;

  if (GVSI(iter)->i < GVSI(iter)->n)
    return g_variant_get_child_value (GVSI(iter)->value, GVSI(iter)->i);

  return NULL;
}

/* GVariantBuilder {{{1 */
/**
 * GVariantBuilder:
 *
 * A utility type for constructing container-type #GVariant instances.
 *
 * This is an opaque structure and may only be accessed using the
 * following functions.
 *
 * #GVariantBuilder is not threadsafe in any way.  Do not attempt to
 * access it from more than one thread.
 **/

struct stack_builder
{
  GVariantBuilder *parent;
  GVariantType *type;

  /* type constraint explicitly specified by 'type'.
   * for tuple types, this moves along as we add more items.
   */
  const GVariantType *expected_type;

  /* type constraint implied by previous array item.
   */
  const GVariantType *prev_item_type;

  /* constraints on the number of children.  max = -1 for unlimited. */
  gsize min_items;
  gsize max_items;

  /* dynamically-growing pointer array */
  GVariant **children;
  gsize allocated_children;
  gsize offset;

  /* set to '1' if all items in the container will have the same type
   * (ie: maybe, array, variant) '0' if not (ie: tuple, dict entry)
   */
  guint uniform_item_types : 1;

  /* set to '1' initially and changed to '0' if an untrusted value is
   * added
   */
  guint trusted : 1;

  gsize magic;
};

G_STATIC_ASSERT (sizeof (struct stack_builder) <= sizeof (GVariantBuilder));

struct heap_builder
{
  GVariantBuilder builder;
  gsize magic;

  gint ref_count;
};

#define GVSB(b)                  ((struct stack_builder *) (b))
#define GVHB(b)                  ((struct heap_builder *) (b))
#define GVSB_MAGIC               ((gsize) 1033660112u)
#define GVHB_MAGIC               ((gsize) 3087242682u)
#define is_valid_builder(b)      (b != NULL && \
                                  GVSB(b)->magic == GVSB_MAGIC)
#define is_valid_heap_builder(b) (GVHB(b)->magic == GVHB_MAGIC)

/**
 * g_variant_builder_new:
 * @type: a container type
 * @returns: a #GVariantBuilder
 *
 * Allocates and initialises a new #GVariantBuilder.
 *
 * You should call g_variant_builder_unref() on the return value when it
 * is no longer needed.  The memory will not be automatically freed by
 * any other call.
 *
 * In most cases it is easier to place a #GVariantBuilder directly on
 * the stack of the calling function and initialise it with
 * g_variant_builder_init().
 *
 * Since: 2.24
 **/
GVariantBuilder *
g_variant_builder_new (const GVariantType *type)
{
  GVariantBuilder *builder;

  builder = (GVariantBuilder *) g_slice_new (struct heap_builder);
  g_variant_builder_init (builder, type);
  GVHB(builder)->magic = GVHB_MAGIC;
  GVHB(builder)->ref_count = 1;

  return builder;
}

/**
 * g_variant_builder_unref:
 * @builder: a #GVariantBuilder allocated by g_variant_builder_new()
 *
 * Decreases the reference count on @builder.
 *
 * In the event that there are no more references, releases all memory
 * associated with the #GVariantBuilder.
 *
 * Don't call this on stack-allocated #GVariantBuilder instances or bad
 * things will happen.
 *
 * Since: 2.24
 **/
void
g_variant_builder_unref (GVariantBuilder *builder)
{
  g_return_if_fail (is_valid_heap_builder (builder));

  if (--GVHB(builder)->ref_count)
    return;

  g_variant_builder_clear (builder);
  GVHB(builder)->magic = 0;

  g_slice_free (struct heap_builder, GVHB(builder));
}

/**
 * g_variant_builder_ref:
 * @builder: a #GVariantBuilder allocated by g_variant_builder_new()
 * @returns: a new reference to @builder
 *
 * Increases the reference count on @builder.
 *
 * Don't call this on stack-allocated #GVariantBuilder instances or bad
 * things will happen.
 *
 * Since: 2.24
 **/
GVariantBuilder *
g_variant_builder_ref (GVariantBuilder *builder)
{
  g_return_val_if_fail (is_valid_heap_builder (builder), NULL);

  GVHB(builder)->ref_count++;

  return builder;
}

/**
 * g_variant_builder_clear:
 * @builder: a #GVariantBuilder
 *
 * Releases all memory associated with a #GVariantBuilder without
 * freeing the #GVariantBuilder structure itself.
 *
 * It typically only makes sense to do this on a stack-allocated
 * #GVariantBuilder if you want to abort building the value part-way
 * through.  This function need not be called if you call
 * g_variant_builder_end() and it also doesn't need to be called on
 * builders allocated with g_variant_builder_new (see
 * g_variant_builder_free() for that).
 *
 * This function leaves the #GVariantBuilder structure set to all-zeros.
 * It is valid to call this function on either an initialised
 * #GVariantBuilder or one that is set to all-zeros but it is not valid
 * to call this function on uninitialised memory.
 *
 * Since: 2.24
 **/
void
g_variant_builder_clear (GVariantBuilder *builder)
{
  gsize i;

  if (GVSB(builder)->magic == 0)
    /* all-zeros case */
    return;

  g_return_if_fail (is_valid_builder (builder));

  g_variant_type_free (GVSB(builder)->type);

  for (i = 0; i < GVSB(builder)->offset; i++)
    g_variant_unref (GVSB(builder)->children[i]);

  g_free (GVSB(builder)->children);

  if (GVSB(builder)->parent)
    {
      g_variant_builder_clear (GVSB(builder)->parent);
      g_slice_free (GVariantBuilder, GVSB(builder)->parent);
    }

  memset (builder, 0, sizeof (GVariantBuilder));
}

/**
 * g_variant_builder_init:
 * @builder: a #GVariantBuilder
 * @type: a container type
 *
 * Initialises a #GVariantBuilder structure.
 *
 * @type must be non-%NULL.  It specifies the type of container to
 * construct.  It can be an indefinite type such as
 * %G_VARIANT_TYPE_ARRAY or a definite type such as "as" or "(ii)".
 * Maybe, array, tuple, dictionary entry and variant-typed values may be
 * constructed.
 *
 * After the builder is initialised, values are added using
 * g_variant_builder_add_value() or g_variant_builder_add().
 *
 * After all the child values are added, g_variant_builder_end() frees
 * the memory associated with the builder and returns the #GVariant that
 * was created.
 *
 * This function completely ignores the previous contents of @builder.
 * On one hand this means that it is valid to pass in completely
 * uninitialised memory.  On the other hand, this means that if you are
 * initialising over top of an existing #GVariantBuilder you need to
 * first call g_variant_builder_clear() in order to avoid leaking
 * memory.
 *
 * You must not call g_variant_builder_ref() or
 * g_variant_builder_unref() on a #GVariantBuilder that was initialised
 * with this function.  If you ever pass a reference to a
 * #GVariantBuilder outside of the control of your own code then you
 * should assume that the person receiving that reference may try to use
 * reference counting; you should use g_variant_builder_new() instead of
 * this function.
 *
 * Since: 2.24
 **/
void
g_variant_builder_init (GVariantBuilder    *builder,
                        const GVariantType *type)
{
  g_return_if_fail (type != NULL);
  g_return_if_fail (g_variant_type_is_container (type));

  memset (builder, 0, sizeof (GVariantBuilder));

  GVSB(builder)->type = g_variant_type_copy (type);
  GVSB(builder)->magic = GVSB_MAGIC;
  GVSB(builder)->trusted = TRUE;

  switch (*(const gchar *) type)
    {
    case G_VARIANT_CLASS_VARIANT:
      GVSB(builder)->uniform_item_types = TRUE;
      GVSB(builder)->allocated_children = 1;
      GVSB(builder)->expected_type = NULL;
      GVSB(builder)->min_items = 1;
      GVSB(builder)->max_items = 1;
      break;

    case G_VARIANT_CLASS_ARRAY:
      GVSB(builder)->uniform_item_types = TRUE;
      GVSB(builder)->allocated_children = 8;
      GVSB(builder)->expected_type =
        g_variant_type_element (GVSB(builder)->type);
      GVSB(builder)->min_items = 0;
      GVSB(builder)->max_items = -1;
      break;

    case G_VARIANT_CLASS_MAYBE:
      GVSB(builder)->uniform_item_types = TRUE;
      GVSB(builder)->allocated_children = 1;
      GVSB(builder)->expected_type =
        g_variant_type_element (GVSB(builder)->type);
      GVSB(builder)->min_items = 0;
      GVSB(builder)->max_items = 1;
      break;

    case G_VARIANT_CLASS_DICT_ENTRY:
      GVSB(builder)->uniform_item_types = FALSE;
      GVSB(builder)->allocated_children = 2;
      GVSB(builder)->expected_type =
        g_variant_type_key (GVSB(builder)->type);
      GVSB(builder)->min_items = 2;
      GVSB(builder)->max_items = 2;
      break;

    case 'r': /* G_VARIANT_TYPE_TUPLE was given */
      GVSB(builder)->uniform_item_types = FALSE;
      GVSB(builder)->allocated_children = 8;
      GVSB(builder)->expected_type = NULL;
      GVSB(builder)->min_items = 0;
      GVSB(builder)->max_items = -1;
      break;

    case G_VARIANT_CLASS_TUPLE: /* a definite tuple type was given */
      GVSB(builder)->allocated_children = g_variant_type_n_items (type);
      GVSB(builder)->expected_type =
        g_variant_type_first (GVSB(builder)->type);
      GVSB(builder)->min_items = GVSB(builder)->allocated_children;
      GVSB(builder)->max_items = GVSB(builder)->allocated_children;
      GVSB(builder)->uniform_item_types = FALSE;
      break;

    default:
      g_assert_not_reached ();
   }

  GVSB(builder)->children = g_new (GVariant *,
                                   GVSB(builder)->allocated_children);
}

static void
g_variant_builder_make_room (struct stack_builder *builder)
{
  if (builder->offset == builder->allocated_children)
    {
      builder->allocated_children *= 2;
      builder->children = g_renew (GVariant *, builder->children,
                                   builder->allocated_children);
    }
}

/**
 * g_variant_builder_add_value:
 * @builder: a #GVariantBuilder
 * @value: a #GVariant
 *
 * Adds @value to @builder.
 *
 * It is an error to call this function in any way that would create an
 * inconsistent value to be constructed.  Some examples of this are
 * putting different types of items into an array, putting the wrong
 * types or number of items in a tuple, putting more than one value into
 * a variant, etc.
 *
 * Since: 2.24
 **/
void
g_variant_builder_add_value (GVariantBuilder *builder,
                             GVariant        *value)
{
  g_return_if_fail (is_valid_builder (builder));
  g_return_if_fail (GVSB(builder)->offset < GVSB(builder)->max_items);
  g_return_if_fail (!GVSB(builder)->expected_type ||
                    g_variant_is_of_type (value,
                                          GVSB(builder)->expected_type));
  g_return_if_fail (!GVSB(builder)->prev_item_type ||
                    g_variant_is_of_type (value,
                                          GVSB(builder)->prev_item_type));

  GVSB(builder)->trusted &= g_variant_is_trusted (value);

  if (!GVSB(builder)->uniform_item_types)
    {
      /* advance our expected type pointers */
      if (GVSB(builder)->expected_type)
        GVSB(builder)->expected_type =
          g_variant_type_next (GVSB(builder)->expected_type);

      if (GVSB(builder)->prev_item_type)
        GVSB(builder)->prev_item_type =
          g_variant_type_next (GVSB(builder)->prev_item_type);
    }
  else
    GVSB(builder)->prev_item_type = g_variant_get_type (value);

  g_variant_builder_make_room (GVSB(builder));

  GVSB(builder)->children[GVSB(builder)->offset++] =
    g_variant_ref_sink (value);
}

/**
 * g_variant_builder_open:
 * @builder: a #GVariantBuilder
 * @type: a #GVariantType
 *
 * Opens a subcontainer inside the given @builder.  When done adding
 * items to the subcontainer, g_variant_builder_close() must be called.
 *
 * It is an error to call this function in any way that would cause an
 * inconsistent value to be constructed (ie: adding too many values or
 * a value of an incorrect type).
 *
 * Since: 2.24
 **/
void
g_variant_builder_open (GVariantBuilder    *builder,
                        const GVariantType *type)
{
  GVariantBuilder *parent;

  g_return_if_fail (is_valid_builder (builder));
  g_return_if_fail (GVSB(builder)->offset < GVSB(builder)->max_items);
  g_return_if_fail (!GVSB(builder)->expected_type ||
                    g_variant_type_is_subtype_of (type,
                                                  GVSB(builder)->expected_type));
  g_return_if_fail (!GVSB(builder)->prev_item_type ||
                    g_variant_type_is_subtype_of (GVSB(builder)->prev_item_type,
                                                  type));

  parent = g_slice_dup (GVariantBuilder, builder);
  g_variant_builder_init (builder, type);
  GVSB(builder)->parent = parent;

  /* push the prev_item_type down into the subcontainer */
  if (GVSB(parent)->prev_item_type)
    {
      if (!GVSB(builder)->uniform_item_types)
        /* tuples and dict entries */
        GVSB(builder)->prev_item_type =
          g_variant_type_first (GVSB(parent)->prev_item_type);

      else if (!g_variant_type_is_variant (GVSB(builder)->type))
        /* maybes and arrays */
        GVSB(builder)->prev_item_type =
          g_variant_type_element (GVSB(parent)->prev_item_type);
    }
}

/**
 * g_variant_builder_close:
 * @builder: a #GVariantBuilder
 *
 * Closes the subcontainer inside the given @builder that was opened by
 * the most recent call to g_variant_builder_open().
 *
 * It is an error to call this function in any way that would create an
 * inconsistent value to be constructed (ie: too few values added to the
 * subcontainer).
 *
 * Since: 2.24
 **/
void
g_variant_builder_close (GVariantBuilder *builder)
{
  GVariantBuilder *parent;

  g_return_if_fail (is_valid_builder (builder));
  g_return_if_fail (GVSB(builder)->parent != NULL);

  parent = GVSB(builder)->parent;
  GVSB(builder)->parent = NULL;

  g_variant_builder_add_value (parent, g_variant_builder_end (builder));
  *builder = *parent;

  g_slice_free (GVariantBuilder, parent);
}

/*< private >
 * g_variant_make_maybe_type:
 * @element: a #GVariant
 *
 * Return the type of a maybe containing @element.
 */
static GVariantType *
g_variant_make_maybe_type (GVariant *element)
{
  return g_variant_type_new_maybe (g_variant_get_type (element));
}

/*< private >
 * g_variant_make_array_type:
 * @element: a #GVariant
 *
 * Return the type of an array containing @element.
 */
static GVariantType *
g_variant_make_array_type (GVariant *element)
{
  return g_variant_type_new_array (g_variant_get_type (element));
}

/**
 * g_variant_builder_end:
 * @builder: a #GVariantBuilder
 * @returns: a new, floating, #GVariant
 *
 * Ends the builder process and returns the constructed value.
 *
 * This call automatically reduces the reference count on @builder by
 * one, unless it has previously had g_variant_builder_no_autofree()
 * called on it.  Unless you've taken other actions, this is usually
 * sufficient to free @builder.
 *
 * Even if additional references are held, it is not permissible to use
 * @builder in any way after this call except for further reference
 * counting operations.
 *
 * It is an error to call this function in any way that would create an
 * inconsistent value to be constructed (ie: insufficient number of
 * items added to a container with a specific number of children
 * required).  It is also an error to call this function if the builder
 * was created with an indefinite array or maybe type and no children
 * have been added; in this case it is impossible to infer the type of
 * the empty array.
 *
 * Since: 2.24
 **/
GVariant *
g_variant_builder_end (GVariantBuilder *builder)
{
  GVariantType *my_type;
  GVariant *value;

  g_return_val_if_fail (is_valid_builder (builder), NULL);
  g_return_val_if_fail (GVSB(builder)->offset >= GVSB(builder)->min_items,
                        NULL);
  g_return_val_if_fail (!GVSB(builder)->uniform_item_types ||
                        GVSB(builder)->prev_item_type != NULL ||
                        g_variant_type_is_definite (GVSB(builder)->type),
                        NULL);

  if (g_variant_type_is_definite (GVSB(builder)->type))
    my_type = g_variant_type_copy (GVSB(builder)->type);

  else if (g_variant_type_is_maybe (GVSB(builder)->type))
    my_type = g_variant_make_maybe_type (GVSB(builder)->children[0]);

  else if (g_variant_type_is_array (GVSB(builder)->type))
    my_type = g_variant_make_array_type (GVSB(builder)->children[0]);

  else if (g_variant_type_is_tuple (GVSB(builder)->type))
    my_type = g_variant_make_tuple_type (GVSB(builder)->children,
                                         GVSB(builder)->offset);

  else if (g_variant_type_is_dict_entry (GVSB(builder)->type))
    my_type = g_variant_make_dict_entry_type (GVSB(builder)->children[0],
                                              GVSB(builder)->children[1]);
  else
    g_assert_not_reached ();

  value = g_variant_new_from_children (my_type,
                                       g_renew (GVariant *,
                                                GVSB(builder)->children,
                                                GVSB(builder)->offset),
                                       GVSB(builder)->offset,
                                       GVSB(builder)->trusted);
  GVSB(builder)->children = NULL;
  GVSB(builder)->offset = 0;

  g_variant_builder_clear (builder);
  g_variant_type_free (my_type);

  return value;
}

/* Format strings {{{1 */
/*< private >
 * g_variant_format_string_scan:
 * @string: a string that may be prefixed with a format string
 * @limit: a pointer to the end of @string, or %NULL
 * @endptr: location to store the end pointer, or %NULL
 * @returns: %TRUE if there was a valid format string
 *
 * Checks the string pointed to by @string for starting with a properly
 * formed #GVariant varargs format string.  If no valid format string is
 * found then %FALSE is returned.
 *
 * If @string does start with a valid format string then %TRUE is
 * returned.  If @endptr is non-%NULL then it is updated to point to the
 * first character after the format string.
 *
 * If @limit is non-%NULL then @limit (and any charater after it) will
 * not be accessed and the effect is otherwise equivalent to if the
 * character at @limit were nul.
 *
 * See the section on <link linkend='gvariant-format-strings'>GVariant
 * Format Strings</link>.
 *
 * Since: 2.24
 */
gboolean
g_variant_format_string_scan (const gchar  *string,
                              const gchar  *limit,
                              const gchar **endptr)
{
#define next_char() (string == limit ? '\0' : *string++)
#define peek_char() (string == limit ? '\0' : *string)
  char c;

  switch (next_char())
    {
    case 'b': case 'y': case 'n': case 'q': case 'i': case 'u':
    case 'x': case 't': case 'h': case 'd': case 's': case 'o':
    case 'g': case 'v': case '*': case '?': case 'r':
      break;

    case 'm':
      return g_variant_format_string_scan (string, limit, endptr);

    case 'a':
    case '@':
      return g_variant_type_string_scan (string, limit, endptr);

    case '(':
      while (peek_char() != ')')
        if (!g_variant_format_string_scan (string, limit, &string))
          return FALSE;

      next_char(); /* consume ')' */
      break;

    case '{':
      c = next_char();

      if (c == '&')
        {
          c = next_char ();

          if (c != 's' && c != 'o' && c != 'g')
            return FALSE;
        }
      else
        {
          if (c == '@')
            c = next_char ();

          /* ISO/IEC 9899:1999 (C99) §7.21.5.2:
           *    The terminating null character is considered to be
           *    part of the string.
           */
          if (c != '\0' && strchr ("bynqiuxthdsog?", c) == NULL)
            return FALSE;
        }

      if (!g_variant_format_string_scan (string, limit, &string))
        return FALSE;

      if (next_char() != '}')
        return FALSE;

      break;

    case '^': /* '^as' or '^a&s' only */
      if (next_char() != 'a')
        return FALSE;

      if (peek_char() == '&')
        next_char ();

      c = next_char ();

      if (c != 's' && c != 'o' && c != 'g')
        return FALSE;

      break;

    case '&':
      c = next_char();

      if (c != 's' && c != 'o' && c != 'g')
        return FALSE;

      break;

    default:
      return FALSE;
    }

  if (endptr != NULL)
    *endptr = string;

#undef next_char
#undef peek_char

  return TRUE;
}

/*< private >
 * g_variant_format_string_scan_type:
 * @string: a string that may be prefixed with a format string
 * @limit: a pointer to the end of @string
 * @endptr: location to store the end pointer, or %NULL
 * @returns: a #GVariantType if there was a valid format string
 *
 * If @string starts with a valid format string then this function will
 * return the type that the format string corresponds to.  Otherwise
 * this function returns %NULL.
 *
 * Use g_variant_type_free() to free the return value when you no longer
 * need it.
 *
 * This function is otherwise exactly like
 * g_variant_format_string_scan().
 *
 * Since: 2.24
 */
GVariantType *
g_variant_format_string_scan_type (const gchar  *string,
                                   const gchar  *limit,
                                   const gchar **endptr)
{
  const gchar *my_end;
  gchar *dest;
  gchar *new;

  if (endptr == NULL)
    endptr = &my_end;

  if (!g_variant_format_string_scan (string, limit, endptr))
    return NULL;

  dest = new = g_malloc (*endptr - string + 1);
  while (string != *endptr)
    {
      if (*string != '@' && *string != '&' && *string != '^')
        *dest++ = *string;
      string++;
    }
  *dest = '\0';

  return (GVariantType *) G_VARIANT_TYPE (new);
}

static gboolean
valid_format_string (const gchar *format_string,
                     gboolean     single,
                     GVariant    *value)
{
  const gchar *endptr;
  GVariantType *type;

  type = g_variant_format_string_scan_type (format_string, NULL, &endptr);

  if G_UNLIKELY (type == NULL || (single && *endptr != '\0'))
    {
      if (single)
        g_critical ("`%s' is not a valid GVariant format string",
                    format_string);
      else
        g_critical ("`%s' does not have a valid GVariant format "
                    "string as a prefix", format_string);

      if (type != NULL)
        g_variant_type_free (type);

      return FALSE;
    }

  if G_UNLIKELY (value && !g_variant_is_of_type (value, type))
    {
      gchar *fragment;
      gchar *typestr;

      fragment = g_strndup (format_string, endptr - format_string);
      typestr = g_variant_type_dup_string (type);

      g_critical ("the GVariant format string `%s' has a type of "
                  "`%s' but the given value has a type of `%s'",
                  fragment, typestr, g_variant_get_type_string (value));

      g_variant_type_free (type);

      return FALSE;
    }

  g_variant_type_free (type);

  return TRUE;
}

/* Variable Arguments {{{1 */
/* We consider 2 main classes of format strings:
 *
 *   - recursive format strings
 *      these are ones that result in recursion and the collection of
 *      possibly more than one argument.  Maybe types, tuples,
 *      dictionary entries.
 *
 *   - leaf format string
 *      these result in the collection of a single argument.
 *
 * Leaf format strings are further subdivided into two categories:
 *
 *   - single non-null pointer ("nnp")
 *      these either collect or return a single non-null pointer.
 *
 *   - other
 *      these collect or return something else (bool, number, etc).
 *
 * Based on the above, the varargs handling code is split into 4 main parts:
 *
 *   - nnp handling code
 *   - leaf handling code (which may invoke nnp code)
 *   - generic handling code (may be recursive, may invoke leaf code)
 *   - user-facing API (which invokes the generic code)
 *
 * Each section implements some of the following functions:
 *
 *   - skip:
 *      collect the arguments for the format string as if
 *      g_variant_new() had been called, but do nothing with them.  used
 *      for skipping over arguments when constructing a Nothing maybe
 *      type.
 *
 *   - new:
 *      create a GVariant *
 *
 *   - get:
 *      unpack a GVariant *
 *
 *   - free (nnp only):
 *      free a previously allocated item
 */

static gboolean
g_variant_format_string_is_leaf (const gchar *str)
{
  return str[0] != 'm' && str[0] != '(' && str[0] != '{';
}

static gboolean
g_variant_format_string_is_nnp (const gchar *str)
{
  return str[0] == 'a' || str[0] == 's' || str[0] == 'o' || str[0] == 'g' ||
         str[0] == '^' || str[0] == '@' || str[0] == '*' || str[0] == '?' ||
         str[0] == 'r' || str[0] == 'v' || str[0] == '&';
}

/* Single non-null pointer ("nnp") {{{2 */
static void
g_variant_valist_free_nnp (const gchar *str,
                           gpointer     ptr)
{
  switch (*str)
    {
    case 'a':
      g_variant_iter_free (ptr);
      break;

    case '^':
      if (str[2] != '&')        /* '^as' */
        g_strfreev (ptr);
      else                      /* '^a&s' */
        g_free (ptr);
      break;

    case 's':
    case 'o':
    case 'g':
      g_free (ptr);
      break;

    case '@':
    case '*':
    case '?':
    case 'v':
      g_variant_unref (ptr);
      break;

    case '&':
      break;

    default:
      g_assert_not_reached ();
    }
}

static GVariant *
g_variant_valist_new_nnp (const gchar **str,
                          gpointer      ptr)
{
  if (**str == '&')
    (*str)++;

  switch (*(*str)++)
    {
    case 'a':
      {
        const GVariantType *type;
        GVariant *value;

        value = g_variant_builder_end (ptr);
        type = g_variant_get_type (value);

        if G_UNLIKELY (!g_variant_type_is_array (type))
          g_error ("g_variant_new: expected array GVariantBuilder but "
                   "the built value has type `%s'",
                   g_variant_get_type_string (value));

        type = g_variant_type_element (type);

        if G_UNLIKELY (!g_variant_type_is_subtype_of (type, (GVariantType *) *str))
          g_error ("g_variant_new: expected GVariantBuilder array element "
                   "type `%s' but the built value has element type `%s'",
                   g_variant_type_dup_string ((GVariantType *) *str),
                   g_variant_get_type_string (value) + 1);

        g_variant_type_string_scan (*str, NULL, str);

        return value;
      }

    case 's':
      return g_variant_new_string (ptr);

    case 'o':
      return g_variant_new_object_path (ptr);

    case 'g':
      return g_variant_new_signature (ptr);

    case '^':
      {
        const GVariantType *type;
        GVariantType *array_type;
        GVariant **children;
        gchar **strv = ptr;
        GVariant *value;
        guint length, i;

        if ((*str)[1] == '&')    /* '^a&s' */
          (*str) += 2;
        else                     /* '^as' */
          (*str)++;

        type = (GVariantType *) (*str)++;
        array_type = g_variant_type_new_array (type);
        length = g_strv_length (strv);
        children = g_new (GVariant *, length);
        for (i = 0; i < length; i++)
          children[i] = g_variant_ref_sink (
            g_variant_new_from_trusted (type, strv[i], strlen (strv[i]) + 1));

        value = g_variant_new_from_children (array_type, children,
                                             length, TRUE);
        g_variant_type_free (array_type);

        return value;
      }

    case '@':
      if G_UNLIKELY (!g_variant_is_of_type (ptr, (GVariantType *) *str))
        g_error ("g_variant_new: expected GVariant of type `%s' but "
                 "received value has type `%s'",
                 g_variant_type_dup_string ((GVariantType *) *str),
                 g_variant_get_type_string (ptr));

      g_variant_type_string_scan (*str, NULL, str);

      return ptr;

    case '*':
      return ptr;

    case '?':
      if G_UNLIKELY (!g_variant_type_is_basic (g_variant_get_type (ptr)))
        g_error ("g_variant_new: format string `?' expects basic-typed "
                 "GVariant, but received value has type `%s'",
                 g_variant_get_type_string (ptr));

      return ptr;

    case 'r':
      if G_UNLIKELY (!g_variant_type_is_tuple (g_variant_get_type (ptr)))
        g_error ("g_variant_new: format string `r` expects tuple-typed "
                 "GVariant, but received value has type `%s'",
                 g_variant_get_type_string (ptr));

      return ptr;

    case 'v':
      return g_variant_new_variant (ptr);

    default:
      g_assert_not_reached ();
    }
}

static gpointer
g_variant_valist_get_nnp (const gchar **str,
                          GVariant     *value)
{
  switch (*(*str)++)
    {
    case 'a':
      g_variant_type_string_scan (*str, NULL, str);
      return g_variant_iter_new (value);

    case '&':
      (*str)++;
      return (gchar *) g_variant_get_string (value, NULL);

    case 's':
    case 'o':
    case 'g':
      return g_variant_dup_string (value, NULL);

    case '^':
      if ((*str)[1] == '&')    /* '^a&s' */
        {
          (*str) += 3;
          return g_variant_get_strv (value, NULL);
        }
      else                    /* '^as' */
        {
          (*str) += 2;
          return g_variant_dup_strv (value, NULL);
        }

    case '@':
      g_variant_type_string_scan (*str, NULL, str);
      /* fall through */

    case '*':
    case '?':
    case 'r':
      return g_variant_ref (value);

    case 'v':
      return g_variant_get_variant (value);

    default:
      g_assert_not_reached ();
    }
}

/* Leaves {{{2 */
static void
g_variant_valist_skip_leaf (const gchar **str,
                            va_list      *app)
{
  if (g_variant_format_string_is_nnp (*str))
    {
      g_variant_format_string_scan (*str, NULL, str);
      va_arg (*app, gpointer);
      return;
    }

  switch (*(*str)++)
    {
    case 'b':
    case 'y':
    case 'n':
    case 'q':
    case 'i':
    case 'u':
    case 'h':
      va_arg (*app, int);
      return;

    case 'x':
    case 't':
      va_arg (*app, guint64);
      return;

    case 'd':
      va_arg (*app, gdouble);
      return;

    default:
      g_assert_not_reached ();
    }
}

static GVariant *
g_variant_valist_new_leaf (const gchar **str,
                           va_list      *app)
{
  if (g_variant_format_string_is_nnp (*str))
    return g_variant_valist_new_nnp (str, va_arg (*app, gpointer));

  switch (*(*str)++)
    {
    case 'b':
      return g_variant_new_boolean (va_arg (*app, gboolean));

    case 'y':
      return g_variant_new_byte (va_arg (*app, guint));

    case 'n':
      return g_variant_new_int16 (va_arg (*app, gint));

    case 'q':
      return g_variant_new_uint16 (va_arg (*app, guint));

    case 'i':
      return g_variant_new_int32 (va_arg (*app, gint));

    case 'u':
      return g_variant_new_uint32 (va_arg (*app, guint));

    case 'x':
      return g_variant_new_int64 (va_arg (*app, gint64));

    case 't':
      return g_variant_new_uint64 (va_arg (*app, guint64));

    case 'h':
      return g_variant_new_handle (va_arg (*app, gint));

    case 'd':
      return g_variant_new_double (va_arg (*app, gdouble));

    default:
      g_assert_not_reached ();
    }
}

/* The code below assumes this */
G_STATIC_ASSERT (sizeof (gboolean) == sizeof (guint32));
G_STATIC_ASSERT (sizeof (gdouble) == sizeof (guint64));

static void
g_variant_valist_get_leaf (const gchar **str,
                           GVariant     *value,
                           gboolean      free,
                           va_list      *app)
{
  gpointer ptr = va_arg (*app, gpointer);

  if (ptr == NULL)
    {
      g_variant_format_string_scan (*str, NULL, str);
      return;
    }

  if (g_variant_format_string_is_nnp (*str))
    {
      gpointer *nnp = (gpointer *) ptr;

      if (free && *nnp != NULL)
        g_variant_valist_free_nnp (*str, *nnp);

      *nnp = NULL;

      if (value != NULL)
        *nnp = g_variant_valist_get_nnp (str, value);
      else
        g_variant_format_string_scan (*str, NULL, str);

      return;
    }

  if (value != NULL)
    {
      switch (*(*str)++)
        {
        case 'b':
          *(gboolean *) ptr = g_variant_get_boolean (value);
          return;

        case 'y':
          *(guchar *) ptr = g_variant_get_byte (value);
          return;

        case 'n':
          *(gint16 *) ptr = g_variant_get_int16 (value);
          return;

        case 'q':
          *(guint16 *) ptr = g_variant_get_uint16 (value);
          return;

        case 'i':
          *(gint32 *) ptr = g_variant_get_int32 (value);
          return;

        case 'u':
          *(guint32 *) ptr = g_variant_get_uint32 (value);
          return;

        case 'x':
          *(gint64 *) ptr = g_variant_get_int64 (value);
          return;

        case 't':
          *(guint64 *) ptr = g_variant_get_uint64 (value);
          return;

        case 'h':
          *(gint32 *) ptr = g_variant_get_handle (value);
          return;

        case 'd':
          *(gdouble *) ptr = g_variant_get_double (value);
          return;
        }
    }
  else
    {
      switch (*(*str)++)
        {
        case 'y':
          *(guchar *) ptr = 0;
          return;

        case 'n':
        case 'q':
          *(guint16 *) ptr = 0;
          return;

        case 'i':
        case 'u':
        case 'h':
        case 'b':
          *(guint32 *) ptr = 0;
          return;

        case 'x':
        case 't':
        case 'd':
          *(guint64 *) ptr = 0;
          return;
        }
    }

  g_assert_not_reached ();
}

/* Generic (recursive) {{{2 */
static void
g_variant_valist_skip (const gchar **str,
                       va_list      *app)
{
  if (g_variant_format_string_is_leaf (*str))
    g_variant_valist_skip_leaf (str, app);

  else if (**str == 'm') /* maybe */
    {
      (*str)++;

      if (!g_variant_format_string_is_nnp (*str))
        va_arg (*app, gboolean);

      g_variant_valist_skip (str, app);
    }
  else /* tuple, dictionary entry */
    {
      g_assert (**str == '(' || **str == '{');
      (*str)++;
      while (**str != ')' && **str != '}')
        g_variant_valist_skip (str, app);
      (*str)++;
    }
}

static GVariant *
g_variant_valist_new (const gchar **str,
                      va_list      *app)
{
  if (g_variant_format_string_is_leaf (*str))
    return g_variant_valist_new_leaf (str, app);

  if (**str == 'm') /* maybe */
    {
      GVariantType *type = NULL;
      GVariant *value = NULL;

      (*str)++;

      if (g_variant_format_string_is_nnp (*str))
        {
          gpointer nnp = va_arg (*app, gpointer);

          if (nnp != NULL)
            value = g_variant_valist_new_nnp (str, nnp);
          else
            type = g_variant_format_string_scan_type (*str, NULL, str);
        }
      else
        {
          gboolean just = va_arg (*app, gboolean);

          if (just)
            value = g_variant_valist_new (str, app);
          else
            {
              type = g_variant_format_string_scan_type (*str, NULL, NULL);
              g_variant_valist_skip (str, app);
            }
        }

      value = g_variant_new_maybe (type, value);

      if (type != NULL)
        g_variant_type_free (type);

      return value;
    }
  else /* tuple, dictionary entry */
    {
      GVariantBuilder b;

      if (**str == '(')
        g_variant_builder_init (&b, G_VARIANT_TYPE_TUPLE);
      else
        {
          g_assert (**str == '{');
          g_variant_builder_init (&b, G_VARIANT_TYPE_DICT_ENTRY);
        }

      (*str)++; /* '(' */
      while (**str != ')' && **str != '}')
        g_variant_builder_add_value (&b, g_variant_valist_new (str, app));
      (*str)++; /* ')' */

      return g_variant_builder_end (&b);
    }
}

static void
g_variant_valist_get (const gchar **str,
                      GVariant     *value,
                      gboolean      free,
                      va_list      *app)
{
  if (g_variant_format_string_is_leaf (*str))
    g_variant_valist_get_leaf (str, value, free, app);

  else if (**str == 'm')
    {
      (*str)++;

      if (value != NULL)
        value = g_variant_get_maybe (value);

      if (!g_variant_format_string_is_nnp (*str))
        {
          gboolean *ptr = va_arg (*app, gboolean *);

          if (ptr != NULL)
            *ptr = value != NULL;
        }

      g_variant_valist_get (str, value, free, app);

      if (value != NULL)
        g_variant_unref (value);
    }

  else /* tuple, dictionary entry */
    {
      gint index = 0;

      g_assert (**str == '(' || **str == '{');

      (*str)++;
      while (**str != ')' && **str != '}')
        {
          if (value != NULL)
            {
              GVariant *child = g_variant_get_child_value (value, index++);
              g_variant_valist_get (str, child, free, app);
              g_variant_unref (child);
            }
          else
            g_variant_valist_get (str, NULL, free, app);
        }
      (*str)++;
    }
}

/* User-facing API {{{2 */
/**
 * g_variant_new:
 * @format_string: a #GVariant format string
 * @...: arguments, as per @format_string
 * @returns: a new floating #GVariant instance
 *
 * Creates a new #GVariant instance.
 *
 * Think of this function as an analogue to g_strdup_printf().
 *
 * The type of the created instance and the arguments that are
 * expected by this function are determined by @format_string.  See the
 * section on <link linkend='gvariant-format-strings'>GVariant Format
 * Strings</link>.  Please note that the syntax of the format string is
 * very likely to be extended in the future.
 *
 * The first character of the format string must not be '*' '?' '@' or
 * 'r'; in essence, a new #GVariant must always be constructed by this
 * function (and not merely passed through it unmodified).
 *
 * Since: 2.24
 **/
GVariant *
g_variant_new (const gchar *format_string,
               ...)
{
  GVariant *value;
  va_list ap;

  g_return_val_if_fail (valid_format_string (format_string, TRUE, NULL) &&
                        format_string[0] != '?' && format_string[0] != '@' &&
                        format_string[0] != '*' && format_string[0] != 'r',
                        NULL);

  va_start (ap, format_string);
  value = g_variant_new_va (format_string, NULL, &ap);
  va_end (ap);

  return value;
}

/**
 * g_variant_new_va:
 * @format_string: a string that is prefixed with a format string
 * @endptr: location to store the end pointer, or %NULL
 * @app: a pointer to a #va_list
 * @returns: a new, usually floating, #GVariant
 *
 * This function is intended to be used by libraries based on
 * #GVariant that want to provide g_variant_new()-like functionality
 * to their users.
 *
 * The API is more general than g_variant_new() to allow a wider range
 * of possible uses.
 *
 * @format_string must still point to a valid format string, but it only
 * needs to be nul-terminated if @endptr is %NULL.  If @endptr is
 * non-%NULL then it is updated to point to the first character past the
 * end of the format string.
 *
 * @app is a pointer to a #va_list.  The arguments, according to
 * @format_string, are collected from this #va_list and the list is left
 * pointing to the argument following the last.
 *
 * These two generalisations allow mixing of multiple calls to
 * g_variant_new_va() and g_variant_get_va() within a single actual
 * varargs call by the user.
 *
 * The return value will be floating if it was a newly created GVariant
 * instance (for example, if the format string was "(ii)").  In the case
 * that the format_string was '*', '?', 'r', or a format starting with
 * '@' then the collected #GVariant pointer will be returned unmodified,
 * without adding any additional references.
 *
 * In order to behave correctly in all cases it is necessary for the
 * calling function to g_variant_ref_sink() the return result before
 * returning control to the user that originally provided the pointer.
 * At this point, the caller will have their own full reference to the
 * result.  This can also be done by adding the result to a container,
 * or by passing it to another g_variant_new() call.
 *
 * Since: 2.24
 **/
GVariant *
g_variant_new_va (const gchar  *format_string,
                  const gchar **endptr,
                  va_list      *app)
{
  GVariant *value;

  g_return_val_if_fail (valid_format_string (format_string, !endptr, NULL),
                        NULL);
  g_return_val_if_fail (app != NULL, NULL);

  value = g_variant_valist_new (&format_string, app);

  if (endptr != NULL)
    *endptr = format_string;

  return value;
}

/**
 * g_variant_get:
 * @value: a #GVariant instance
 * @format_string: a #GVariant format string
 * @...: arguments, as per @format_string
 *
 * Deconstructs a #GVariant instance.
 *
 * Think of this function as an analogue to scanf().
 *
 * The arguments that are expected by this function are entirely
 * determined by @format_string.  @format_string also restricts the
 * permissible types of @value.  It is an error to give a value with
 * an incompatible type.  See the section on <link
 * linkend='gvariant-format-strings'>GVariant Format Strings</link>.
 * Please note that the syntax of the format string is very likely to be
 * extended in the future.
 *
 * Since: 2.24
 **/
void
g_variant_get (GVariant    *value,
               const gchar *format_string,
               ...)
{
  va_list ap;

  g_return_if_fail (valid_format_string (format_string, TRUE, value));

  /* if any direct-pointer-access formats are in use, flatten first */
  if (strchr (format_string, '&'))
    g_variant_get_data (value);

  va_start (ap, format_string);
  g_variant_get_va (value, format_string, NULL, &ap);
  va_end (ap);
}

/**
 * g_variant_get_va:
 * @value: a #GVariant
 * @format_string: a string that is prefixed with a format string
 * @endptr: location to store the end pointer, or %NULL
 * @app: a pointer to a #va_list
 *
 * This function is intended to be used by libraries based on #GVariant
 * that want to provide g_variant_get()-like functionality to their
 * users.
 *
 * The API is more general than g_variant_get() to allow a wider range
 * of possible uses.
 *
 * @format_string must still point to a valid format string, but it only
 * need to be nul-terminated if @endptr is %NULL.  If @endptr is
 * non-%NULL then it is updated to point to the first character past the
 * end of the format string.
 *
 * @app is a pointer to a #va_list.  The arguments, according to
 * @format_string, are collected from this #va_list and the list is left
 * pointing to the argument following the last.
 *
 * These two generalisations allow mixing of multiple calls to
 * g_variant_new_va() and g_variant_get_va() within a single actual
 * varargs call by the user.
 *
 * Since: 2.24
 **/
void
g_variant_get_va (GVariant     *value,
                  const gchar  *format_string,
                  const gchar **endptr,
                  va_list      *app)
{
  g_return_if_fail (valid_format_string (format_string, !endptr, value));
  g_return_if_fail (value != NULL);
  g_return_if_fail (app != NULL);

  /* if any direct-pointer-access formats are in use, flatten first */
  if (strchr (format_string, '&'))
    g_variant_get_data (value);

  g_variant_valist_get (&format_string, value, FALSE, app);

  if (endptr != NULL)
    *endptr = format_string;
}

/* Varargs-enabled Utility Functions {{{1 */

/**
 * g_variant_builder_add:
 * @builder: a #GVariantBuilder
 * @format_string: a #GVariant varargs format string
 * @...: arguments, as per @format_string
 *
 * Adds to a #GVariantBuilder.
 *
 * This call is a convenience wrapper that is exactly equivalent to
 * calling g_variant_new() followed by g_variant_builder_add_value().
 *
 * This function might be used as follows:
 *
 * <programlisting>
 * GVariant *
 * make_pointless_dictionary (void)
 * {
 *   GVariantBuilder *builder;
 *   int i;
 *
 *   builder = g_variant_builder_new (G_VARIANT_TYPE_CLASS_ARRAY,
 *                                    NULL);
 *   for (i = 0; i < 16; i++)
 *     {
 *       gchar buf[3];
 *
 *       sprintf (buf, "%d", i);
 *       g_variant_builder_add (builder, "{is}", i, buf);
 *     }
 *
 *   return g_variant_builder_end (builder);
 * }
 * </programlisting>
 *
 * Since: 2.24
 **/
void
g_variant_builder_add (GVariantBuilder *builder,
                       const gchar     *format_string,
                       ...)
{
  GVariant *variant;
  va_list ap;

  va_start (ap, format_string);
  variant = g_variant_new_va (format_string, NULL, &ap);
  va_end (ap);

  g_variant_builder_add_value (builder, variant);
}

/**
 * g_variant_get_child:
 * @value: a container #GVariant
 * @index_: the index of the child to deconstruct
 * @format_string: a #GVariant format string
 * @...: arguments, as per @format_string
 *
 * Reads a child item out of a container #GVariant instance and
 * deconstructs it according to @format_string.  This call is
 * essentially a combination of g_variant_get_child_value() and
 * g_variant_get().
 *
 * Since: 2.24
 **/
void
g_variant_get_child (GVariant    *value,
                     gsize        index_,
                     const gchar *format_string,
                     ...)
{
  GVariant *child;
  va_list ap;

  child = g_variant_get_child_value (value, index_);
  g_return_if_fail (valid_format_string (format_string, TRUE, child));

  va_start (ap, format_string);
  g_variant_get_va (child, format_string, NULL, &ap);
  va_end (ap);

  g_variant_unref (child);
}

/**
 * g_variant_iter_next:
 * @iter: a #GVariantIter
 * @format_string: a GVariant format string
 * @...: the arguments to unpack the value into
 * @returns: %TRUE if a value was unpacked, or %FALSE if there as no
 *           value
 *
 * Gets the next item in the container and unpacks it into the variable
 * argument list according to @format_string, returning %TRUE.
 *
 * If no more items remain then %FALSE is returned.
 *
 * All of the pointers given on the variable arguments list of this
 * function are assumed to point at uninitialised memory.  It is the
 * responsibility of the caller to free all of the values returned by
 * the unpacking process.
 *
 * <example>
 *  <title>Memory management with g_variant_iter_next()</title>
 *  <programlisting>
 *   /<!-- -->* Iterates a dictionary of type 'a{sv}' *<!-- -->/
 *   void
 *   iterate_dictionary (GVariant *dictionary)
 *   {
 *     GVariantIter iter;
 *     GVariant *value;
 *     gchar *key;
 *
 *     g_variant_iter_init (&iter, dictionary);
 *     while (g_variant_iter_next (&iter, "{sv}", &key, &value))
 *       {
 *         g_print ("Item '%s' has type '%s'\n", key,
 *                  g_variant_get_type_string (value));
 *
 *         /<!-- -->* must free data for ourselves *<!-- -->/
 *         g_variant_unref (value);
 *         g_free (key);
 *       }
 *   }
 *  </programlisting>
 * </example>
 *
 * For a solution that is likely to be more convenient to C programmers
 * when dealing with loops, see g_variant_iter_loop().
 *
 * Since: 2.24
 **/
gboolean
g_variant_iter_next (GVariantIter *iter,
                     const gchar  *format_string,
                     ...)
{
  GVariant *value;

  value = g_variant_iter_next_value (iter);

  g_return_val_if_fail (valid_format_string (format_string, TRUE, value),
                        FALSE);

  if (value != NULL)
    {
      va_list ap;

      va_start (ap, format_string);
      g_variant_valist_get (&format_string, value, FALSE, &ap);
      va_end (ap);

      g_variant_unref (value);
    }

  return value != NULL;
}

/**
 * g_variant_iter_loop:
 * @iter: a #GVariantIter
 * @format_string: a GVariant format string
 * @...: the arguments to unpack the value into
 * @returns: %TRUE if a value was unpacked, or %FALSE if there as no
 *           value
 *
 * Gets the next item in the container and unpacks it into the variable
 * argument list according to @format_string, returning %TRUE.
 *
 * If no more items remain then %FALSE is returned.
 *
 * On the first call to this function, the pointers appearing on the
 * variable argument list are assumed to point at uninitialised memory.
 * On the second and later calls, it is assumed that the same pointers
 * will be given and that they will point to the memory as set by the
 * previous call to this function.  This allows the previous values to
 * be freed, as appropriate.
 *
 * This function is intended to be used with a while loop as
 * demonstrated in the following example.  This function can only be
 * used when iterating over an array.  It is only valid to call this
 * function with a string constant for the format string and the same
 * string constant must be used each time.  Mixing calls to this
 * function and g_variant_iter_next() or g_variant_iter_next_value() on
 * the same iterator is not recommended.
 *
 * <example>
 *  <title>Memory management with g_variant_iter_loop()</title>
 *  <programlisting>
 *   /<!-- -->* Iterates a dictionary of type 'a{sv}' *<!-- -->/
 *   void
 *   iterate_dictionary (GVariant *dictionary)
 *   {
 *     GVariantIter iter;
 *     GVariant *value;
 *     gchar *key;
 *
 *     g_variant_iter_init (&iter, dictionary);
 *     while (g_variant_iter_loop (&iter, "{sv}", &key, &value))
 *       {
 *         g_print ("Item '%s' has type '%s'\n", key,
 *                  g_variant_get_type_string (value));
 *
 *         /<!-- -->* no need to free 'key' and 'value' here *<!-- -->/
 *       }
 *   }
 *  </programlisting>
 * </example>
 *
 * If you want a slightly less magical alternative that requires more
 * typing, see g_variant_iter_next().
 *
 * Since: 2.24
 **/
gboolean
g_variant_iter_loop (GVariantIter *iter,
                     const gchar  *format_string,
                     ...)
{
  gboolean first_time = GVSI(iter)->loop_format == NULL;
  GVariant *value;
  va_list ap;

  g_return_val_if_fail (first_time ||
                        format_string == GVSI(iter)->loop_format,
                        FALSE);

  if (first_time)
    {
      TYPE_CHECK (GVSI(iter)->value, G_VARIANT_TYPE_ARRAY, FALSE);
      GVSI(iter)->loop_format = format_string;

      if (strchr (format_string, '&'))
        g_variant_get_data (GVSI(iter)->value);
    }

  value = g_variant_iter_next_value (iter);

  g_return_val_if_fail (!first_time ||
                        valid_format_string (format_string, TRUE, value),
                        FALSE);

  va_start (ap, format_string);
  g_variant_valist_get (&format_string, value, !first_time, &ap);
  va_end (ap);

  if (value != NULL)
    g_variant_unref (value);

  return value != NULL;
}

/* Serialised data {{{1 */
static GVariant *
g_variant_deep_copy (GVariant *value)
{
  switch (g_variant_classify (value))
    {
    case G_VARIANT_CLASS_MAYBE:
    case G_VARIANT_CLASS_ARRAY:
    case G_VARIANT_CLASS_TUPLE:
    case G_VARIANT_CLASS_DICT_ENTRY:
    case G_VARIANT_CLASS_VARIANT:
      {
        GVariantBuilder builder;
        GVariantIter iter;
        GVariant *child;

        g_variant_builder_init (&builder, g_variant_get_type (value));
        g_variant_iter_init (&iter, value);

        while ((child = g_variant_iter_next_value (&iter)))
          {
            g_variant_builder_add_value (&builder, g_variant_deep_copy (child));
            g_variant_unref (child);
          }

        return g_variant_builder_end (&builder);
      }

    case G_VARIANT_CLASS_BOOLEAN:
      return g_variant_new_boolean (g_variant_get_boolean (value));

    case G_VARIANT_CLASS_BYTE:
      return g_variant_new_byte (g_variant_get_byte (value));

    case G_VARIANT_CLASS_INT16:
      return g_variant_new_int16 (g_variant_get_int16 (value));

    case G_VARIANT_CLASS_UINT16:
      return g_variant_new_uint16 (g_variant_get_uint16 (value));

    case G_VARIANT_CLASS_INT32:
      return g_variant_new_int32 (g_variant_get_int32 (value));

    case G_VARIANT_CLASS_UINT32:
      return g_variant_new_uint32 (g_variant_get_uint32 (value));

    case G_VARIANT_CLASS_INT64:
      return g_variant_new_int64 (g_variant_get_int64 (value));

    case G_VARIANT_CLASS_UINT64:
      return g_variant_new_uint64 (g_variant_get_uint64 (value));

    case G_VARIANT_CLASS_HANDLE:
      return g_variant_new_handle (g_variant_get_handle (value));

    case G_VARIANT_CLASS_DOUBLE:
      return g_variant_new_double (g_variant_get_double (value));

    case G_VARIANT_CLASS_STRING:
      return g_variant_new_string (g_variant_get_string (value, NULL));

    case G_VARIANT_CLASS_OBJECT_PATH:
      return g_variant_new_object_path (g_variant_get_string (value, NULL));

    case G_VARIANT_CLASS_SIGNATURE:
      return g_variant_new_signature (g_variant_get_string (value, NULL));
    }

  g_assert_not_reached ();
}

/**
 * g_variant_get_normal_form:
 * @value: a #GVariant
 * @returns: a trusted #GVariant
 *
 * Gets a #GVariant instance that has the same value as @value and is
 * trusted to be in normal form.
 *
 * If @value is already trusted to be in normal form then a new
 * reference to @value is returned.
 *
 * If @value is not already trusted, then it is scanned to check if it
 * is in normal form.  If it is found to be in normal form then it is
 * marked as trusted and a new reference to it is returned.
 *
 * If @value is found not to be in normal form then a new trusted
 * #GVariant is created with the same value as @value.
 *
 * It makes sense to call this function if you've received #GVariant
 * data from untrusted sources and you want to ensure your serialised
 * output is definitely in normal form.
 *
 * Since: 2.24
 **/
GVariant *
g_variant_get_normal_form (GVariant *value)
{
  GVariant *trusted;

  if (g_variant_is_normal_form (value))
    return g_variant_ref (value);

  trusted = g_variant_deep_copy (value);
  g_assert (g_variant_is_trusted (trusted));

  return g_variant_ref_sink (trusted);
}

/**
 * g_variant_byteswap:
 * @value: a #GVariant
 * @returns: the byteswapped form of @value
 *
 * Performs a byteswapping operation on the contents of @value.  The
 * result is that all multi-byte numeric data contained in @value is
 * byteswapped.  That includes 16, 32, and 64bit signed and unsigned
 * integers as well as file handles and double precision floating point
 * values.
 *
 * This function is an identity mapping on any value that does not
 * contain multi-byte numeric data.  That include strings, booleans,
 * bytes and containers containing only these things (recursively).
 *
 * The returned value is always in normal form and is marked as trusted.
 *
 * Since: 2.24
 **/
GVariant *
g_variant_byteswap (GVariant *value)
{
  GVariantSerialised serialised;
  GVariant *trusted;
  GBuffer *buffer;
  GVariant *new;

  trusted = g_variant_get_normal_form (value);
  serialised.type_info = g_variant_get_type_info (trusted);
  serialised.size = g_variant_get_size (trusted);
  serialised.data = g_malloc (serialised.size);
  g_variant_store (trusted, serialised.data);
  g_variant_unref (trusted);

  g_variant_serialised_byteswap (serialised);

  buffer = g_buffer_new_take_data (serialised.data, serialised.size);
  new = g_variant_new_from_buffer (g_variant_get_type (value), buffer, TRUE);
  g_buffer_unref (buffer);

  return g_variant_ref_sink (new);
}

/**
 * g_variant_new_from_data:
 * @type: a definite #GVariantType
 * @data: the serialised data
 * @size: the size of @data
 * @trusted: %TRUE if @data is definitely in normal form
 * @notify: function to call when @data is no longer needed
 * @user_data: data for @notify
 * @returns: a new floating #GVariant of type @type
 *
 * Creates a new #GVariant instance from serialised data.
 *
 * @type is the type of #GVariant instance that will be constructed.
 * The interpretation of @data depends on knowing the type.
 *
 * @data is not modified by this function and must remain valid with an
 * unchanging value until such a time as @notify is called with
 * @user_data.  If the contents of @data change before that time then
 * the result is undefined.
 *
 * If @data is trusted to be serialised data in normal form then
 * @trusted should be %TRUE.  This applies to serialised data created
 * within this process or read from a trusted location on the disk (such
 * as a file installed in /usr/lib alongside your application).  You
 * should set trusted to %FALSE if @data is read from the network, a
 * file in the user's home directory, etc.
 *
 * @notify will be called with @user_data when @data is no longer
 * needed.  The exact time of this call is unspecified and might even be
 * before this function returns.
 *
 * Since: 2.24
 **/
GVariant *
g_variant_new_from_data (const GVariantType *type,
                         gconstpointer       data,
                         gsize               size,
                         gboolean            trusted,
                         GDestroyNotify      notify,
                         gpointer            user_data)
{
  GVariant *value;
  GBuffer *buffer;

  g_return_val_if_fail (g_variant_type_is_definite (type), NULL);
  g_return_val_if_fail (data != NULL || size == 0, NULL);

  if (notify)
    buffer = g_buffer_new_from_pointer (data, size, notify, user_data);
  else
    buffer = g_buffer_new_from_static_data (data, size);

  value = g_variant_new_from_buffer (type, buffer, trusted);
  g_buffer_unref (buffer);

  return value;
}

/* Epilogue {{{1 */
#define __G_VARIANT_C__
#include "galiasdef.c"

/* vim:set foldmethod=marker: */
