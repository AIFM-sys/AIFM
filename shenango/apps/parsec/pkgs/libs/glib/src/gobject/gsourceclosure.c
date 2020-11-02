/* GObject - GLib Type, Object, Parameter and Signal Library
 * Copyright (C) 2001 Red Hat, Inc.
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
 */

#include "config.h"

#include "gsourceclosure.h"
#include "gboxed.h"
#include "genums.h"
#include "gmarshal.h"
#include "gvalue.h"
#include "gvaluetypes.h"
#include "gobjectalias.h"


GType
g_io_channel_get_type (void)
{
  static GType our_type = 0;
  
  if (our_type == 0)
    our_type = g_boxed_type_register_static ("GIOChannel",
					     (GBoxedCopyFunc) g_io_channel_ref,
					     (GBoxedFreeFunc) g_io_channel_unref);

  return our_type;
}

GType
g_io_condition_get_type (void)
{
  static GType etype = 0;
  if (etype == 0)
    {
      static const GFlagsValue values[] = {
	{ G_IO_IN,   "G_IO_IN",   "in" },
	{ G_IO_OUT,  "G_IO_OUT",  "out" },
	{ G_IO_PRI,  "G_IO_PRI",  "pri" },
	{ G_IO_ERR,  "G_IO_ERR",  "err" },
	{ G_IO_HUP,  "G_IO_HUP",  "hup" },
	{ G_IO_NVAL, "G_IO_NVAL", "nval" },
	{ 0, NULL, NULL }
      };
      etype = g_flags_register_static ("GIOCondition", values);
    }
  return etype;
}

/* We need to hand-write this marshaler, since it doesn't have an
 * instance object.
 */
static void
source_closure_marshal_BOOLEAN__VOID (GClosure     *closure,
				      GValue       *return_value,
				      guint         n_param_values,
				      const GValue *param_values,
				      gpointer      invocation_hint,
				      gpointer      marshal_data)
{
  GSourceFunc callback;
  GCClosure *cc = (GCClosure*) closure;
  gboolean v_return;

  g_return_if_fail (return_value != NULL);
  g_return_if_fail (n_param_values == 0);

  callback = (GSourceFunc) (marshal_data ? marshal_data : cc->callback);

  v_return = callback (closure->data);

  g_value_set_boolean (return_value, v_return);
}

static gboolean
io_watch_closure_callback (GIOChannel   *channel,
			   GIOCondition  condition,
			   gpointer      data)
{
  GClosure *closure = data;

  GValue params[2] = { { 0, }, { 0, } };
  GValue result_value = { 0, };
  gboolean result;

  g_value_init (&result_value, G_TYPE_BOOLEAN);
  g_value_init (&params[0], G_TYPE_IO_CHANNEL);
  g_value_set_boxed (&params[0], channel);
		     
  g_value_init (&params[1], G_TYPE_IO_CONDITION);
  g_value_set_flags (&params[1], condition);

  g_closure_invoke (closure, &result_value, 2, params, NULL);

  result = g_value_get_boolean (&result_value);
  g_value_unset (&result_value);
  g_value_unset (&params[0]);
  g_value_unset (&params[1]);

  return result;
}

static gboolean
source_closure_callback (gpointer data)
{
  GClosure *closure = data;
  GValue result_value = { 0, };
  gboolean result;

  g_value_init (&result_value, G_TYPE_BOOLEAN);
  
  g_closure_invoke (closure, &result_value, 0, NULL, NULL);

  result = g_value_get_boolean (&result_value);
  g_value_unset (&result_value);

  return result;
}

static void
closure_callback_get (gpointer     cb_data,
		      GSource     *source,
		      GSourceFunc *func,
		      gpointer    *data)
{
  GSourceFunc closure_callback = source->source_funcs->closure_callback;

  if (!closure_callback)
    {
      if (source->source_funcs == &g_io_watch_funcs)
	closure_callback = (GSourceFunc)io_watch_closure_callback;
      else if (source->source_funcs == &g_timeout_funcs ||
	       source->source_funcs == &g_idle_funcs)
	closure_callback = source_closure_callback;
    }

  *func = closure_callback;
  *data = cb_data;
}

static GSourceCallbackFuncs closure_callback_funcs = {
  (void (*) (gpointer)) g_closure_ref,
  (void (*) (gpointer)) g_closure_unref,
  closure_callback_get
};

/**
 * g_source_set_closure:
 * @source: the source
 * @closure: a #GClosure
 *
 * Set the callback for a source as a #GClosure.
 *
 * If the source is not one of the standard GLib types, the @closure_callback
 * and @closure_marshal fields of the #GSourceFuncs structure must have been
 * filled in with pointers to appropriate functions.
 */
void
g_source_set_closure (GSource  *source,
		      GClosure *closure)
{
  g_return_if_fail (source != NULL);
  g_return_if_fail (closure != NULL);

  if (!source->source_funcs->closure_callback &&
      source->source_funcs != &g_io_watch_funcs &&
      source->source_funcs != &g_timeout_funcs &&
      source->source_funcs != &g_idle_funcs)
    {
      g_critical (G_STRLOC "closure can not be set on closure without GSourceFuncs::closure_callback\n");
      return;
    }

  g_closure_ref (closure);
  g_closure_sink (closure);
  g_source_set_callback_indirect (source, closure, &closure_callback_funcs);

  if (G_CLOSURE_NEEDS_MARSHAL (closure))
    {
      GClosureMarshal marshal = (GClosureMarshal)source->source_funcs->closure_marshal;
      if (!marshal)
	{
	  if (source->source_funcs == &g_idle_funcs ||
	      source->source_funcs == &g_timeout_funcs)
	    marshal = source_closure_marshal_BOOLEAN__VOID;
	  else if (source->source_funcs == &g_io_watch_funcs)
	    marshal = g_cclosure_marshal_BOOLEAN__FLAGS;
	}
      if (marshal)
	g_closure_set_marshal (closure, marshal);
    }
}

#define __G_SOURCECLOSURE_C__
#include "gobjectaliasdef.c"
