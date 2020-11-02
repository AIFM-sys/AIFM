/* GIO - GLib Input, Output and Streaming Library
 * 
 * Copyright (C) 2006-2007 Red Hat, Inc.
 * Copyright (C) 2007 Jürg Billeter
 * Copyright © 2009 Codethink Limited
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Alexander Larsson <alexl@redhat.com>
 */

#include "config.h"
#include "gdatainputstream.h"
#include "gsimpleasyncresult.h"
#include "gcancellable.h"
#include "gioenumtypes.h"
#include "gioerror.h"
#include "glibintl.h"

#include "gioalias.h"

/**
 * SECTION:gdatainputstream
 * @short_description: Data Input Stream
 * @include: gio/gio.h
 * @see_also: #GInputStream
 * 
 * Data input stream implements #GInputStream and includes functions for 
 * reading structured data directly from a binary input stream.
 *
 **/

struct _GDataInputStreamPrivate {
  GDataStreamByteOrder byte_order;
  GDataStreamNewlineType newline_type;
};

enum {
  PROP_0,
  PROP_BYTE_ORDER,
  PROP_NEWLINE_TYPE
};

static void g_data_input_stream_set_property (GObject      *object,
					      guint         prop_id,
					      const GValue *value,
					      GParamSpec   *pspec);
static void g_data_input_stream_get_property (GObject      *object,
					      guint         prop_id,
					      GValue       *value,
					      GParamSpec   *pspec);

G_DEFINE_TYPE (GDataInputStream,
               g_data_input_stream,
               G_TYPE_BUFFERED_INPUT_STREAM)


static void
g_data_input_stream_class_init (GDataInputStreamClass *klass)
{
  GObjectClass *object_class;

  g_type_class_add_private (klass, sizeof (GDataInputStreamPrivate));

  object_class = G_OBJECT_CLASS (klass);
  object_class->get_property = g_data_input_stream_get_property;
  object_class->set_property = g_data_input_stream_set_property;

  /**
   * GDataStream:byte-order:
   *
   * The ::byte-order property determines the byte ordering that
   * is used when reading multi-byte entities (such as integers)
   * from the stream.
   */ 
  g_object_class_install_property (object_class,
                                   PROP_BYTE_ORDER,
                                   g_param_spec_enum ("byte-order",
                                                      P_("Byte order"),
                                                      P_("The byte order"),
                                                      G_TYPE_DATA_STREAM_BYTE_ORDER,
                                                      G_DATA_STREAM_BYTE_ORDER_BIG_ENDIAN,
                                                      G_PARAM_READWRITE|G_PARAM_STATIC_NAME|G_PARAM_STATIC_BLURB));

  /**
   * GDataStream:newline-type:
   *
   * The :newline-type property determines what is considered
   * as a line ending when reading complete lines from the stream.
   */ 
  g_object_class_install_property (object_class,
                                   PROP_NEWLINE_TYPE,
                                   g_param_spec_enum ("newline-type",
                                                      P_("Newline type"),
                                                      P_("The accepted types of line ending"),
                                                      G_TYPE_DATA_STREAM_NEWLINE_TYPE,
                                                      G_DATA_STREAM_NEWLINE_TYPE_LF,
                                                      G_PARAM_READWRITE|G_PARAM_STATIC_NAME|G_PARAM_STATIC_BLURB));
}

