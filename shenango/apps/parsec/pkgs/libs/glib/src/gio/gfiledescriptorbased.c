/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright (C) 2010 Christian Kellner
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
 * Author: Christian Kellner <gicmo@gnome.org>
 */

#include "config.h"
#include "gfiledescriptorbased.h"
#include "glibintl.h"

#include "gioalias.h"

/**
 * SECTION:gfiledescriptorbased
 * @short_description: Interface for file descriptor based IO
 * @include: gio/gio.h
 * @see_also: #GInputStream, #GOutputStream
 *
 * #GFileDescriptorBased is implemented by streams (implementations of
 * #GInputStream or #GOutputStream) that are based on file descriptors.
 *
 * Since: 2.24
 *
 **/

typedef GFileDescriptorBasedIface GFileDescriptorBasedInterface;
G_DEFINE_INTERFACE (GFileDescriptorBased, g_file_descriptor_based, G_TYPE_OBJECT)

static void
g_file_descriptor_based_default_init (GFileDescriptorBasedInterface *iface)
{
}

/**
 * g_file_descriptor_based_get_fd:
 * @fd_based: a #GFileDescriptorBased.
 *
 * Gets the underlying file descriptor.
 *
 * Returns: The file descriptor
 *
 * Since: 2.24
 **/
int
g_file_descriptor_based_get_fd (GFileDescriptorBased *fd_based)
{
  GFileDescriptorBasedIface *iface;

  g_return_val_if_fail (G_IS_FILE_DESCRIPTOR_BASED (fd_based), 0);

  iface = G_FILE_DESCRIPTOR_BASED_GET_IFACE (fd_based);

  return (* iface->get_fd) (fd_based);
}


#define __G_FILE_DESCRIPTOR_BASED_C__
#include "gioaliasdef.c"
