/* GMODULE - GLIB wrapper code for dynamic module loading
 * Copyright (C) 1998, 2000 Tim Janik
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
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

/* 
 * MT safe
 */
#include "config.h"

#include <dlfcn.h>

/* Perl includes <nlist.h> and <link.h> instead of <dlfcn.h> on some systmes? */


/* dlerror() is not implemented on all systems
 */
#ifndef	G_MODULE_HAVE_DLERROR
/* could we rely on errno's state here? */
#  define dlerror()	"unknown dl-error"
#endif	/* G_MODULE_HAVE_DLERROR */

/* some flags are missing on some systems, so we provide
 * harmless defaults.
 * The Perl sources say, RTLD_LAZY needs to be defined as (1),
 * at least for Solaris 1.
 *
 * Mandatory:
 * RTLD_LAZY   - resolve undefined symbols as code from the dynamic library
 *		 is executed.
 * RTLD_NOW    - resolve all undefined symbols before dlopen returns, and fail
 *		 if this cannot be done.
 * Optionally:
 * RTLD_GLOBAL - the external symbols defined in the library will be made
 *		 available to subsequently loaded libraries.
 */
#ifndef	RTLD_GLOBAL
#define	RTLD_GLOBAL	0
#endif	/* RTLD_GLOBAL */
#ifndef	RTLD_LAZY
#define	RTLD_LAZY	1
#endif	/* RTLD_LAZY */
#ifndef	RTLD_NOW
#define	RTLD_NOW	0
#endif	/* RTLD_NOW */


/* --- functions --- */
static gpointer
_g_module_open (const gchar    *file_name,
		gboolean	bind_lazy,
		gboolean	bind_local)
{
  gpointer handle;
  
  handle = dlopen (file_name,
	(bind_local ? 0 : RTLD_GLOBAL) | (bind_lazy ? RTLD_LAZY : RTLD_NOW));
  if (!handle)
    g_module_set_error (dlerror ());
  
  return handle;
}

static gpointer
_g_module_self (void)
{
  gpointer handle;
  
  /* to query symbols from the program itself, special link options
   * are required on some systems.
   */
  
  /* XXX, not supported */
  handle = NULL;
  g_module_set_error ("module handle for self not supported");
  
  return handle;
}

static void
_g_module_close (gpointer handle,
		 gboolean is_unref)
{
  /* are there any systems out there that have dlopen()/dlclose()
   * without a reference count implementation?
   */
  is_unref |= 1;
  
  if (is_unref)
    {
      /* XXX, no return code */
      dlclose (handle);
    }
}

static gpointer
_g_module_symbol (gpointer     handle,
		  const gchar *symbol_name)
{
  gpointer p;
  
  p = dlsym (handle, symbol_name);
  if (!p)
    g_module_set_error (dlerror ());
  
  return p;
}

static gchar*
_g_module_build_path (const gchar *directory,
		      const gchar *module_name)
{
  gchar *suffix = strrchr(module_name, '.');
  if (directory && *directory)
    if (suffix && (stricmp (suffix, ".dll") == 0))
      return g_strconcat (directory, "/", module_name, NULL);
    else
      return g_strconcat (directory, "/", module_name, ".dll", NULL);
  else if (suffix && (stricmp (suffix, ".dll") == 0))
    return g_strdup (module_name);
  else
    return g_strconcat (module_name, ".dll", NULL);
}
