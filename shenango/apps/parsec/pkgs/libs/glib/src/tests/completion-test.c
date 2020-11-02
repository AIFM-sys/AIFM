/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
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
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GLib Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GLib Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GLib at ftp://ftp.gtk.org/pub/gtk/. 
 */
#include <string.h>

#include "glib.h"

int main (int argc, char *argv[])
{
  GCompletion *cmp;
  GList *items;
  gchar *prefix;
  
  cmp = g_completion_new (NULL);

  items = NULL;
  items = g_list_append (items, "a\302\243");
  items = g_list_append (items, "a\302\244");
  items = g_list_append (items, "bb");
  items = g_list_append (items, "bc");
  g_completion_add_items (cmp, items);

  items = g_completion_complete (cmp, "a", &prefix);
  g_assert (!strcmp ("a\302", prefix));
  g_assert (g_list_length (items) == 2);
  g_free (prefix);
  
  items = g_completion_complete_utf8 (cmp, "a", &prefix);
  g_assert (!strcmp ("a", prefix));
  g_assert (g_list_length (items) == 2);
  g_free (prefix);

  items = g_completion_complete (cmp, "b", &prefix);
  g_assert (!strcmp ("b", prefix));
  g_assert (g_list_length (items) == 2);
  g_free (prefix);
  
  items = g_completion_complete_utf8 (cmp, "b", &prefix);
  g_assert (!strcmp ("b", prefix));
  g_assert (g_list_length (items) == 2);
  g_free (prefix);

  items = g_completion_complete (cmp, "a", NULL);
  g_assert (g_list_length (items) == 2);

  items = g_completion_complete_utf8 (cmp, "a", NULL);
  g_assert (g_list_length (items) == 2);

  g_completion_free (cmp);

  return 0;
}