static void
g_data_input_stream_set_property (GObject      *object,
				  guint         prop_id,
				  const GValue *value,
				  GParamSpec   *pspec)
{
  GDataInputStream        *dstream;

  dstream = G_DATA_INPUT_STREAM (object);

   switch (prop_id)
    {
    case PROP_BYTE_ORDER:
      g_data_input_stream_set_byte_order (dstream, g_value_get_enum (value));
      break;

    case PROP_NEWLINE_TYPE:
      g_data_input_stream_set_newline_type (dstream, g_value_get_enum (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }

}

static void
g_data_input_stream_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  GDataInputStreamPrivate *priv;
  GDataInputStream        *dstream;

  dstream = G_DATA_INPUT_STREAM (object);
  priv = dstream->priv;

  switch (prop_id)
    { 
    case PROP_BYTE_ORDER:
      g_value_set_enum (value, priv->byte_order);
      break;

    case PROP_NEWLINE_TYPE:
      g_value_set_enum (value, priv->newline_type);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }

}
static void
g_data_input_stream_init (GDataInputStream *stream)
{
  stream->priv = G_TYPE_INSTANCE_GET_PRIVATE (stream,
                                              G_TYPE_DATA_INPUT_STREAM,
                                              GDataInputStreamPrivate);

  stream->priv->byte_order = G_DATA_STREAM_BYTE_ORDER_BIG_ENDIAN;
  stream->priv->newline_type = G_DATA_STREAM_NEWLINE_TYPE_LF;
}

/**
 * g_data_input_stream_new:
 * @base_stream: a #GInputStream.
 * 
 * Creates a new data input stream for the @base_stream.
 * 
 * Returns: a new #GDataInputStream.
 **/
GDataInputStream *
g_data_input_stream_new (GInputStream *base_stream)
{
  GDataInputStream *stream;

  g_return_val_if_fail (G_IS_INPUT_STREAM (base_stream), NULL);

  stream = g_object_new (G_TYPE_DATA_INPUT_STREAM,
                         "base-stream", base_stream,
                         NULL);

  return stream;
}

/**
 * g_data_input_stream_set_byte_order:
 * @stream: a given #GDataInputStream.
 * @order: a #GDataStreamByteOrder to set.
 * 
 * This function sets the byte order for the given @stream. All subsequent
 * reads from the @stream will be read in the given @order.
 *  
 **/
void
g_data_input_stream_set_byte_order (GDataInputStream     *stream,
				    GDataStreamByteOrder  order)
{
  GDataInputStreamPrivate *priv;

  g_return_if_fail (G_IS_DATA_INPUT_STREAM (stream));

  priv = stream->priv;

  if (priv->byte_order != order)
    {
      priv->byte_order = order;
      
      g_object_notify (G_OBJECT (stream), "byte-order");
    }
}

/**
 * g_data_input_stream_get_byte_order:
 * @stream: a given #GDataInputStream.
 * 
 * Gets the byte order for the data input stream.
 * 
 * Returns: the @stream's current #GDataStreamByteOrder. 
 **/
GDataStreamByteOrder
g_data_input_stream_get_byte_order (GDataInputStream *stream)
{
  g_return_val_if_fail (G_IS_DATA_INPUT_STREAM (stream), G_DATA_STREAM_BYTE_ORDER_HOST_ENDIAN);

  return stream->priv->byte_order;
}

/**
 * g_data_input_stream_set_newline_type:
 * @stream: a #GDataInputStream.
 * @type: the type of new line return as #GDataStreamNewlineType.
 * 
 * Sets the newline type for the @stream.
 * 
 * Note that using G_DATA_STREAM_NEWLINE_TYPE_ANY is slightly unsafe. If a read
 * chunk ends in "CR" we must read an additional byte to know if this is "CR" or
 * "CR LF", and this might block if there is no more data availible.
 *  
 **/
void
g_data_input_stream_set_newline_type (GDataInputStream       *stream,
				      GDataStreamNewlineType  type)
{
  GDataInputStreamPrivate *priv;

  g_return_if_fail (G_IS_DATA_INPUT_STREAM (stream));

  priv = stream->priv;
  
  if (priv->newline_type != type)
    {
      priv->newline_type = type;

      g_object_notify (G_OBJECT (stream), "newline-type");
    }
}

/**
 * g_data_input_stream_get_newline_type:
 * @stream: a given #GDataInputStream.
 * 
 * Gets the current newline type for the @stream.
 * 
 * Returns: #GDataStreamNewlineType for the given @stream.
 **/
GDataStreamNewlineType
g_data_input_stream_get_newline_type (GDataInputStream *stream)
{
  g_return_val_if_fail (G_IS_DATA_INPUT_STREAM (stream), G_DATA_STREAM_NEWLINE_TYPE_ANY);

  return stream->priv->newline_type;
}

static gboolean
read_data (GDataInputStream  *stream,
           void              *buffer,
           gsize              size,
           GCancellable      *cancellable,
           GError           **error)
{
  gsize available;
  gssize res;

  while ((available = g_buffered_input_stream_get_available (G_BUFFERED_INPUT_STREAM (stream))) < size)
    {
      res = g_buffered_input_stream_fill (G_BUFFERED_INPUT_STREAM (stream),
					  size - available,
					  cancellable, error);
      if (res < 0)
	return FALSE;
      if (res == 0)
	{
	  g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                               _("Unexpected early end-of-stream"));
	  return FALSE;
	}
    }
  
  /* This should always succeed, since it's in the buffer */
  res = g_input_stream_read (G_INPUT_STREAM (stream),
			     buffer, size,
			     NULL, NULL);
  g_warn_if_fail (res == size);
  return TRUE;
}


