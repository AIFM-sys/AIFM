/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

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
 *         David Zeuthen <davidz@redhat.com>
 */

#include "config.h"

#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include <glib.h>
#include "gunixvolumemonitor.h"
#include "gunixmount.h"
#include "gunixmounts.h"
#include "gunixvolume.h"
#include "gmountprivate.h"
#include "gmount.h"
#include "gfile.h"
#include "gvolumemonitor.h"
#include "gthemedicon.h"
#include "gsimpleasyncresult.h"
#include "gioerror.h"
#include "glibintl.h"
/* for BUFSIZ */
#include <stdio.h>

#include "gioalias.h"

struct _GUnixMount {
  GObject parent;

  GVolumeMonitor   *volume_monitor;

  GUnixVolume      *volume; /* owned by volume monitor */

  char *name;
  GIcon *icon;
  char *device_path;
  char *mount_path;

  gboolean can_eject;
};

static void g_unix_mount_mount_iface_init (GMountIface *iface);

#define g_unix_mount_get_type _g_unix_mount_get_type
G_DEFINE_TYPE_WITH_CODE (GUnixMount, g_unix_mount, G_TYPE_OBJECT,
			 G_IMPLEMENT_INTERFACE (G_TYPE_MOUNT,
						g_unix_mount_mount_iface_init))


static void
g_unix_mount_finalize (GObject *object)
{
  GUnixMount *mount;
  
  mount = G_UNIX_MOUNT (object);

  if (mount->volume_monitor != NULL)
    g_object_unref (mount->volume_monitor);

  if (mount->volume)
    _g_unix_volume_unset_mount (mount->volume, mount);
    
  /* TODO: g_warn_if_fail (volume->volume == NULL); */
  g_object_unref (mount->icon);
  g_free (mount->name);
  g_free (mount->device_path);
  g_free (mount->mount_path);

  G_OBJECT_CLASS (g_unix_mount_parent_class)->finalize (object);
}

static void
g_unix_mount_class_init (GUnixMountClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize = g_unix_mount_finalize;
}

static void
g_unix_mount_init (GUnixMount *unix_mount)
{
}

GUnixMount *
_g_unix_mount_new (GVolumeMonitor  *volume_monitor,
                   GUnixMountEntry *mount_entry,
                   GUnixVolume     *volume)
{
  GUnixMount *mount;
  
  /* No volume for mount: Ignore internal things */
  if (volume == NULL && !g_unix_mount_guess_should_display (mount_entry))
    return NULL;

  mount = g_object_new (G_TYPE_UNIX_MOUNT, NULL);
  mount->volume_monitor = volume_monitor != NULL ? g_object_ref (volume_monitor) : NULL;
  mount->device_path = g_strdup (g_unix_mount_get_device_path (mount_entry));
  mount->mount_path = g_strdup (g_unix_mount_get_mount_path (mount_entry));
  mount->can_eject = g_unix_mount_guess_can_eject (mount_entry);

  mount->name = g_unix_mount_guess_name (mount_entry);
  mount->icon = g_unix_mount_guess_icon (mount_entry);

  /* need to do this last */
  mount->volume = volume;
  if (volume != NULL)
    _g_unix_volume_set_mount (volume, mount);

  return mount;
}

void
_g_unix_mount_unmounted (GUnixMount *mount)
{
  if (mount->volume != NULL)
    {
      _g_unix_volume_unset_mount (mount->volume, mount);
      mount->volume = NULL;
      g_signal_emit_by_name (mount, "changed");
      /* there's really no need to emit mount_changed on the volume monitor 
       * as we're going to be deleted.. */
    }
}

void
_g_unix_mount_unset_volume (GUnixMount *mount,
                            GUnixVolume  *volume)
{
  if (mount->volume == volume)
    {
      mount->volume = NULL;
      /* TODO: Emit changed in idle to avoid locking issues */
      g_signal_emit_by_name (mount, "changed");
      if (mount->volume_monitor != NULL)
        g_signal_emit_by_name (mount->volume_monitor, "mount-changed", mount);
    }
}

static GFile *
g_unix_mount_get_root (GMount *mount)
{
  GUnixMount *unix_mount = G_UNIX_MOUNT (mount);

  return g_file_new_for_path (unix_mount->mount_path);
}

static GIcon *
g_unix_mount_get_icon (GMount *mount)
{
  GUnixMount *unix_mount = G_UNIX_MOUNT (mount);

  return g_object_ref (unix_mount->icon);
}

static char *
g_unix_mount_get_uuid (GMount *mount)
{
  return NULL;
}

