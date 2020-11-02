/*
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

   Authors:.
		John McCutchan <john@johnmccutchan.com>
*/

#ifndef __INOTIFY_KERNEL_H
#define __INOTIFY_KERNEL_H

typedef struct ik_event_s {
  gint32 wd;
  guint32 mask;
  guint32 cookie;
  guint32 len;
  char *  name;
  struct ik_event_s *pair;
} ik_event_t;

gboolean _ik_startup (void (*cb) (ik_event_t *event));

ik_event_t *_ik_event_new_dummy (const char *name,
				 gint32      wd,
				 guint32     mask);
void        _ik_event_free      (ik_event_t *event);

gint32      _ik_watch           (const char *path,
				 guint32     mask,
				 int        *err);
int         _ik_ignore          (const char *path,
				 gint32      wd);


/* The miss count will probably be enflated */
void        _ik_move_stats     (guint32 *matches,
				guint32 *misses);
const char *_ik_mask_to_string (guint32  mask);


#endif