/**
 * g_data_input_stream_read_byte:
 * @stream: a given #GDataInputStream.
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * @error: #GError for error reporting.
 * 
 * Reads an unsigned 8-bit/1-byte value from @stream.
 *
 * Returns: an unsigned 8-bit/1-byte value read from the @stream or %0 
 * if an error occurred.
 **/
guchar
g_data_input_stream_read_byte (GDataInputStream  *stream,
			       GCancellable       *cancellable,
			       GError            **error)
{
  guchar c;
  
  g_return_val_if_fail (G_IS_DATA_INPUT_STREAM (stream), '\0');
  
  if (read_data (stream, &c, 1, cancellable, error))
      return c;
  
  return 0;
}


/**
 * g_data_input_stream_read_int16:
 * @stream: a given #GDataInputStream.
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * @error: #GError for error reporting.
 * 
 * Reads a 16-bit/2-byte value from @stream.
 *
 * In order to get the correct byte order for this read operation, 
 * see g_data_stream_get_byte_order() and g_data_stream_set_byte_order().
 * 
 * Returns: a signed 16-bit/2-byte value read from @stream or %0 if 
 * an error occurred.
 **/
gint16
g_data_input_stream_read_int16 (GDataInputStream  *stream,
			       GCancellable       *cancellable,
			       GError            **error)
{
  gint16 v;
  
  g_return_val_if_fail (G_IS_DATA_INPUT_STREAM (stream), 0);
  
  if (read_data (stream, &v, 2, cancellable, error))
    {
      switch (stream->priv->byte_order)
	{
	case G_DATA_STREAM_BYTE_ORDER_BIG_ENDIAN:
	  v = GINT16_FROM_BE (v);
	  break;
	case G_DATA_STREAM_BYTE_ORDER_LITTLE_ENDIAN:
	  v = GINT16_FROM_LE (v);
	  break;
	case G_DATA_STREAM_BYTE_ORDER_HOST_ENDIAN:
	default:
	  break;
	}
      return v;
    }
  
  return 0;
}


/**
 * g_data_input_stream_read_uint16:
 * @stream: a given #GDataInputStream.
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * @error: #GError for error reporting.
 *
 * Reads an unsigned 16-bit/2-byte value from @stream.
 *
 * In order to get the correct byte order for this read operation, 
 * see g_data_stream_get_byte_order() and g_data_stream_set_byte_order(). 
 * 
 * Returns: an unsigned 16-bit/2-byte value read from the @stream or %0 if 
 * an error occurred. 
 **/
guint16
g_data_input_stream_read_uint16 (GDataInputStream  *stream,
				 GCancellable       *cancellable,
				 GError            **error)
{
  guint16 v;
  
  g_return_val_if_fail (G_IS_DATA_INPUT_STREAM (stream), 0);
  
  if (read_data (stream, &v, 2, cancellable, error))
    {
      switch (stream->priv->byte_order)
	{
	case G_DATA_STREAM_BYTE_ORDER_BIG_ENDIAN:
	  v = GUINT16_FROM_BE (v);
	  break;
	case G_DATA_STREAM_BYTE_ORDER_LITTLE_ENDIAN:
	  v = GUINT16_FROM_LE (v);
	  break;
	case G_DATA_STREAM_BYTE_ORDER_HOST_ENDIAN:
	default:
	  break;
	}
      return v;
    }
  
  return 0;
}


/**
 * g_data_input_stream_read_int32:
 * @stream: a given #GDataInputStream.
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * @error: #GError for error reporting.
 * 
 * Reads a signed 32-bit/4-byte value from @stream.
 *
 * In order to get the correct byte order for this read operation, 
 * see g_data_stream_get_byte_order() and g_data_stream_set_byte_order().
 *
 * If @cancellable is not %NULL, then the operation can be cancelled by
 * triggering the cancellable object from another thread. If the operation
 * was cancelled, the error %G_IO_ERROR_CANCELLED will be returned. 
 *   
 * Returns: a signed 32-bit/4-byte value read from the @stream or %0 if 
 * an error occurred. 
 **/
gint32
g_data_input_stream_read_int32 (GDataInputStream  *stream,
				GCancellable       *cancellable,
				GError            **error)
{
  gint32 v;
  
  g_return_val_if_fail (G_IS_DATA_INPUT_STREAM (stream), 0);
  
  if (read_data (stream, &v, 4, cancellable, error))
    {
      switch (stream->priv->byte_order)
	{
	case G_DATA_STREAM_BYTE_ORDER_BIG_ENDIAN:
	  v = GINT32_FROM_BE (v);
	  break;
	case G_DATA_STREAM_BYTE_ORDER_LITTLE_ENDIAN:
	  v = GINT32_FROM_LE (v);
	  break;
	case G_DATA_STREAM_BYTE_ORDER_HOST_ENDIAN:
	default:
	  break;
	}
      return v;
    }
  
  return 0;
}


