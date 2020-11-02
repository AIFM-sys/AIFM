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

#include <string.h>

#include <glib.h>
#include "gnativevolumemonitor.h"

#include "gioalias.h"

G_DEFINE_ABSTRACT_TYPE (GNativeVolumeMonitor, g_native_volume_monitor, G_TYPE_VOLUME_MONITOR);

static void
g_native_volume_monitor_finalize (GObject *object)
{
  G_OBJECT_CLASS (g_native_volume_monitor_parent_class)->finalize (object);
}


static void
g_native_volume_monitor_class_init (GNativeVolumeMonitorClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  
  gobject_class->finalize = g_native_volume_monitor_finalize;
}


static void
g_native_volume_monitor_init (GNativeVolumeMonitor *native_monitor)
{
}

#define __G_NATIVE_VOLUME_MONITOR_C__
#include "gioaliasdef.c"
