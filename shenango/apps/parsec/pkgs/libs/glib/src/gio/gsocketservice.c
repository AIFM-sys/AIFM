/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright © 2009 Codethink Limited
 * Copyright © 2009 Red Hat, Inc
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2 of the licence or (at
 * your option) any later version.
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
 * Authors: Ryan Lortie <desrt@desrt.ca>
 *          Alexander Larsson <alexl@redhat.com>
 */

/**
 * SECTION: gsocketservice
 * @title: GSocketService
 * @short_description: Make it easy to implement a network service
 * @see_also: #GThreadedSocketService, #GSocketListener.
 *
 * A #GSocketService is an object that represents a service that is
 * provided to the network or over local sockets.  When a new
 * connection is made to the service the #GSocketService:incoming
 * signal is emitted.
 *
 * A #GSocketService is a subclass of #GSocketListener and you need
 * to add the addresses you want to accept connections on to the
 * with the #GSocketListener APIs.
 *
 * There are two options for implementing a network service based on
 * #GSocketService. The first is to create the service using
 * g_socket_service_new() and to connect to the #GSocketService:incoming
 * signal. The second is to subclass #GSocketService and override the
 * default signal handler implementation.
 *
 * In either case, the handler must immediately return, or else it
 * will block additional incoming connections from being serviced.
 * If you are interested in writing connection handlers that contain
 * blocking code then see #GThreadedSocketService.
 *
 * The socket service runs on the main loop in the main thread, and is
 * not threadsafe in general. However, the calls to start and stop
 * the service are threadsafe so these can be used from threads that
 * handle incoming clients.
 *
 * Since: 2.22
 */

#include "config.h"
#include "gsocketservice.h"

#include "gio-marshal.h"
#include <gio/gio.h>
#include "gsocketlistener.h"
#include "gsocketconnection.h"

#include "gioalias.h"

static guint g_socket_service_incoming_signal;

G_DEFINE_TYPE (GSocketService, g_socket_service, G_TYPE_SOCKET_LISTENER);

G_LOCK_DEFINE_STATIC(active);

struct _GSocketServicePrivate
{
  GCancellable *cancellable;
  guint active : 1;
  guint outstanding_accept : 1;
};

static void g_socket_service_ready (GObject      *object,
				    GAsyncResult *result,
				    gpointer      user_data);

static gboolean
g_socket_service_real_incoming (GSocketService    *service,
                                GSocketConnection *connection,
                                GObject           *source_object)
{
  return FALSE;
}

static void
g_socket_service_init (GSocketService *service)
{
  service->priv = G_TYPE_INSTANCE_GET_PRIVATE (service,
					       G_TYPE_SOCKET_SERVICE,
					       GSocketServicePrivate);
  service->priv->cancellable = g_cancellable_new ();
  service->priv->active = TRUE;
}

static void
g_socket_service_finalize (GObject *object)
{
  GSocketService *service = G_SOCKET_SERVICE (object);

  g_object_unref (service->priv->cancellable);

  G_OBJECT_CLASS (g_socket_service_parent_class)
    ->finalize (object);
}

static void
do_accept (GSocketService  *service)
{
  g_socket_listener_accept_async (G_SOCKET_LISTENER (service),
				  service->priv->cancellable,
				  g_socket_service_ready, NULL);
  service->priv->outstanding_accept = TRUE;
}

static void
g_socket_service_changed (GSocketListener *listener)
{
  GSocketService  *service = G_SOCKET_SERVICE (listener);

  G_LOCK (active);

  if (service->priv->active)
    {
      if (service->priv->outstanding_accept)
	g_cancellable_cancel (service->priv->cancellable);
      else
	{
	  g_socket_listener_accept_async (listener, service->priv->cancellable,
					  g_socket_service_ready, NULL);
	  service->priv->outstanding_accept = TRUE;
	}
    }

  G_UNLOCK (active);
}

/**
 * g_socket_service_is_active:
 * @service: a #GSocketService
 *
 * Check whether the service is active or not. An active
 * service will accept new clients that connect, while
 * a non-active service will let connecting clients queue
 * up until the service is started.
 *
 * Returns: %TRUE if the service is active, %FALSE otherwise
 *
 * Since: 2.22
 */
gboolean
g_socket_service_is_active (GSocketService *service)
{
  gboolean active;

  G_LOCK (active);
  active = service->priv->active;
  G_UNLOCK (active);
  return active;
}