/**
 * g_data_input_stream_read_uint32:
 * @stream: a given #GDataInputStream.
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * @error: #GError for error reporting.
 * 
 * Reads an unsigned 32-bit/4-byte value from @stream.
 *
 * In order to get the correct byte order for this read operation, 
 * see g_data_stream_get_byte_order() and g_data_stream_set_byte_order().
 *
 * If @cancellable is not %NULL, then the operation can be cancelled by
 * triggering the cancellable object from another thread. If the operation
 * was cancelled, the error %G_IO_ERROR_CANCELLED will be returned. 
 * 
 * Returns: an unsigned 32-bit/4-byte value read from the @stream or %0 if 
 * an error occurred. 
 **/
guint32
g_data_input_stream_read_uint32 (GDataInputStream  *stream,
				 GCancellable       *cancellable,
				 GError            **error)
{
  guint32 v;
  
  g_return_val_if_fail (G_IS_DATA_INPUT_STREAM (stream), 0);
  
  if (read_data (stream, &v, 4, cancellable, error))
    {
      switch (stream->priv->byte_order)
	{
	case G_DATA_STREAM_BYTE_ORDER_BIG_ENDIAN:
	  v = GUINT32_FROM_BE (v);
	  break;
	case G_DATA_STREAM_BYTE_ORDER_LITTLE_ENDIAN:
	  v = GUINT32_FROM_LE (v);
	  break;
	case G_DATA_STREAM_BYTE_ORDER_HOST_ENDIAN:
	default:
	  break;
	}
      return v;
    }
  
  return 0;
}


/**
 * g_data_input_stream_read_int64:
 * @stream: a given #GDataInputStream.
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * @error: #GError for error reporting.
 * 
 * Reads a 64-bit/8-byte value from @stream.
 *
 * In order to get the correct byte order for this read operation, 
 * see g_data_stream_get_byte_order() and g_data_stream_set_byte_order().
 *
 * If @cancellable is not %NULL, then the operation can be cancelled by
 * triggering the cancellable object from another thread. If the operation
 * was cancelled, the error %G_IO_ERROR_CANCELLED will be returned. 
 * 
 * Returns: a signed 64-bit/8-byte value read from @stream or %0 if 
 * an error occurred.  
 **/
gint64
g_data_input_stream_read_int64 (GDataInputStream  *stream,
			       GCancellable       *cancellable,
			       GError            **error)
{
  gint64 v;
  
  g_return_val_if_fail (G_IS_DATA_INPUT_STREAM (stream), 0);
  
  if (read_data (stream, &v, 8, cancellable, error))
    {
      switch (stream->priv->byte_order)
	{
	case G_DATA_STREAM_BYTE_ORDER_BIG_ENDIAN:
	  v = GINT64_FROM_BE (v);
	  break;
	case G_DATA_STREAM_BYTE_ORDER_LITTLE_ENDIAN:
	  v = GINT64_FROM_LE (v);
	  break;
	case G_DATA_STREAM_BYTE_ORDER_HOST_ENDIAN:
	default:
	  break;
	}
      return v;
    }
  
  return 0;
}


/**
 * g_data_input_stream_read_uint64:
 * @stream: a given #GDataInputStream.
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * @error: #GError for error reporting.
 * 
 * Reads an unsigned 64-bit/8-byte value from @stream.
 *
 * In order to get the correct byte order for this read operation, 
 * see g_data_stream_get_byte_order().
 *
 * If @cancellable is not %NULL, then the operation can be cancelled by
 * triggering the cancellable object from another thread. If the operation
 * was cancelled, the error %G_IO_ERROR_CANCELLED will be returned. 
 * 
 * Returns: an unsigned 64-bit/8-byte read from @stream or %0 if 
 * an error occurred. 
 **/
guint64
g_data_input_stream_read_uint64 (GDataInputStream  *stream,
				GCancellable       *cancellable,
				GError            **error)
{
  guint64 v;
  
  g_return_val_if_fail (G_IS_DATA_INPUT_STREAM (stream), 0);
  
  if (read_data (stream, &v, 8, cancellable, error))
    {
      switch (stream->priv->byte_order)
	{
	case G_DATA_STREAM_BYTE_ORDER_BIG_ENDIAN:
	  v = GUINT64_FROM_BE (v);
	  break;
	case G_DATA_STREAM_BYTE_ORDER_LITTLE_ENDIAN:
	  v = GUINT64_FROM_LE (v);
	  break;
	case G_DATA_STREAM_BYTE_ORDER_HOST_ENDIAN:
	default:
	  break;
	}
      return v;
    }
  
  return 0;
}