static char *
g_unix_mount_get_name (GMount *mount)
{
  GUnixMount *unix_mount = G_UNIX_MOUNT (mount);
  
  return g_strdup (unix_mount->name);
}

gboolean
_g_unix_mount_has_mount_path (GUnixMount *mount,
                              const char  *mount_path)
{
  return strcmp (mount->mount_path, mount_path) == 0;
}

static GDrive *
g_unix_mount_get_drive (GMount *mount)
{
  GUnixMount *unix_mount = G_UNIX_MOUNT (mount);

  if (unix_mount->volume != NULL)
    return g_volume_get_drive (G_VOLUME (unix_mount->volume));

  return NULL;
}

static GVolume *
g_unix_mount_get_volume (GMount *mount)
{
  GUnixMount *unix_mount = G_UNIX_MOUNT (mount);

  if (unix_mount->volume)
    return G_VOLUME (g_object_ref (unix_mount->volume));
  
  return NULL;
}

static gboolean
g_unix_mount_can_unmount (GMount *mount)
{
  return TRUE;
}

static gboolean
g_unix_mount_can_eject (GMount *mount)
{
  GUnixMount *unix_mount = G_UNIX_MOUNT (mount);
  return unix_mount->can_eject;
}


typedef struct {
  GUnixMount *unix_mount;
  GAsyncReadyCallback callback;
  gpointer user_data;
  GCancellable *cancellable;
  int error_fd;
  GIOChannel *error_channel;
  GSource *error_channel_source;
  GString *error_string;
  gchar **argv;
} UnmountEjectOp;

static void 
eject_unmount_cb (GPid pid, gint status, gpointer user_data)
{
  UnmountEjectOp *data = user_data;
  GSimpleAsyncResult *simple;
  
  if (WEXITSTATUS (status) != 0)
    {
      GError *error;
      error = g_error_new_literal (G_IO_ERROR, 
                                   G_IO_ERROR_FAILED,
                                   data->error_string->str);
      simple = g_simple_async_result_new_from_error (G_OBJECT (data->unix_mount),
                                                     data->callback,
                                                     data->user_data,
                                                     error);
      g_error_free (error);
    }
  else
    {
      simple = g_simple_async_result_new (G_OBJECT (data->unix_mount),
                                          data->callback,
                                          data->user_data,
                                          NULL);
    }

  g_simple_async_result_complete (simple);
  g_object_unref (simple);

  if (data->error_channel_source)
    {
      g_source_destroy (data->error_channel_source);
      g_source_unref (data->error_channel_source);
    }
  g_io_channel_unref (data->error_channel);
  g_string_free (data->error_string, TRUE);
  g_strfreev (data->argv);
  close (data->error_fd);
  g_spawn_close_pid (pid);
  g_free (data);
}

static gboolean
eject_unmount_read_error (GIOChannel *channel,
                    GIOCondition condition,
                    gpointer user_data)
{
  UnmountEjectOp *data = user_data;
  char buf[BUFSIZ];
  gsize bytes_read;
  GError *error;
  GIOStatus status;

  error = NULL;
read:
  status = g_io_channel_read_chars (channel, buf, sizeof (buf), &bytes_read, &error);
  if (status == G_IO_STATUS_NORMAL)
   {
     g_string_append_len (data->error_string, buf, bytes_read);
     if (bytes_read == sizeof (buf))
        goto read;
   }
  else if (status == G_IO_STATUS_EOF)
    g_string_append_len (data->error_string, buf, bytes_read);
  else if (status == G_IO_STATUS_ERROR)
    {
      if (data->error_string->len > 0)
        g_string_append (data->error_string, "\n");

      g_string_append (data->error_string, error->message);
      g_error_free (error);

      if (data->error_channel_source)
        {
          g_source_unref (data->error_channel_source);
          data->error_channel_source = NULL;
        }
      return FALSE;
    }

  return TRUE;
}

