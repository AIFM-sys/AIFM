/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 2; tab-width: 8 -*- */

/* inotify-helper.c - Gnome VFS Monitor based on inotify.

   Copyright (C) 2005 John McCutchan

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Authors: 
		 John McCutchan <john@johnmccutchan.com>
*/

#include "config.h"
#include <glib.h>
#include "inotify-missing.h"
#include "inotify-path.h"

#define SCAN_MISSING_TIME 4 /* 1/4 Hz */

static gboolean im_debug_enabled = FALSE;
#define IM_W if (im_debug_enabled) g_warning

/* We put inotify_sub's that are missing on this list */
static GList *missing_sub_list = NULL;
static gboolean im_scan_missing (gpointer user_data);
static gboolean scan_missing_running = FALSE;
static void (*missing_cb)(inotify_sub *sub) = NULL;

G_LOCK_EXTERN (inotify_lock);

/* inotify_lock must be held before calling */
void
_im_startup (void (*callback)(inotify_sub *sub))
{
  static gboolean initialized = FALSE;
  
  if (!initialized)
    {
      missing_cb = callback;
      initialized = TRUE;
    }
}

/* inotify_lock must be held before calling */
void
_im_add (inotify_sub *sub)
{
  if (g_list_find (missing_sub_list, sub))
    {
      IM_W ("asked to add %s to missing list but it's already on the list!\n", sub->dirname);
      return;
    }

  IM_W ("adding %s to missing list\n", sub->dirname);
  missing_sub_list = g_list_prepend (missing_sub_list, sub);

  /* If the timeout is turned off, we turn it back on */
  if (!scan_missing_running)
    {
      scan_missing_running = TRUE;
      g_timeout_add_seconds (SCAN_MISSING_TIME, im_scan_missing, NULL);
    }
}

/* inotify_lock must be held before calling */
void
_im_rm (inotify_sub *sub)
{
  GList *link;
  
  link = g_list_find (missing_sub_list, sub);

  if (!link)
    {
      IM_W ("asked to remove %s from missing list but it isn't on the list!\n", sub->dirname);
      return;
    }

  IM_W ("removing %s from missing list\n", sub->dirname);

  missing_sub_list = g_list_remove_link (missing_sub_list, link);
  g_list_free_1 (link);
}

/* Scans the list of missing subscriptions checking if they
 * are available yet.
 */
static gboolean
im_scan_missing (gpointer user_data)
{
  GList *nolonger_missing = NULL;
  GList *l;
  
  G_LOCK (inotify_lock);
  
  IM_W ("scanning missing list with %d items\n", g_list_length (missing_sub_list));
  for (l = missing_sub_list; l; l = l->next)
    {
      inotify_sub *sub = l->data;
      gboolean not_m = FALSE;
      
      IM_W ("checking %p\n", sub);
      g_assert (sub);
      g_assert (sub->dirname);
      not_m = _ip_start_watching (sub);

      if (not_m)
	{
	  missing_cb (sub);
	  IM_W ("removed %s from missing list\n", sub->dirname);
	  /* We have to build a list of list nodes to remove from the
	   * missing_sub_list. We do the removal outside of this loop.
	   */
	  nolonger_missing = g_list_prepend (nolonger_missing, l);
	} 
    }

  for (l = nolonger_missing; l ; l = l->next)
    {
      GList *llink = l->data;
      missing_sub_list = g_list_remove_link (missing_sub_list, llink);
      g_list_free_1 (llink);
    }

  g_list_free (nolonger_missing);
  
  /* If the missing list is now empty, we disable the timeout */
  if (missing_sub_list == NULL)
    {
      scan_missing_running = FALSE;
      G_UNLOCK (inotify_lock);
      return FALSE;
    }
  else
    {
      G_UNLOCK (inotify_lock);
      return TRUE;
    }
}


/* inotify_lock must be held */
void
_im_diag_dump (GIOChannel *ioc)
{
  GList *l;
  g_io_channel_write_chars (ioc, "missing list:\n", -1, NULL, NULL);
  for (l = missing_sub_list; l; l = l->next)
    {
      inotify_sub *sub = l->data;
      g_io_channel_write_chars (ioc, sub->dirname, -1, NULL, NULL);
      g_io_channel_write_chars (ioc, "\n", -1, NULL, NULL);
    }
}