static gssize
scan_for_newline (GDataInputStream *stream,
		  gsize            *checked_out,
		  gboolean         *last_saw_cr_out,
		  int              *newline_len_out)
{
  GBufferedInputStream *bstream;
  GDataInputStreamPrivate *priv;
  const char *buffer;
  gsize start, end, peeked;
  int i;
  gssize found_pos;
  int newline_len;
  gsize available, checked;
  gboolean last_saw_cr;

  priv = stream->priv;
  
  bstream = G_BUFFERED_INPUT_STREAM (stream);

  checked = *checked_out;
  last_saw_cr = *last_saw_cr_out;
  found_pos = -1;
  newline_len = 0;
  
  start = checked;
  buffer = (const char*)g_buffered_input_stream_peek_buffer (bstream, &available) + start;
  end = available;
  peeked = end - start;

  for (i = 0; checked < available && i < peeked; i++)
    {
      switch (priv->newline_type)
	{
	case G_DATA_STREAM_NEWLINE_TYPE_LF:
	  if (buffer[i] == 10)
	    {
	      found_pos = start + i;
	      newline_len = 1;
	    }
	  break;
	case G_DATA_STREAM_NEWLINE_TYPE_CR:
	  if (buffer[i] == 13)
	    {
	      found_pos = start + i;
	      newline_len = 1;
	    }
	  break;
	case G_DATA_STREAM_NEWLINE_TYPE_CR_LF:
	  if (last_saw_cr && buffer[i] == 10)
	    {
	      found_pos = start + i - 1;
	      newline_len = 2;
	    }
	  break;
	default:
	case G_DATA_STREAM_NEWLINE_TYPE_ANY:
	  if (buffer[i] == 10) /* LF */
	    {
	      if (last_saw_cr)
		{
		  /* CR LF */
		  found_pos = start + i - 1;
		  newline_len = 2;
		}
	      else
		{
		  /* LF */
		  found_pos = start + i;
		  newline_len = 1;
		}
	    }
	  else if (last_saw_cr)
	    {
	      /* Last was cr, this is not LF, end is CR */
	      found_pos = start + i - 1;
	      newline_len = 1;
	    }
	  /* Don't check for CR here, instead look at last_saw_cr on next byte */
	  break;
	}
	
      last_saw_cr = (buffer[i] == 13);

      if (found_pos != -1)
	{
	  *newline_len_out = newline_len;
	  return found_pos;
	}
    }

  checked = end;

  *checked_out = checked;
  *last_saw_cr_out = last_saw_cr;
  return -1;
}
		  

/**
 * g_data_input_stream_read_line:
 * @stream: a given #GDataInputStream.
 * @length: a #gsize to get the length of the data read in.
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * @error: #GError for error reporting.
 *
 * Reads a line from the data input stream.
 *
 * If @cancellable is not %NULL, then the operation can be cancelled by
 * triggering the cancellable object from another thread. If the operation
 * was cancelled, the error %G_IO_ERROR_CANCELLED will be returned.
 *
 * Returns: a string with the line that was read in (without the newlines).
 *     Set @length to a #gsize to get the length of the read line.
 *     On an error, it will return %NULL and @error will be set. If there's no
 *     content to read, it will still return %NULL, but @error won't be set.
 **/
char *
g_data_input_stream_read_line (GDataInputStream  *stream,
			       gsize             *length,
			       GCancellable      *cancellable,
			       GError           **error)
{
  GBufferedInputStream *bstream;
  gsize checked;
  gboolean last_saw_cr;
  gssize found_pos;
  gssize res;
  int newline_len;
  char *line;
  
  g_return_val_if_fail (G_IS_DATA_INPUT_STREAM (stream), NULL);  

  bstream = G_BUFFERED_INPUT_STREAM (stream);

  newline_len = 0;
  checked = 0;
  last_saw_cr = FALSE;

  while ((found_pos = scan_for_newline (stream, &checked, &last_saw_cr, &newline_len)) == -1)
    {
      if (g_buffered_input_stream_get_available (bstream) ==
	  g_buffered_input_stream_get_buffer_size (bstream))
	g_buffered_input_stream_set_buffer_size (bstream,
						 2 * g_buffered_input_stream_get_buffer_size (bstream));

      res = g_buffered_input_stream_fill (bstream, -1, cancellable, error);
      if (res < 0)
	return NULL;
      if (res == 0)
	{
	  /* End of stream */
	  if (g_buffered_input_stream_get_available (bstream) == 0)
	    {
	      if (length)
		*length = 0;
	      return NULL;
	    }
	  else
	    {
	      found_pos = checked;
	      newline_len = 0;
	      break;
	    }
	}
    }

  line = g_malloc (found_pos + newline_len + 1);

  res = g_input_stream_read (G_INPUT_STREAM (stream),
			     line,
			     found_pos + newline_len,
			     NULL, NULL);
  if (length)
    *length = (gsize)found_pos;
  g_warn_if_fail (res == found_pos + newline_len);
  line[found_pos] = 0;
  
  return line;
}

