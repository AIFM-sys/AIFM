/* GIO - GLib Input, Output and Streaming Library
 * 
 * Copyright (C) 2006-2007 Red Hat, Inc.
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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <errno.h>

#include <glib.h>
#include <glib/gstdio.h>
#include "gcancellable.h"
#include "gfiledescriptorbased.h"
#include "gioerror.h"
#include "glocalfileinputstream.h"
#include "glocalfileinfo.h"
#include "glibintl.h"

#ifdef G_OS_WIN32
#include <io.h>
#endif

#include "gioalias.h"


static void       g_file_descriptor_based_iface_init   (GFileDescriptorBasedIface *iface);
#define g_local_file_input_stream_get_type _g_local_file_input_stream_get_type
G_DEFINE_TYPE_WITH_CODE (GLocalFileInputStream, g_local_file_input_stream, G_TYPE_FILE_INPUT_STREAM,
			 G_IMPLEMENT_INTERFACE (G_TYPE_FILE_DESCRIPTOR_BASED,
						g_file_descriptor_based_iface_init));

struct _GLocalFileInputStreamPrivate {
  int fd;
  guint do_close : 1;
};

static gssize     g_local_file_input_stream_read       (GInputStream      *stream,
							void              *buffer,
							gsize              count,
							GCancellable      *cancellable,
							GError           **error);
static gssize     g_local_file_input_stream_skip       (GInputStream      *stream,
							gsize              count,
							GCancellable      *cancellable,
							GError           **error);
static gboolean   g_local_file_input_stream_close      (GInputStream      *stream,
							GCancellable      *cancellable,
							GError           **error);
static goffset    g_local_file_input_stream_tell       (GFileInputStream  *stream);
static gboolean   g_local_file_input_stream_can_seek   (GFileInputStream  *stream);
static gboolean   g_local_file_input_stream_seek       (GFileInputStream  *stream,
							goffset            offset,
							GSeekType          type,
							GCancellable      *cancellable,
							GError           **error);
static GFileInfo *g_local_file_input_stream_query_info (GFileInputStream  *stream,
							const char        *attributes,
							GCancellable      *cancellable,
							GError           **error);
static int        g_local_file_input_stream_get_fd     (GFileDescriptorBased *stream);

static void
g_local_file_input_stream_finalize (GObject *object)
{
  G_OBJECT_CLASS (g_local_file_input_stream_parent_class)->finalize (object);
}

void
_g_local_file_input_stream_set_do_close (GLocalFileInputStream *in,
					  gboolean do_close)
{
  in->priv->do_close = do_close;
}

static void
g_local_file_input_stream_class_init (GLocalFileInputStreamClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GInputStreamClass *stream_class = G_INPUT_STREAM_CLASS (klass);
  GFileInputStreamClass *file_stream_class = G_FILE_INPUT_STREAM_CLASS (klass);
  
  g_type_class_add_private (klass, sizeof (GLocalFileInputStreamPrivate));
  
  gobject_class->finalize = g_local_file_input_stream_finalize;

  stream_class->read_fn = g_local_file_input_stream_read;
  stream_class->skip = g_local_file_input_stream_skip;
  stream_class->close_fn = g_local_file_input_stream_close;
  file_stream_class->tell = g_local_file_input_stream_tell;
  file_stream_class->can_seek = g_local_file_input_stream_can_seek;
  file_stream_class->seek = g_local_file_input_stream_seek;
  file_stream_class->query_info = g_local_file_input_stream_query_info;
}

static void
g_file_descriptor_based_iface_init (GFileDescriptorBasedIface *iface)
{
  iface->get_fd = g_local_file_input_stream_get_fd;
}

static void
g_local_file_input_stream_init (GLocalFileInputStream *info)
{
  info->priv = G_TYPE_INSTANCE_GET_PRIVATE (info,
					    G_TYPE_LOCAL_FILE_INPUT_STREAM,
					    GLocalFileInputStreamPrivate);
  info->priv->do_close = TRUE;
}

GFileInputStream *
_g_local_file_input_stream_new (int fd)
{
  GLocalFileInputStream *stream;

  stream = g_object_new (G_TYPE_LOCAL_FILE_INPUT_STREAM, NULL);
  stream->priv->fd = fd;
  
  return G_FILE_INPUT_STREAM (stream);
}

static gssize
g_local_file_input_stream_read (GInputStream  *stream,
				void          *buffer,
				gsize          count,
				GCancellable  *cancellable,
				GError       **error)
{
  GLocalFileInputStream *file;
  gssize res;

  file = G_LOCAL_FILE_INPUT_STREAM (stream);

  res = -1;
  while (1)
    {
      if (g_cancellable_set_error_if_cancelled (cancellable, error))
	break;
      res = read (file->priv->fd, buffer, count);
      if (res == -1)
	{
          int errsv = errno;

	  if (errsv == EINTR)
	    continue;
	  
	  g_set_error (error, G_IO_ERROR,
		       g_io_error_from_errno (errsv),
		       _("Error reading from file: %s"),
		       g_strerror (errsv));
	}
      
      break;
    }
  
  return res;
}

static gssize
g_local_file_input_stream_skip (GInputStream  *stream,
				gsize          count,
				GCancellable  *cancellable,
				GError       **error)
{
  off_t res, start;
  GLocalFileInputStream *file;

  file = G_LOCAL_FILE_INPUT_STREAM (stream);
  
  if (g_cancellable_set_error_if_cancelled (cancellable, error))
    return -1;
  
  start = lseek (file->priv->fd, 0, SEEK_CUR);
  if (start == -1)
    {
      int errsv = errno;

      g_set_error (error, G_IO_ERROR,
		   g_io_error_from_errno (errsv),
		   _("Error seeking in file: %s"),
		   g_strerror (errsv));
      return -1;
    }
  
  res = lseek (file->priv->fd, count, SEEK_CUR);
  if (res == -1)
    {
      int errsv = errno;

      g_set_error (error, G_IO_ERROR,
		   g_io_error_from_errno (errsv),
		   _("Error seeking in file: %s"),
		   g_strerror (errsv));
      return -1;
    }

  return res - start;
}

static gboolean
g_local_file_input_stream_close (GInputStream  *stream,
				 GCancellable  *cancellable,
				 GError       **error)
{
  GLocalFileInputStream *file;
  int res;

  file = G_LOCAL_FILE_INPUT_STREAM (stream);

  if (!file->priv->do_close)
    return TRUE;

  if (file->priv->fd == -1)
    return TRUE;

  while (1)
    {
      res = close (file->priv->fd);
      if (res == -1)
        {
          int errsv = errno;

          g_set_error (error, G_IO_ERROR,
                       g_io_error_from_errno (errsv),
                       _("Error closing file: %s"),
                       g_strerror (errsv));
        }
      break;
    }

  return res != -1;
}


static goffset
g_local_file_input_stream_tell (GFileInputStream *stream)
{
  GLocalFileInputStream *file;
  off_t pos;

  file = G_LOCAL_FILE_INPUT_STREAM (stream);
  
  pos = lseek (file->priv->fd, 0, SEEK_CUR);

  if (pos == (off_t)-1)
    return 0;
  
  return pos;
}

static gboolean
g_local_file_input_stream_can_seek (GFileInputStream *stream)
{
  GLocalFileInputStream *file;
  off_t pos;

  file = G_LOCAL_FILE_INPUT_STREAM (stream);
  
  pos = lseek (file->priv->fd, 0, SEEK_CUR);

  if (pos == (off_t)-1 && errno == ESPIPE)
    return FALSE;
  
  return TRUE;
}

static int
seek_type_to_lseek (GSeekType type)
{
  switch (type)
    {
    default:
    case G_SEEK_CUR:
      return SEEK_CUR;
      
    case G_SEEK_SET:
      return SEEK_SET;
      
    case G_SEEK_END:
      return SEEK_END;
    }
}

static gboolean
g_local_file_input_stream_seek (GFileInputStream  *stream,
				goffset            offset,
				GSeekType          type,
				GCancellable      *cancellable,
				GError           **error)
{
  GLocalFileInputStream *file;
  off_t pos;

  file = G_LOCAL_FILE_INPUT_STREAM (stream);

  pos = lseek (file->priv->fd, offset, seek_type_to_lseek (type));

  if (pos == (off_t)-1)
    {
      int errsv = errno;

      g_set_error (error, G_IO_ERROR,
		   g_io_error_from_errno (errsv),
		   _("Error seeking in file: %s"),
		   g_strerror (errsv));
      return FALSE;
    }
  
  return TRUE;
}

static GFileInfo *
g_local_file_input_stream_query_info (GFileInputStream  *stream,
				      const char        *attributes,
				      GCancellable      *cancellable,
				      GError           **error)
{
  GLocalFileInputStream *file;

  file = G_LOCAL_FILE_INPUT_STREAM (stream);

  if (g_cancellable_set_error_if_cancelled (cancellable, error))
    return NULL;
  
  return _g_local_file_info_get_from_fd (file->priv->fd,
					 attributes,
					 error);
}

static int
g_local_file_input_stream_get_fd (GFileDescriptorBased *fd_based)
{
  GLocalFileInputStream *stream = G_LOCAL_FILE_INPUT_STREAM (fd_based);
  return stream->priv->fd;
}

