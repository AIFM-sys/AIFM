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

#ifndef __G_ASYNC_HELPER_H__
#define __G_ASYNC_HELPER_H__

#include <gio/gio.h>
#include <glib-object.h>

G_BEGIN_DECLS

typedef struct
{
  gpointer       async_object;
  GError *       error;
  gpointer       user_data;
} GAsyncResultData;

typedef gboolean (*GFDSourceFunc) (gpointer     user_data,
				   GIOCondition condition,
				   int          fd);
typedef gboolean (*GFDSourceObjectFunc) (GObject *object,
					 GIOCondition condition,
					 gpointer     user_data);

void     _g_queue_async_result (GAsyncResultData *result,
				gpointer         async_object,
				GError          *error,
				gpointer         user_data,
				GSourceFunc      source_func);

GSource *_g_fd_source_new_with_object (GObject      *object,
				       int           fd,
				       gushort       events,
				       GCancellable *cancellable);
GSource *_g_fd_source_new             (int           fd,
				       gushort       events,
				       GCancellable *cancellable);

G_END_DECLS

#endif /* __G_ASYNC_HELPER_H__ */
