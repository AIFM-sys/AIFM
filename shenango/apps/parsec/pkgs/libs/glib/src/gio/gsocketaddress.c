/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright (C) 2008 Christian Kellner, Samuel Cormier-Iijima
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
 * Authors: Christian Kellner <gicmo@gnome.org>
 *          Samuel Cormier-Iijima <sciyoshi@gmail.com>
 */

#include <config.h>
#include <glib.h>

#include "gsocketaddress.h"
#include "ginetaddress.h"
#include "ginetsocketaddress.h"
#include "gnetworkingprivate.h"
#include "gsocketaddressenumerator.h"
#include "gsocketconnectable.h"
#include "glibintl.h"
#include "gioenumtypes.h"

#ifdef G_OS_UNIX
#include "gunixsocketaddress.h"
#endif

#include "gioalias.h"

/**
 * SECTION:gsocketaddress
 * @short_description: Abstract base class representing endpoints for
 * socket communication
 *
 * #GSocketAddress is the equivalent of <type>struct sockaddr</type>
 * in the BSD sockets API. This is an abstract class; use
 * #GInetSocketAddress for internet sockets, or #GUnixSocketAddress
 * for UNIX domain sockets.
 */

/**
 * GSocketAddress:
 *
 * A socket endpoint address, corresponding to <type>struct sockaddr</type>
 * or one of its subtypes.
 */

enum
{
  PROP_NONE,
  PROP_FAMILY
};

static void                      g_socket_address_connectable_iface_init (GSocketConnectableIface *iface);
static GSocketAddressEnumerator *g_socket_address_connectable_enumerate  (GSocketConnectable      *connectable);

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (GSocketAddress, g_socket_address, G_TYPE_OBJECT,
				  G_IMPLEMENT_INTERFACE (G_TYPE_SOCKET_CONNECTABLE,
							 g_socket_address_connectable_iface_init))

/**
 * g_socket_address_get_family:
 * @address: a #GSocketAddress
 *
 * Gets the socket family type of @address.
 *
 * Returns: the socket family type of @address.
 *
 * Since: 2.22
 */
GSocketFamily
g_socket_address_get_family (GSocketAddress *address)
{
  g_return_val_if_fail (G_IS_SOCKET_ADDRESS (address), 0);

  return G_SOCKET_ADDRESS_GET_CLASS (address)->get_family (address);
}