static gssize
scan_for_chars (GDataInputStream *stream,
		gsize            *checked_out,
		const char       *stop_chars)
{
  GBufferedInputStream *bstream;
  const char *buffer;
  gsize start, end, peeked;
  int i;
  gsize available, checked;
  const char *stop_char;

  bstream = G_BUFFERED_INPUT_STREAM (stream);

  checked = *checked_out;

  start = checked;
  buffer = (const char *)g_buffered_input_stream_peek_buffer (bstream, &available) + start;
  end = available;
  peeked = end - start;

  for (i = 0; checked < available && i < peeked; i++)
    {
      for (stop_char = stop_chars; *stop_char != '\0'; stop_char++)
	{
	  if (buffer[i] == *stop_char)
	    return (start + i);
	}
    }

  checked = end;

  *checked_out = checked;
  return -1;
}

/**
 * g_data_input_stream_read_until:
 * @stream: a given #GDataInputStream.
 * @stop_chars: characters to terminate the read.
 * @length: a #gsize to get the length of the data read in.
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * @error: #GError for error reporting.
 *
 * Reads a string from the data input stream, up to the first
 * occurrence of any of the stop characters.
 *
 * Note that, in contrast to g_data_input_stream_read_until_async(),
 * this function consumes the stop character that it finds.
 *
 * Returns: a string with the data that was read before encountering
 *     any of the stop characters. Set @length to a #gsize to get the length
 *     of the string. This function will return %NULL on an error.
 */
char *
g_data_input_stream_read_until (GDataInputStream  *stream,
			       const gchar        *stop_chars,
			       gsize              *length,
			       GCancellable       *cancellable,
			       GError            **error)
{
  GBufferedInputStream *bstream;
  gsize checked;
  gssize found_pos;
  gssize res;
  int stop_char_len;
  char *data_until;
  
  g_return_val_if_fail (G_IS_DATA_INPUT_STREAM (stream), NULL);  

  bstream = G_BUFFERED_INPUT_STREAM (stream);

  stop_char_len = 1;
  checked = 0;

  while ((found_pos = scan_for_chars (stream, &checked, stop_chars)) == -1)
    {
      if (g_buffered_input_stream_get_available (bstream) ==
	  g_buffered_input_stream_get_buffer_size (bstream))
	g_buffered_input_stream_set_buffer_size (bstream,
						 2 * g_buffered_input_stream_get_buffer_size (bstream));

      res = g_buffered_input_stream_fill (bstream, -1, cancellable, error);
      if (res < 0)
	return NULL;
      if (res == 0)
	{
	  /* End of stream */
	  if (g_buffered_input_stream_get_available (bstream) == 0)
	    {
	      if (length)
		*length = 0;
	      return NULL;
	    }
	  else
	    {
	      found_pos = checked;
	      stop_char_len = 0;
	      break;
	    }
	}
    }

  data_until = g_malloc (found_pos + stop_char_len + 1);

  res = g_input_stream_read (G_INPUT_STREAM (stream),
			     data_until,
			     found_pos + stop_char_len,
			     NULL, NULL);
  if (length)
    *length = (gsize)found_pos;
  g_warn_if_fail (res == found_pos + stop_char_len);
  data_until[found_pos] = 0;
  
  return data_until;
}

typedef struct
{
  GDataInputStream *stream;
  GSimpleAsyncResult *simple;
  gboolean last_saw_cr;
  gsize checked;
  gint io_priority;
  GCancellable *cancellable;

  gchar *stop_chars;
  gchar *line;
  gsize length;
} GDataInputStreamReadData;