/**
 * g_socket_service_start:
 * @service: a #GSocketService
 *
 * Starts the service, i.e. start accepting connections
 * from the added sockets when the mainloop runs.
 *
 * This call is threadsafe, so it may be called from a thread
 * handling an incomming client request.
 *
 * Since: 2.22
 */
void
g_socket_service_start (GSocketService *service)
{
  G_LOCK (active);

  if (!service->priv->active)
    {
      service->priv->active = TRUE;

      if (service->priv->outstanding_accept)
	g_cancellable_cancel (service->priv->cancellable);
      else
	do_accept (service);
    }

  G_UNLOCK (active);
}

/**
 * g_socket_service_stop:
 * @service: a #GSocketService
 *
 * Stops the service, i.e. stops accepting connections
 * from the added sockets when the mainloop runs.
 *
 * This call is threadsafe, so it may be called from a thread
 * handling an incomming client request.
 *
 * Since: 2.22
 */
void
g_socket_service_stop (GSocketService *service)
{
  G_LOCK (active);

  if (service->priv->active)
    {
      service->priv->active = FALSE;

      if (service->priv->outstanding_accept)
	g_cancellable_cancel (service->priv->cancellable);
    }

  G_UNLOCK (active);
}


static gboolean
g_socket_service_incoming (GSocketService    *service,
                           GSocketConnection *connection,
                           GObject           *source_object)
{
  gboolean result;

  g_signal_emit (service, g_socket_service_incoming_signal,
                 0, connection, source_object, &result);
  return result;
}

static void
g_socket_service_class_init (GSocketServiceClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);
  GSocketListenerClass *listener_class = G_SOCKET_LISTENER_CLASS (class);

  g_type_class_add_private (class, sizeof (GSocketServicePrivate));

  gobject_class->finalize = g_socket_service_finalize;
  listener_class->changed = g_socket_service_changed;
  class->incoming = g_socket_service_real_incoming;

  /**
   * GSocketService::incoming:
   * @service: the #GSocketService.
   * @connection: a new #GSocketConnection object.
   * @source_object: the source_object passed to g_socket_listener_add_address().
   *
   * The ::incoming signal is emitted when a new incoming connection
   * to @service needs to be handled. The handler must initiate the
   * handling of @connection, but may not block; in essence,
   * asynchronous operations must be used.
   *
   * Returns: %TRUE to stop other handlers from being called
   *
   * Since: 2.22
   */
  g_socket_service_incoming_signal =
    g_signal_new ("incoming", G_TYPE_FROM_CLASS (class), G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GSocketServiceClass, incoming),
                  g_signal_accumulator_true_handled, NULL,
                  _gio_marshal_BOOLEAN__OBJECT_OBJECT, G_TYPE_BOOLEAN,
                  2, G_TYPE_SOCKET_CONNECTION, G_TYPE_OBJECT);
}

static void
g_socket_service_ready (GObject      *object,
                        GAsyncResult *result,
                        gpointer      user_data)
{
  GSocketListener *listener = G_SOCKET_LISTENER (object);
  GSocketService *service = G_SOCKET_SERVICE (object);
  GSocketConnection *connection;
  GObject *source_object;
  GError *error = NULL;

  connection = g_socket_listener_accept_finish (listener, result, &source_object, &error);
  if (error)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
	g_warning ("fail: %s", error->message);
      g_error_free (error);
    }
  else
    {
      g_socket_service_incoming (service, connection, source_object);
      g_object_unref (connection);
    }

  G_LOCK (active);

  g_cancellable_reset (service->priv->cancellable);

  /* requeue */
  service->priv->outstanding_accept = FALSE;
  if (service->priv->active)
    do_accept (service);

  G_UNLOCK (active);
}


/**
 * g_socket_service_new:
 *
 * Creates a new #GSocketService with no sockets to listen for.
 * New listeners can be added with e.g. g_socket_listener_add_address()
 * or g_socket_listener_add_inet_port().
 *
 * Returns: a new #GSocketService.
 *
 * Since: 2.22
 */
GSocketService *
g_socket_service_new (void)
{
  return g_object_new (G_TYPE_SOCKET_SERVICE, NULL);
}

#define __G_SOCKET_SERVICE_C__
#include "gioaliasdef.c"