static void
g_socket_address_get_property (GObject *object, guint prop_id,
			       GValue *value, GParamSpec *pspec)
{
  GSocketAddress *address = G_SOCKET_ADDRESS (object);

  switch (prop_id)
    {
     case PROP_FAMILY:
      g_value_set_enum (value, g_socket_address_get_family (address));
      break;

     default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
g_socket_address_class_init (GSocketAddressClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->get_property = g_socket_address_get_property;

  g_object_class_install_property (gobject_class, PROP_FAMILY,
                                   g_param_spec_enum ("family",
						      P_("Address family"),
						      P_("The family of the socket address"),
						      G_TYPE_SOCKET_FAMILY,
						      G_SOCKET_FAMILY_INVALID,
						      G_PARAM_READABLE |
                                                      G_PARAM_STATIC_STRINGS));
}

static void
g_socket_address_connectable_iface_init (GSocketConnectableIface *connectable_iface)
{
  connectable_iface->enumerate  = g_socket_address_connectable_enumerate;
}

static void
g_socket_address_init (GSocketAddress *address)
{

}

/**
 * g_socket_address_get_native_size:
 * @address: a #GSocketAddress
 *
 * Gets the size of @address's native <type>struct sockaddr</type>.
 * You can use this to allocate memory to pass to
 * g_socket_address_to_native().
 *
 * Returns: the size of the native <type>struct sockaddr</type> that
 *     @address represents
 *
 * Since: 2.22
 */
gssize
g_socket_address_get_native_size (GSocketAddress *address)
{
  g_return_val_if_fail (G_IS_SOCKET_ADDRESS (address), -1);

  return G_SOCKET_ADDRESS_GET_CLASS (address)->get_native_size (address);
}

/**
 * g_socket_address_to_native:
 * @address: a #GSocketAddress
 * @dest: a pointer to a memory location that will contain the native
 * <type>struct sockaddr</type>.
 * @destlen: the size of @dest. Must be at least as large as
 * g_socket_address_get_native_size().
 * @error: #GError for error reporting, or %NULL to ignore.
 *
 * Converts a #GSocketAddress to a native <type>struct
 * sockaddr</type>, which can be passed to low-level functions like
 * connect() or bind().
 *
 * If not enough space is availible, a %G_IO_ERROR_NO_SPACE error is
 * returned. If the address type is not known on the system
 * then a %G_IO_ERROR_NOT_SUPPORTED error is returned.
 *
 * Returns: %TRUE if @dest was filled in, %FALSE on error
 *
 * Since: 2.22
 */
gboolean
g_socket_address_to_native (GSocketAddress  *address,
			    gpointer         dest,
			    gsize            destlen,
			    GError         **error)
{
  g_return_val_if_fail (G_IS_SOCKET_ADDRESS (address), FALSE);

  return G_SOCKET_ADDRESS_GET_CLASS (address)->to_native (address, dest, destlen, error);
}

/**
 * g_socket_address_new_from_native:
 * @native: a pointer to a <type>struct sockaddr</type>
 * @len: the size of the memory location pointed to by @native
 *
 * Creates a #GSocketAddress subclass corresponding to the native
 * <type>struct sockaddr</type> @native.
 *
 * Returns: a new #GSocketAddress if @native could successfully be converted,
 * otherwise %NULL.
 *
 * Since: 2.22
 */
GSocketAddress *
g_socket_address_new_from_native (gpointer native,
				  gsize    len)
{
  gshort family;

  if (len < sizeof (gshort))
    return NULL;

  family = ((struct sockaddr *) native)->sa_family;

  if (family == AF_UNSPEC)
    return NULL;

  if (family == AF_INET)
    {
      struct sockaddr_in *addr = (struct sockaddr_in *) native;
      GInetAddress *iaddr = g_inet_address_new_from_bytes ((guint8 *) &(addr->sin_addr), AF_INET);
      GSocketAddress *sockaddr;

      sockaddr = g_inet_socket_address_new (iaddr, g_ntohs (addr->sin_port));
      g_object_unref (iaddr);
      return sockaddr;
    }

  if (family == AF_INET6)
    {
      struct sockaddr_in6 *addr = (struct sockaddr_in6 *) native;
      GInetAddress *iaddr = g_inet_address_new_from_bytes ((guint8 *) &(addr->sin6_addr), AF_INET6);
      GSocketAddress *sockaddr;

      sockaddr = g_inet_socket_address_new (iaddr, g_ntohs (addr->sin6_port));
      g_object_unref (iaddr);
      return sockaddr;
    }

#ifdef G_OS_UNIX
  if (family == AF_UNIX)
    {
      struct sockaddr_un *addr = (struct sockaddr_un *) native;

      if (addr->sun_path[0] == 0)
	return g_unix_socket_address_new_abstract (addr->sun_path+1,
						   sizeof (addr->sun_path) - 1);
      return g_unix_socket_address_new (addr->sun_path);
    }
#endif

  return NULL;
}


#define G_TYPE_SOCKET_ADDRESS_ADDRESS_ENUMERATOR (_g_socket_address_address_enumerator_get_type ())
#define G_SOCKET_ADDRESS_ADDRESS_ENUMERATOR(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), G_TYPE_SOCKET_ADDRESS_ADDRESS_ENUMERATOR, GSocketAddressAddressEnumerator))

typedef struct {
  GSocketAddressEnumerator parent_instance;

  GSocketAddress *sockaddr;
} GSocketAddressAddressEnumerator;

typedef struct {
  GSocketAddressEnumeratorClass parent_class;

} GSocketAddressAddressEnumeratorClass;

G_DEFINE_TYPE (GSocketAddressAddressEnumerator, _g_socket_address_address_enumerator, G_TYPE_SOCKET_ADDRESS_ENUMERATOR)

static void
g_socket_address_address_enumerator_finalize (GObject *object)
{
  GSocketAddressAddressEnumerator *sockaddr_enum =
    G_SOCKET_ADDRESS_ADDRESS_ENUMERATOR (object);

  if (sockaddr_enum->sockaddr)
    g_object_unref (sockaddr_enum->sockaddr);

  G_OBJECT_CLASS (_g_socket_address_address_enumerator_parent_class)->finalize (object);
}

static GSocketAddress *
g_socket_address_address_enumerator_next (GSocketAddressEnumerator  *enumerator,
					  GCancellable              *cancellable,
					  GError                   **error)
{
  GSocketAddressAddressEnumerator *sockaddr_enum =
    G_SOCKET_ADDRESS_ADDRESS_ENUMERATOR (enumerator);

  if (sockaddr_enum->sockaddr)
    {
      GSocketAddress *ret = sockaddr_enum->sockaddr;

      sockaddr_enum->sockaddr = NULL;
      return ret;
    }
  else
    return NULL;
}

static void
_g_socket_address_address_enumerator_init (GSocketAddressAddressEnumerator *enumerator)
{
}

static void
_g_socket_address_address_enumerator_class_init (GSocketAddressAddressEnumeratorClass *sockaddrenum_class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (sockaddrenum_class);
  GSocketAddressEnumeratorClass *enumerator_class =
    G_SOCKET_ADDRESS_ENUMERATOR_CLASS (sockaddrenum_class);

  enumerator_class->next = g_socket_address_address_enumerator_next;
  object_class->finalize = g_socket_address_address_enumerator_finalize;
}

static GSocketAddressEnumerator *
g_socket_address_connectable_enumerate (GSocketConnectable *connectable)
{
  GSocketAddressAddressEnumerator *sockaddr_enum;

  sockaddr_enum = g_object_new (G_TYPE_SOCKET_ADDRESS_ADDRESS_ENUMERATOR, NULL);
  sockaddr_enum->sockaddr = g_object_ref (connectable);

  return (GSocketAddressEnumerator *)sockaddr_enum;
}

#define __G_SOCKET_ADDRESS_C__
#include "gioaliasdef.c"
