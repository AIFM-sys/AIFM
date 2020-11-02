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

#ifndef __G_LOCAL_FILE_MONITOR_H__
#define __G_LOCAL_FILE_MONITOR_H__

#include <gio/gfilemonitor.h>

G_BEGIN_DECLS

#define G_TYPE_LOCAL_FILE_MONITOR		(g_local_file_monitor_get_type ())
#define G_LOCAL_FILE_MONITOR(o)			(G_TYPE_CHECK_INSTANCE_CAST ((o), G_TYPE_LOCAL_FILE_MONITOR, GLocalFileMonitor))
#define G_LOCAL_FILE_MONITOR_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST ((k), G_TYPE_LOCAL_FILE_MONITOR, GLocalFileMonitorClass))
#define G_IS_LOCAL_FILE_MONITOR(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), G_TYPE_LOCAL_FILE_MONITOR))
#define G_IS_LOCAL_FILE_MONITOR_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), G_TYPE_LOCAL_FILE_MONITOR))

#define G_LOCAL_FILE_MONITOR_EXTENSION_POINT_NAME "gio-local-file-monitor"

typedef struct _GLocalFileMonitor      GLocalFileMonitor;
typedef struct _GLocalFileMonitorClass GLocalFileMonitorClass;

struct _GLocalFileMonitor
{
  GFileMonitor parent_instance;

  gchar *filename;
  GFileMonitorFlags flags;
};

struct _GLocalFileMonitorClass
{
  GFileMonitorClass parent_class;

  gboolean (* is_supported) (void);
};

GType           g_local_file_monitor_get_type (void) G_GNUC_CONST;

GFileMonitor * _g_local_file_monitor_new      (const char         *pathname,
                                               GFileMonitorFlags   flags,
                                               GError            **error);

G_END_DECLS

#endif /* __G_LOCAL_FILE_MONITOR_H__ */