static void
g_data_input_stream_read_complete (GDataInputStreamReadData *data,
                                   gsize                     read_length,
                                   gsize                     skip_length,
                                   gboolean                  need_idle_dispatch)
{
  if (read_length || skip_length)
    {
      gssize bytes;

      data->length = read_length;
      data->line = g_malloc (read_length + 1);
      data->line[read_length] = '\0';

      /* we already checked the buffer.  this shouldn't fail. */
      bytes = g_input_stream_read (G_INPUT_STREAM (data->stream),
                                   data->line, read_length, NULL, NULL);
      g_assert_cmpint (bytes, ==, read_length);

      bytes = g_input_stream_skip (G_INPUT_STREAM (data->stream),
                                   skip_length, NULL, NULL);
      g_assert_cmpint (bytes, ==, skip_length);
    }

  if (need_idle_dispatch)
    g_simple_async_result_complete_in_idle (data->simple);
  else
    g_simple_async_result_complete (data->simple);

  g_object_unref (data->simple);
}

static void
g_data_input_stream_read_line_ready (GObject      *object,
                                     GAsyncResult *result,
                                     gpointer      user_data)
{
  GDataInputStreamReadData *data = user_data;
  gssize found_pos;
  gint newline_len;

  if (result)
    /* this is a callback.  finish the async call. */
    {
      GBufferedInputStream *buffer = G_BUFFERED_INPUT_STREAM (data->stream);
      GError *error = NULL;
      gssize bytes;

      bytes = g_buffered_input_stream_fill_finish (buffer, result, &error);

      if (bytes <= 0)
        {
          if (bytes < 0)
            /* stream error. */
            {
              g_simple_async_result_set_from_error (data->simple, error);
              g_error_free (error);
              data->checked = 0;
            }

          g_data_input_stream_read_complete (data, data->checked, 0, FALSE);
          return;
        }

      /* only proceed if we got more bytes... */
    }

  if (data->stop_chars)
    {
      found_pos = scan_for_chars (data->stream,
                                  &data->checked,
                                  data->stop_chars);
      newline_len = 0;
    }
  else
    found_pos = scan_for_newline (data->stream, &data->checked,
                                  &data->last_saw_cr, &newline_len);

  if (found_pos == -1)
    /* didn't find a full line; need to buffer some more bytes */
    {
      GBufferedInputStream *buffer = G_BUFFERED_INPUT_STREAM (data->stream);
      gsize size;

      size = g_buffered_input_stream_get_buffer_size (buffer);

      if (g_buffered_input_stream_get_available (buffer) == size)
        /* need to grow the buffer */
        g_buffered_input_stream_set_buffer_size (buffer, size * 2);

      /* try again */
      g_buffered_input_stream_fill_async (buffer, -1, data->io_priority,
                                          data->cancellable,
                                          g_data_input_stream_read_line_ready,
                                          user_data);
    }
  else
    {
      /* read the line and the EOL.  no error is possible. */
      g_data_input_stream_read_complete (data, found_pos,
                                         newline_len, result == NULL);
    }
}

static void
g_data_input_stream_read_data_free (gpointer user_data)
{
  GDataInputStreamReadData *data = user_data;

  /* we don't hold a ref to ->simple because it keeps a ref to us.
   * we are called because it is being finalized.
   */

  g_free (data->stop_chars);
  if (data->cancellable)
    g_object_unref (data->cancellable);
  g_free (data->line);
  g_slice_free (GDataInputStreamReadData, data);
}

static void
g_data_input_stream_read_async (GDataInputStream    *stream,
                                const gchar         *stop_chars,
                                gint                 io_priority,
                                GCancellable        *cancellable,
                                GAsyncReadyCallback  callback,
                                gpointer             user_data,
                                gpointer             source_tag)
{
  GDataInputStreamReadData *data;

  data = g_slice_new (GDataInputStreamReadData);
  data->stream = stream;
  if (cancellable)
    g_object_ref (cancellable);
  data->cancellable = cancellable;
  data->stop_chars = g_strdup (stop_chars);
  data->io_priority = io_priority;
  data->last_saw_cr = FALSE;
  data->checked = 0;
  data->line = NULL;

  data->simple = g_simple_async_result_new (G_OBJECT (stream), callback,
                                            user_data, source_tag);
  g_simple_async_result_set_op_res_gpointer (data->simple, data,
                                             g_data_input_stream_read_data_free);
  g_data_input_stream_read_line_ready (NULL, NULL, data);
}

static gchar *
g_data_input_stream_read_finish (GDataInputStream  *stream,
                                 GAsyncResult      *result,
                                 gsize             *length,
                                 GError           **error)
{
  GDataInputStreamReadData *data;
  GSimpleAsyncResult *simple;
  gchar *line;

  simple = G_SIMPLE_ASYNC_RESULT (result);

  if (g_simple_async_result_propagate_error (simple, error))
    return NULL;

  data = g_simple_async_result_get_op_res_gpointer (simple);

  line = data->line;
  data->line = NULL;

  if (length && line)
    *length = data->length;

  return line;
}