static gboolean
eject_unmount_do_cb (gpointer user_data)
{
  UnmountEjectOp *data = (UnmountEjectOp *) user_data;
  GPid child_pid;
  GSource *child_watch;
  GError *error = NULL;

  if (!g_spawn_async_with_pipes (NULL,         /* working dir */
                                 data->argv,
                                 NULL,         /* envp */
                                 G_SPAWN_DO_NOT_REAP_CHILD|G_SPAWN_SEARCH_PATH,
                                 NULL,         /* child_setup */
                                 NULL,         /* user_data for child_setup */
                                 &child_pid,
                                 NULL,           /* standard_input */
                                 NULL,           /* standard_output */
                                 &(data->error_fd),
                                 &error)) {
    g_assert (error != NULL);
    goto handle_error;
  }

  data->error_string = g_string_new ("");

  data->error_channel = g_io_channel_unix_new (data->error_fd);
  g_io_channel_set_flags (data->error_channel, G_IO_FLAG_NONBLOCK, &error);
  if (error != NULL)
    goto handle_error;

  data->error_channel_source = g_io_create_watch (data->error_channel, G_IO_IN);
  g_source_set_callback (data->error_channel_source,
                         (GSourceFunc) eject_unmount_read_error, data, NULL);
  g_source_attach (data->error_channel_source, g_main_context_get_thread_default ());

  child_watch = g_child_watch_source_new (child_pid);
  g_source_set_callback (child_watch, (GSourceFunc) eject_unmount_cb, data, NULL);
  g_source_attach (child_watch, g_main_context_get_thread_default ());
  g_source_unref (child_watch);

handle_error:
  if (error != NULL) {
    GSimpleAsyncResult *simple;
    simple = g_simple_async_result_new_from_error (G_OBJECT (data->unix_mount),
                                                   data->callback,
                                                   data->user_data,
                                                   error);
    g_simple_async_result_complete (simple);
    g_object_unref (simple);

    if (data->error_string != NULL)
      g_string_free (data->error_string, TRUE);

    if (data->error_channel != NULL)
      g_io_channel_unref (data->error_channel);

    g_strfreev (data->argv);
    g_error_free (error);
    g_free (data);
  }

  return FALSE;
}

static void
eject_unmount_do (GMount              *mount,
                  GCancellable        *cancellable,
                  GAsyncReadyCallback  callback,
                  gpointer             user_data,
                  char               **argv)
{
  GUnixMount *unix_mount = G_UNIX_MOUNT (mount);
  UnmountEjectOp *data;

  data = g_new0 (UnmountEjectOp, 1);
  data->unix_mount = unix_mount;
  data->callback = callback;
  data->user_data = user_data;
  data->cancellable = cancellable;
  data->argv = g_strdupv (argv);

  if (unix_mount->volume_monitor != NULL)
    g_signal_emit_by_name (unix_mount->volume_monitor, "mount-pre-unmount", mount);

  g_signal_emit_by_name (mount, "pre-unmount", 0);

  g_timeout_add (500, (GSourceFunc) eject_unmount_do_cb, data);
}

static void
g_unix_mount_unmount (GMount             *mount,
                      GMountUnmountFlags flags,
                      GCancellable        *cancellable,
                      GAsyncReadyCallback  callback,
                      gpointer             user_data)
{
  GUnixMount *unix_mount = G_UNIX_MOUNT (mount);
  char *argv[] = {"umount", NULL, NULL};

  if (unix_mount->mount_path != NULL)
    argv[1] = unix_mount->mount_path;
  else
    argv[1] = unix_mount->device_path;

  eject_unmount_do (mount, cancellable, callback, user_data, argv);
}

static gboolean
g_unix_mount_unmount_finish (GMount       *mount,
                             GAsyncResult  *result,
                             GError       **error)
{
  return TRUE;
}

static void
g_unix_mount_eject (GMount             *mount,
                    GMountUnmountFlags flags,
                    GCancellable        *cancellable,
                    GAsyncReadyCallback  callback,
                    gpointer             user_data)
{
  GUnixMount *unix_mount = G_UNIX_MOUNT (mount);
  char *argv[] = {"eject", NULL, NULL};

  if (unix_mount->mount_path != NULL)
    argv[1] = unix_mount->mount_path;
  else
    argv[1] = unix_mount->device_path;

  eject_unmount_do (mount, cancellable, callback, user_data, argv);
}

static gboolean
g_unix_mount_eject_finish (GMount       *mount,
                           GAsyncResult  *result,
                           GError       **error)
{
  return TRUE;
}

static void
g_unix_mount_mount_iface_init (GMountIface *iface)
{
  iface->get_root = g_unix_mount_get_root;
  iface->get_name = g_unix_mount_get_name;
  iface->get_icon = g_unix_mount_get_icon;
  iface->get_uuid = g_unix_mount_get_uuid;
  iface->get_drive = g_unix_mount_get_drive;
  iface->get_volume = g_unix_mount_get_volume;
  iface->can_unmount = g_unix_mount_can_unmount;
  iface->can_eject = g_unix_mount_can_eject;
  iface->unmount = g_unix_mount_unmount;
  iface->unmount_finish = g_unix_mount_unmount_finish;
  iface->eject = g_unix_mount_eject;
  iface->eject_finish = g_unix_mount_eject_finish;
}