/**
 * g_data_input_stream_read_line_async:
 * @stream: a given #GDataInputStream.
 * @io_priority: the <link linkend="io-priority">I/O priority</link>
 *     of the request.
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * @callback: callback to call when the request is satisfied.
 * @user_data: the data to pass to callback function.
 *
 * The asynchronous version of g_data_input_stream_read_line().  It is
 * an error to have two outstanding calls to this function.
 *
 * When the operation is finished, @callback will be called. You
 * can then call g_data_input_stream_read_line_finish() to get
 * the result of the operation.
 *
 * Since: 2.20
 */
void
g_data_input_stream_read_line_async (GDataInputStream    *stream,
                                     gint                 io_priority,
                                     GCancellable        *cancellable,
                                     GAsyncReadyCallback  callback,
                                     gpointer             user_data)
{
  g_return_if_fail (G_IS_DATA_INPUT_STREAM (stream));
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  g_data_input_stream_read_async (stream, NULL, io_priority,
                                  cancellable, callback, user_data,
                                  g_data_input_stream_read_line_async);
}

/**
 * g_data_input_stream_read_until_async:
 * @stream: a given #GDataInputStream.
 * @stop_chars: characters to terminate the read.
 * @io_priority: the <link linkend="io-priority">I/O priority</link>
 *     of the request.
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * @callback: callback to call when the request is satisfied.
 * @user_data: the data to pass to callback function.
 *
 * The asynchronous version of g_data_input_stream_read_until().
 * It is an error to have two outstanding calls to this function.
 *
 * Note that, in contrast to g_data_input_stream_read_until(),
 * this function does not consume the stop character that it finds.  You
 * must read it for yourself.
 *
 * When the operation is finished, @callback will be called. You
 * can then call g_data_input_stream_read_until_finish() to get
 * the result of the operation.
 *
 * Since: 2.20
 */
void
g_data_input_stream_read_until_async (GDataInputStream    *stream,
                                      const gchar         *stop_chars,
                                      gint                 io_priority,
                                      GCancellable        *cancellable,
                                      GAsyncReadyCallback  callback,
                                      gpointer             user_data)
{
  g_return_if_fail (G_IS_DATA_INPUT_STREAM (stream));
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
  g_return_if_fail (stop_chars != NULL);

  g_data_input_stream_read_async (stream, stop_chars, io_priority,
                                  cancellable, callback, user_data,
                                  g_data_input_stream_read_until_async);
}

/**
 * g_data_input_stream_read_line_finish:
 * @stream: a given #GDataInputStream.
 * @result: the #GAsyncResult that was provided to the callback.
 * @length: a #gsize to get the length of the data read in.
 * @error: #GError for error reporting.
 *
 * Finish an asynchronous call started by
 * g_data_input_stream_read_line_async().
 *
 * Returns: a string with the line that was read in (without the newlines).
 *     Set @length to a #gsize to get the length of the read line.
 *     On an error, it will return %NULL and @error will be set. If there's no
 *     content to read, it will still return %NULL, but @error won't be set.
 *
 * Since: 2.20
 */
gchar *
g_data_input_stream_read_line_finish (GDataInputStream  *stream,
                                      GAsyncResult      *result,
                                      gsize             *length,
                                      GError           **error)
{
  g_return_val_if_fail (
    g_simple_async_result_is_valid (result, G_OBJECT (stream),
      g_data_input_stream_read_line_async), NULL);

  return g_data_input_stream_read_finish (stream, result, length, error);
}

/**
 * g_data_input_stream_read_until_finish:
 * @stream: a given #GDataInputStream.
 * @result: the #GAsyncResult that was provided to the callback.
 * @length: a #gsize to get the length of the data read in.
 * @error: #GError for error reporting.
 *
 * Finish an asynchronous call started by
 * g_data_input_stream_read_until_async().
 *
 * Since: 2.20
 *
 * Returns: a string with the data that was read before encountering
 *     any of the stop characters. Set @length to a #gsize to get the length
 *     of the string. This function will return %NULL on an error.
 */
gchar *
g_data_input_stream_read_until_finish (GDataInputStream  *stream,
                                       GAsyncResult      *result,
                                       gsize             *length,
                                       GError           **error)
{
  g_return_val_if_fail (
    g_simple_async_result_is_valid (result, G_OBJECT (stream),
      g_data_input_stream_read_until_async), NULL);

  return g_data_input_stream_read_finish (stream, result, length, error);
}


#define __G_DATA_INPUT_STREAM_C__
#include "gioaliasdef.c"
