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
 */

#include "config.h"
#ifdef HAVE_SPLICE
#define _GNU_SOURCE
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#endif
#include <string.h>
#include <sys/types.h>
#ifdef HAVE_PWD_H
#include <pwd.h>
#endif
#include "gfile.h"
#include "gvfs.h"
#include "gioscheduler.h"
#include "gsimpleasyncresult.h"
#include "gfileattribute-priv.h"
#include "gfiledescriptorbased.h"
#include "gpollfilemonitor.h"
#include "gappinfo.h"
#include "gfileinputstream.h"
#include "gfileoutputstream.h"
#include "gcancellable.h"
#include "gasyncresult.h"
#include "gioerror.h"
#include "glibintl.h"

#include "gioalias.h"

/**
 * SECTION:gfile
 * @short_description: File and Directory Handling
 * @include: gio/gio.h
 * @see_also: #GFileInfo, #GFileEnumerator
 * 
 * #GFile is a high level abstraction for manipulating files on a 
 * virtual file system. #GFile<!-- -->s are lightweight, immutable 
 * objects that do no I/O upon creation. It is necessary to understand that
 * #GFile objects do not represent files, merely an identifier for a file. All
 * file content I/O is implemented as streaming operations (see #GInputStream and 
 * #GOutputStream).
 *
 * To construct a #GFile, you can use: 
 * g_file_new_for_path() if you have a path.
 * g_file_new_for_uri() if you have a URI.
 * g_file_new_for_commandline_arg() for a command line argument.
 * g_file_parse_name() from a utf8 string gotten from g_file_get_parse_name().
 * 
 * One way to think of a #GFile is as an abstraction of a pathname. For normal
 * files the system pathname is what is stored internally, but as #GFile<!-- -->s
 * are extensible it could also be something else that corresponds to a pathname
 * in a userspace implementation of a filesystem.
 *
 * #GFile<!-- -->s make up hierarchies of directories and files that correspond to the
 * files on a filesystem. You can move through the file system with #GFile using
 * g_file_get_parent() to get an identifier for the parent directory, g_file_get_child()
 * to get a child within a directory, g_file_resolve_relative_path() to resolve a relative
 * path between two #GFile<!-- -->s. There can be multiple hierarchies, so you may not
 * end up at the same root if you repeatedly call g_file_get_parent() on two different
 * files.
 *
 * All #GFile<!-- -->s have a basename (get with g_file_get_basename()). These names
 * are byte strings that are used to identify the file on the filesystem (relative to
 * its parent directory) and there is no guarantees that they have any particular charset
 * encoding or even make any sense at all. If you want to use filenames in a user
 * interface you should use the display name that you can get by requesting the
 * %G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME attribute with g_file_query_info().
 * This is guaranteed to be in utf8 and can be used in a user interface. But always
 * store the real basename or the #GFile to use to actually access the file, because
 * there is no way to go from a display name to the actual name.
 *
 * Using #GFile as an identifier has the same weaknesses as using a path in that
 * there may be multiple aliases for the same file. For instance, hard or
 * soft links may cause two different #GFile<!-- -->s to refer to the same file.
 * Other possible causes for aliases are: case insensitive filesystems, short
 * and long names on Fat/NTFS, or bind mounts in Linux. If you want to check if
 * two #GFile<!-- -->s point to the same file you can query for the
 * %G_FILE_ATTRIBUTE_ID_FILE attribute. Note that #GFile does some trivial
 * canonicalization of pathnames passed in, so that trivial differences in the
 * path string used at creation (duplicated slashes, slash at end of path, "."
 * or ".." path segments, etc) does not create different #GFile<!-- -->s.
 * 
 * Many #GFile operations have both synchronous and asynchronous versions 
 * to suit your application. Asynchronous versions of synchronous functions 
 * simply have _async() appended to their function names. The asynchronous 
 * I/O functions call a #GAsyncReadyCallback which is then used to finalize 
 * the operation, producing a GAsyncResult which is then passed to the 
 * function's matching _finish() operation. 
 *
 * Some #GFile operations do not have synchronous analogs, as they may
 * take a very long time to finish, and blocking may leave an application
 * unusable. Notable cases include:
 * g_file_mount_mountable() to mount a mountable file.
 * g_file_unmount_mountable_with_operation() to unmount a mountable file.
 * g_file_eject_mountable_with_operation() to eject a mountable file.
 * 
 * <para id="gfile-etag"><indexterm><primary>entity tag</primary></indexterm>
 * One notable feature of #GFile<!-- -->s are entity tags, or "etags" for 
 * short. Entity tags are somewhat like a more abstract version of the 
 * traditional mtime, and can be used to quickly determine if the file has
 * been modified from the version on the file system. See the HTTP 1.1 
 * <ulink url="http://www.w3.org/Protocols/rfc2616/rfc2616-sec14.html">specification</ulink>
 * for HTTP Etag headers, which are a very similar concept.
 * </para>
 **/

static void               g_file_real_query_info_async            (GFile                  *file,
								   const char             *attributes,
								   GFileQueryInfoFlags     flags,
								   int                     io_priority,
								   GCancellable           *cancellable,
								   GAsyncReadyCallback     callback,
								   gpointer                user_data);
static GFileInfo *        g_file_real_query_info_finish           (GFile                  *file,
								   GAsyncResult           *res,
								   GError                **error);
static void               g_file_real_query_filesystem_info_async (GFile                  *file,
								   const char             *attributes,
								   int                     io_priority,
								   GCancellable           *cancellable,
								   GAsyncReadyCallback     callback,
								   gpointer                user_data);
static GFileInfo *        g_file_real_query_filesystem_info_finish (GFile                  *file,
								   GAsyncResult           *res,
								   GError                **error);
static void               g_file_real_enumerate_children_async    (GFile                  *file,
								   const char             *attributes,
								   GFileQueryInfoFlags     flags,
								   int                     io_priority,
								   GCancellable           *cancellable,
								   GAsyncReadyCallback     callback,
								   gpointer                user_data);
static GFileEnumerator *  g_file_real_enumerate_children_finish   (GFile                  *file,
								   GAsyncResult           *res,
								   GError                **error);
static void               g_file_real_read_async                  (GFile                  *file,
								   int                     io_priority,
								   GCancellable           *cancellable,
								   GAsyncReadyCallback     callback,
								   gpointer                user_data);
static GFileInputStream * g_file_real_read_finish                 (GFile                  *file,
								   GAsyncResult           *res,
								   GError                **error);
static void               g_file_real_append_to_async             (GFile                  *file,
								   GFileCreateFlags        flags,
								   int                     io_priority,
								   GCancellable           *cancellable,
								   GAsyncReadyCallback     callback,
								   gpointer                user_data);
static GFileOutputStream *g_file_real_append_to_finish            (GFile                  *file,
								   GAsyncResult           *res,
								   GError                **error);
static void               g_file_real_create_async                (GFile                  *file,
								   GFileCreateFlags        flags,
								   int                     io_priority,
								   GCancellable           *cancellable,
								   GAsyncReadyCallback     callback,
								   gpointer                user_data);
static GFileOutputStream *g_file_real_create_finish               (GFile                  *file,
								   GAsyncResult           *res,
								   GError                **error);
static void               g_file_real_replace_async               (GFile                  *file,
								   const char             *etag,
								   gboolean                make_backup,
								   GFileCreateFlags        flags,
								   int                     io_priority,
								   GCancellable           *cancellable,
								   GAsyncReadyCallback     callback,
								   gpointer                user_data);
static GFileOutputStream *g_file_real_replace_finish              (GFile                  *file,
								   GAsyncResult           *res,
								   GError                **error);
static void               g_file_real_open_readwrite_async        (GFile                  *file,
                                                                   int                  io_priority,
                                                                   GCancellable           *cancellable,
                                                                   GAsyncReadyCallback     callback,
                                                                   gpointer                user_data);
static GFileIOStream *    g_file_real_open_readwrite_finish       (GFile                  *file,
                                                                   GAsyncResult           *res,
                                                                   GError                **error);
static void               g_file_real_create_readwrite_async      (GFile                  *file,
                                                                   GFileCreateFlags        flags,
                                                                   int                     io_priority,
                                                                   GCancellable           *cancellable,
                                                                   GAsyncReadyCallback     callback,
                                                                   gpointer                user_data);
static GFileIOStream *    g_file_real_create_readwrite_finish     (GFile                  *file,
                                                                   GAsyncResult           *res,
                                                                   GError                **error);
static void               g_file_real_replace_readwrite_async     (GFile                  *file,
                                                                   const char             *etag,
                                                                   gboolean                make_backup,
                                                                   GFileCreateFlags        flags,
                                                                   int                     io_priority,
                                                                   GCancellable           *cancellable,
                                                                   GAsyncReadyCallback     callback,
                                                                   gpointer                user_data);
static GFileIOStream *    g_file_real_replace_readwrite_finish    (GFile                  *file,
                                                                  GAsyncResult            *res,
                                                                  GError                 **error);
static gboolean           g_file_real_set_attributes_from_info    (GFile                  *file,
								   GFileInfo              *info,
								   GFileQueryInfoFlags     flags,
								   GCancellable           *cancellable,
								   GError                **error);
static void               g_file_real_set_display_name_async      (GFile                  *file,
								   const char             *display_name,
								   int                     io_priority,
								   GCancellable           *cancellable,
								   GAsyncReadyCallback     callback,
								   gpointer                user_data);
static GFile *            g_file_real_set_display_name_finish     (GFile                  *file,
								   GAsyncResult           *res,
								   GError                **error);
static void               g_file_real_set_attributes_async        (GFile                  *file,
								   GFileInfo              *info,
								   GFileQueryInfoFlags     flags,
								   int                     io_priority,
								   GCancellable           *cancellable,
								   GAsyncReadyCallback     callback,
								   gpointer                user_data);
static gboolean           g_file_real_set_attributes_finish       (GFile                  *file,
								   GAsyncResult           *res,
								   GFileInfo             **info,
								   GError                **error);
static void               g_file_real_find_enclosing_mount_async  (GFile                  *file,
								   int                     io_priority,
								   GCancellable           *cancellable,
								   GAsyncReadyCallback     callback,
								   gpointer                user_data);
static GMount *           g_file_real_find_enclosing_mount_finish (GFile                  *file,
								   GAsyncResult           *res,
								   GError                **error);
static void               g_file_real_copy_async                  (GFile                  *source,
								   GFile                  *destination,
								   GFileCopyFlags          flags,
								   int                     io_priority,
								   GCancellable           *cancellable,
								   GFileProgressCallback   progress_callback,
								   gpointer                progress_callback_data,
								   GAsyncReadyCallback     callback,
								   gpointer                user_data);
static gboolean           g_file_real_copy_finish                 (GFile                  *file,
								   GAsyncResult           *res,
								   GError                **error);

typedef GFileIface GFileInterface;
G_DEFINE_INTERFACE (GFile, g_file, G_TYPE_OBJECT)

static void
g_file_default_init (GFileIface *iface)
{
  iface->enumerate_children_async = g_file_real_enumerate_children_async;
  iface->enumerate_children_finish = g_file_real_enumerate_children_finish;
  iface->set_display_name_async = g_file_real_set_display_name_async;
  iface->set_display_name_finish = g_file_real_set_display_name_finish;
  iface->query_info_async = g_file_real_query_info_async;
  iface->query_info_finish = g_file_real_query_info_finish;
  iface->query_filesystem_info_async = g_file_real_query_filesystem_info_async;
  iface->query_filesystem_info_finish = g_file_real_query_filesystem_info_finish;
  iface->set_attributes_async = g_file_real_set_attributes_async;
  iface->set_attributes_finish = g_file_real_set_attributes_finish;
  iface->read_async = g_file_real_read_async;
  iface->read_finish = g_file_real_read_finish;
  iface->append_to_async = g_file_real_append_to_async;
  iface->append_to_finish = g_file_real_append_to_finish;
  iface->create_async = g_file_real_create_async;
  iface->create_finish = g_file_real_create_finish;
  iface->replace_async = g_file_real_replace_async;
  iface->replace_finish = g_file_real_replace_finish;
  iface->open_readwrite_async = g_file_real_open_readwrite_async;
  iface->open_readwrite_finish = g_file_real_open_readwrite_finish;
  iface->create_readwrite_async = g_file_real_create_readwrite_async;
  iface->create_readwrite_finish = g_file_real_create_readwrite_finish;
  iface->replace_readwrite_async = g_file_real_replace_readwrite_async;
  iface->replace_readwrite_finish = g_file_real_replace_readwrite_finish;
  iface->find_enclosing_mount_async = g_file_real_find_enclosing_mount_async;
  iface->find_enclosing_mount_finish = g_file_real_find_enclosing_mount_finish;
  iface->set_attributes_from_info = g_file_real_set_attributes_from_info;
  iface->copy_async = g_file_real_copy_async;
  iface->copy_finish = g_file_real_copy_finish;
}


/**
 * g_file_is_native:
 * @file: input #GFile.
 *
 * Checks to see if a file is native to the platform.
 *
 * A native file s one expressed in the platform-native filename format,
 * e.g. "C:\Windows" or "/usr/bin/". This does not mean the file is local,
 * as it might be on a locally mounted remote filesystem.
 *
 * On some systems non-native files may be available using
 * the native filesystem via a userspace filesystem (FUSE), in
 * these cases this call will return %FALSE, but g_file_get_path()
 * will still return a native path.
 *
 * This call does no blocking i/o.
 * 
 * Returns: %TRUE if file is native. 
 **/
gboolean
g_file_is_native (GFile *file)
{
  GFileIface *iface;

  g_return_val_if_fail (G_IS_FILE (file), FALSE);

  iface = G_FILE_GET_IFACE (file);

  return (* iface->is_native) (file);
}


/**
 * g_file_has_uri_scheme: 
 * @file: input #GFile.
 * @uri_scheme: a string containing a URI scheme.
 *
 * Checks to see if a #GFile has a given URI scheme.
 *
 * This call does no blocking i/o.
 * 
 * Returns: %TRUE if #GFile's backend supports the
 *     given URI scheme, %FALSE if URI scheme is %NULL,
 *     not supported, or #GFile is invalid.
 **/
gboolean
g_file_has_uri_scheme (GFile      *file,
		       const char *uri_scheme)
{
  GFileIface *iface;
  
  g_return_val_if_fail (G_IS_FILE (file), FALSE);
  g_return_val_if_fail (uri_scheme != NULL, FALSE);

  iface = G_FILE_GET_IFACE (file);

  return (* iface->has_uri_scheme) (file, uri_scheme);
}


/**
 * g_file_get_uri_scheme:
 * @file: input #GFile.
 *
 * Gets the URI scheme for a #GFile.
 * RFC 3986 decodes the scheme as:
 * <programlisting>
 * URI = scheme ":" hier-part [ "?" query ] [ "#" fragment ] 
 * </programlisting>
 * Common schemes include "file", "http", "ftp", etc. 
 *
 * This call does no blocking i/o.
 * 
 * Returns: a string containing the URI scheme for the given 
 *     #GFile. The returned string should be freed with g_free() 
 *     when no longer needed.
 **/
char *
g_file_get_uri_scheme (GFile *file)
{
  GFileIface *iface;

  g_return_val_if_fail (G_IS_FILE (file), NULL);

  iface = G_FILE_GET_IFACE (file);

  return (* iface->get_uri_scheme) (file);
}


/**
 * g_file_get_basename:
 * @file: input #GFile.
 *
 * Gets the base name (the last component of the path) for a given #GFile.
 *
 * If called for the top level of a system (such as the filesystem root
 * or a uri like sftp://host/) it will return a single directory separator
 * (and on Windows, possibly a drive letter).
 *
 * The base name is a byte string (*not* UTF-8). It has no defined encoding
 * or rules other than it may not contain zero bytes.  If you want to use
 * filenames in a user interface you should use the display name that you
 * can get by requesting the %G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME
 * attribute with g_file_query_info().
 * 
 * This call does no blocking i/o.
 * 
 * Returns: string containing the #GFile's base name, or %NULL 
 *     if given #GFile is invalid. The returned string should be 
 *     freed with g_free() when no longer needed.
 **/
char *
g_file_get_basename (GFile *file)
{
  GFileIface *iface;
  
  g_return_val_if_fail (G_IS_FILE (file), NULL);

  iface = G_FILE_GET_IFACE (file);

  return (* iface->get_basename) (file);
}

/**
 * g_file_get_path:
 * @file: input #GFile.
 *
 * Gets the local pathname for #GFile, if one exists. 
 *
 * This call does no blocking i/o.
 * 
 * Returns: string containing the #GFile's path, or %NULL if 
 *     no such path exists. The returned string should be 
 *     freed with g_free() when no longer needed.
 **/
char *
g_file_get_path (GFile *file)
{
  GFileIface *iface;

  g_return_val_if_fail (G_IS_FILE (file), NULL);

  iface = G_FILE_GET_IFACE (file);

  return (* iface->get_path) (file);
}

/**
 * g_file_get_uri:
 * @file: input #GFile.
 *
 * Gets the URI for the @file.
 *
 * This call does no blocking i/o.
 * 
 * Returns: a string containing the #GFile's URI.
 *     The returned string should be freed with g_free() when no longer needed.
 **/
char *
g_file_get_uri (GFile *file)
{
  GFileIface *iface;
  
  g_return_val_if_fail (G_IS_FILE (file), NULL);

  iface = G_FILE_GET_IFACE (file);

  return (* iface->get_uri) (file);
}

/**
 * g_file_get_parse_name:
 * @file: input #GFile.
 *
 * Gets the parse name of the @file.
 * A parse name is a UTF-8 string that describes the
 * file such that one can get the #GFile back using
 * g_file_parse_name().
 *
 * This is generally used to show the #GFile as a nice
 * full-pathname kind of string in a user interface,
 * like in a location entry.
 *
 * For local files with names that can safely be converted
 * to UTF8 the pathname is used, otherwise the IRI is used
 * (a form of URI that allows UTF8 characters unescaped).
 *
 * This call does no blocking i/o.
 * 
 * Returns: a string containing the #GFile's parse name. The returned 
 *     string should be freed with g_free() when no longer needed.
 **/
char *
g_file_get_parse_name (GFile *file)
{
  GFileIface *iface;
  
  g_return_val_if_fail (G_IS_FILE (file), NULL);

  iface = G_FILE_GET_IFACE (file);

  return (* iface->get_parse_name) (file);
}

/**
 * g_file_dup:
 * @file: input #GFile.
 *
 * Duplicates a #GFile handle. This operation does not duplicate 
 * the actual file or directory represented by the #GFile; see 
 * g_file_copy() if attempting to copy a file. 
 *
 * This call does no blocking i/o.
 * 
 * Returns: a new #GFile that is a duplicate of the given #GFile. 
 **/
GFile *
g_file_dup (GFile *file)
{
  GFileIface *iface;
  
  g_return_val_if_fail (G_IS_FILE (file), NULL);

  iface = G_FILE_GET_IFACE (file);

  return (* iface->dup) (file);
}

/**
 * g_file_hash:
 * @file: #gconstpointer to a #GFile.
 *
 * Creates a hash value for a #GFile.
 *
 * This call does no blocking i/o.
 * 
 * Returns: 0 if @file is not a valid #GFile, otherwise an 
 *     integer that can be used as hash value for the #GFile. 
 *     This function is intended for easily hashing a #GFile to 
 *     add to a #GHashTable or similar data structure.
 **/
guint
g_file_hash (gconstpointer file)
{
  GFileIface *iface;
  
  g_return_val_if_fail (G_IS_FILE (file), 0);

  iface = G_FILE_GET_IFACE (file);

  return (* iface->hash) ((GFile *)file);
}

/**
 * g_file_equal:
 * @file1: the first #GFile.
 * @file2: the second #GFile.
 *
 * Checks equality of two given #GFile<!-- -->s. Note that two
 * #GFile<!-- -->s that differ can still refer to the same
 * file on the filesystem due to various forms of filename
 * aliasing.
 *
 * This call does no blocking i/o.
 * 
 * Returns: %TRUE if @file1 and @file2 are equal.
 *     %FALSE if either is not a #GFile.
 **/
gboolean
g_file_equal (GFile *file1,
	      GFile *file2)
{
  GFileIface *iface;
  
  g_return_val_if_fail (G_IS_FILE (file1), FALSE);
  g_return_val_if_fail (G_IS_FILE (file2), FALSE);
  
  if (G_TYPE_FROM_INSTANCE (file1) != G_TYPE_FROM_INSTANCE (file2))
    return FALSE;

  iface = G_FILE_GET_IFACE (file1);
  
  return (* iface->equal) (file1, file2);
}


/**
 * g_file_get_parent:
 * @file: input #GFile.
 *
 * Gets the parent directory for the @file. 
 * If the @file represents the root directory of the 
 * file system, then %NULL will be returned.
 *
 * This call does no blocking i/o.
 * 
 * Returns: a #GFile structure to the parent of the given
 *     #GFile or %NULL if there is no parent. 
 *     Free the returned object with g_object_unref().
 **/
GFile *
g_file_get_parent (GFile *file)
{
  GFileIface *iface;
  
  g_return_val_if_fail (G_IS_FILE (file), NULL);

  iface = G_FILE_GET_IFACE (file);

  return (* iface->get_parent) (file);
}

/**
 * g_file_has_parent:
 * @file: input #GFile
 * @parent: the parent to check for, or %NULL
 *
 * Checks if @file has a parent, and optionally, if it is @parent.
 *
 * If @parent is %NULL then this function returns %TRUE if @file has any
 * parent at all.  If @parent is non-%NULL then %TRUE is only returned
 * if @file is a child of @parent.
 *
 * Returns: %TRUE if @file is a child of @parent (or any parent in the
 *          case that @parent is %NULL).
 *
 * Since: 2.24
 **/
gboolean
g_file_has_parent (GFile *file,
                   GFile *parent)
{
  GFile *actual_parent;
  gboolean result;

  g_return_val_if_fail (G_IS_FILE (file), FALSE);
  g_return_val_if_fail (parent == NULL || G_IS_FILE (parent), FALSE);

  actual_parent = g_file_get_parent (file);

  if (actual_parent != NULL)
    {
      if (parent != NULL)
        result = g_file_equal (parent, actual_parent);
      else
        result = TRUE;

      g_object_unref (actual_parent);
    }
  else
    result = FALSE;

  return result;
}

/**
 * g_file_get_child:
 * @file: input #GFile.
 * @name: string containing the child's basename.
 *
 * Gets a child of @file with basename equal to @name.
 *
 * Note that the file with that specific name might not exist, but
 * you can still have a #GFile that points to it. You can use this
 * for instance to create that file.
 *
 * This call does no blocking i/o.
 * 
 * Returns: a #GFile to a child specified by @name.
 *     Free the returned object with g_object_unref().
 **/
GFile *
g_file_get_child (GFile      *file,
		  const char *name)
{
  g_return_val_if_fail (G_IS_FILE (file), NULL);
  g_return_val_if_fail (name != NULL, NULL);

  return g_file_resolve_relative_path (file, name);
}

/**
 * g_file_get_child_for_display_name:
 * @file: input #GFile.
 * @display_name: string to a possible child.
 * @error: #GError.
 *
 * Gets the child of @file for a given @display_name (i.e. a UTF8
 * version of the name). If this function fails, it returns %NULL and @error will be 
 * set. This is very useful when constructing a GFile for a new file
 * and the user entered the filename in the user interface, for instance
 * when you select a directory and type a filename in the file selector.
 * 
 * This call does no blocking i/o.
 * 
 * Returns: a #GFile to the specified child, or 
 *     %NULL if the display name couldn't be converted.  
 *     Free the returned object with g_object_unref().
 **/
GFile *
g_file_get_child_for_display_name (GFile      *file,
				   const char *display_name,
				   GError **error)
{
  GFileIface *iface;
  
  g_return_val_if_fail (G_IS_FILE (file), NULL);
  g_return_val_if_fail (display_name != NULL, NULL);

  iface = G_FILE_GET_IFACE (file);

  return (* iface->get_child_for_display_name) (file, display_name, error);
}

/**
 * g_file_has_prefix:
 * @file: input #GFile.
 * @prefix: input #GFile.
 * 
 * Checks whether @file has the prefix specified by @prefix. In other word, 
 * if the names of inital elements of @file<!-- -->s pathname match @prefix.
 * Only full pathname elements are matched, so a path like /foo is not
 * considered a prefix of /foobar, only of /foo/bar.
 * 
 * This call does no i/o, as it works purely on names. As such it can 
 * sometimes return %FALSE even if @file is inside a @prefix (from a 
 * filesystem point of view), because the prefix of @file is an alias 
 * of @prefix.
 *
 * Returns:  %TRUE if the @files's parent, grandparent, etc is @prefix. 
 *     %FALSE otherwise.
 **/
gboolean
g_file_has_prefix (GFile *file,
		   GFile *prefix)
{
  GFileIface *iface;
  
  g_return_val_if_fail (G_IS_FILE (file), FALSE);
  g_return_val_if_fail (G_IS_FILE (prefix), FALSE);

  if (G_TYPE_FROM_INSTANCE (file) != G_TYPE_FROM_INSTANCE (prefix))
    return FALSE;
  
  iface = G_FILE_GET_IFACE (file);

  /* The vtable function differs in arg order since we're
     using the old contains_file call */
  return (* iface->prefix_matches) (prefix, file);
}

/**
 * g_file_get_relative_path:
 * @parent: input #GFile.
 * @descendant: input #GFile.
 *
 * Gets the path for @descendant relative to @parent. 
 *
 * This call does no blocking i/o.
 * 
 * Returns: string with the relative path from @descendant 
 *     to @parent, or %NULL if @descendant doesn't have @parent as prefix. 
 *     The returned string should be freed with g_free() when no longer needed.
 **/
char *
g_file_get_relative_path (GFile *parent,
			  GFile *descendant)
{
  GFileIface *iface;
  
  g_return_val_if_fail (G_IS_FILE (parent), NULL);
  g_return_val_if_fail (G_IS_FILE (descendant), NULL);

  if (G_TYPE_FROM_INSTANCE (parent) != G_TYPE_FROM_INSTANCE (descendant))
    return NULL;
  
  iface = G_FILE_GET_IFACE (parent);

  return (* iface->get_relative_path) (parent, descendant);
}

/**
 * g_file_resolve_relative_path:
 * @file: input #GFile.
 * @relative_path: a given relative path string.
 *
 * Resolves a relative path for @file to an absolute path.
 *
 * This call does no blocking i/o.
 * 
 * Returns: #GFile to the resolved path. %NULL if @relative_path 
 *     is %NULL or if @file is invalid.
 *     Free the returned object with g_object_unref().
 **/
GFile *
g_file_resolve_relative_path (GFile      *file,
			      const char *relative_path)
{
  GFileIface *iface;

  g_return_val_if_fail (G_IS_FILE (file), NULL);
  g_return_val_if_fail (relative_path != NULL, NULL);

  iface = G_FILE_GET_IFACE (file);

  return (* iface->resolve_relative_path) (file, relative_path);
}

/**
 * g_file_enumerate_children:
 * @file: input #GFile.
 * @attributes: an attribute query string.
 * @flags: a set of #GFileQueryInfoFlags.
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * @error: #GError for error reporting.
 *
 * Gets the requested information about the files in a directory. The result
 * is a #GFileEnumerator object that will give out #GFileInfo objects for
 * all the files in the directory.
 *
 * The @attribute value is a string that specifies the file attributes that
 * should be gathered. It is not an error if it's not possible to read a particular
 * requested attribute from a file - it just won't be set. @attribute should
 * be a comma-separated list of attribute or attribute wildcards. The wildcard "*"
 * means all attributes, and a wildcard like "standard::*" means all attributes in the standard
 * namespace. An example attribute query be "standard::*,owner::user".
 * The standard attributes are available as defines, like #G_FILE_ATTRIBUTE_STANDARD_NAME.
 *
 * If @cancellable is not %NULL, then the operation can be cancelled by
 * triggering the cancellable object from another thread. If the operation
 * was cancelled, the error %G_IO_ERROR_CANCELLED will be returned. 
 * 
 * If the file does not exist, the G_IO_ERROR_NOT_FOUND error will be returned.
 * If the file is not a directory, the G_FILE_ERROR_NOTDIR error will be returned.
 * Other errors are possible too.
 *
 * Returns: A #GFileEnumerator if successful, %NULL on error. 
 *     Free the returned object with g_object_unref().
 **/
GFileEnumerator *
g_file_enumerate_children (GFile                *file,
			   const char           *attributes,
			   GFileQueryInfoFlags   flags,
			   GCancellable         *cancellable,
			   GError              **error)
			   
{
  GFileIface *iface;
  
  g_return_val_if_fail (G_IS_FILE (file), NULL);

  if (g_cancellable_set_error_if_cancelled (cancellable, error))
    return NULL;
  
  iface = G_FILE_GET_IFACE (file);

  if (iface->enumerate_children == NULL)
    {
      g_set_error_literal (error, G_IO_ERROR,
                           G_IO_ERROR_NOT_SUPPORTED,
                           _("Operation not supported"));
      return NULL;
    }

  return (* iface->enumerate_children) (file, attributes, flags,
					cancellable, error);
}

/**
 * g_file_enumerate_children_async:
 * @file: input #GFile.
 * @attributes: an attribute query string.
 * @flags: a set of #GFileQueryInfoFlags.
 * @io_priority: the <link linkend="io-priority">I/O priority</link> 
 *     of the request.
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * @callback: a #GAsyncReadyCallback to call when the request is satisfied
 * @user_data: the data to pass to callback function
 *
 * Asynchronously gets the requested information about the files in a directory. The result
 * is a #GFileEnumerator object that will give out #GFileInfo objects for
 * all the files in the directory.
 *
 * For more details, see g_file_enumerate_children() which is
 * the synchronous version of this call.
 *
 * When the operation is finished, @callback will be called. You can then call
 * g_file_enumerate_children_finish() to get the result of the operation.
 **/
void
g_file_enumerate_children_async (GFile               *file,
				 const char          *attributes,
				 GFileQueryInfoFlags  flags,
				 int                  io_priority,
				 GCancellable        *cancellable,
				 GAsyncReadyCallback  callback,
				 gpointer             user_data)
{
  GFileIface *iface;

  g_return_if_fail (G_IS_FILE (file));

  iface = G_FILE_GET_IFACE (file);
  (* iface->enumerate_children_async) (file,
				       attributes,
				       flags,
				       io_priority,
				       cancellable,
				       callback,
				       user_data);
}

/**
 * g_file_enumerate_children_finish:
 * @file: input #GFile.
 * @res: a #GAsyncResult.
 * @error: a #GError.
 * 
 * Finishes an async enumerate children operation.
 * See g_file_enumerate_children_async().
 *
 * Returns: a #GFileEnumerator or %NULL if an error occurred.
 *     Free the returned object with g_object_unref().
 **/
GFileEnumerator *
g_file_enumerate_children_finish (GFile         *file,
				  GAsyncResult  *res,
				  GError       **error)
{
  GFileIface *iface;
  
  g_return_val_if_fail (G_IS_FILE (file), NULL);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (res), NULL);

  if (G_IS_SIMPLE_ASYNC_RESULT (res))
    {
      GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (res);
      if (g_simple_async_result_propagate_error (simple, error))
	return NULL;
    }
  
  iface = G_FILE_GET_IFACE (file);
  return (* iface->enumerate_children_finish) (file, res, error);
}

/**
 * g_file_query_exists:
 * @file: input #GFile.
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 *
 * Utility function to check if a particular file exists. This is
 * implemented using g_file_query_info() and as such does blocking I/O.
 *
 * Note that in many cases it is racy to first check for file existence
 * and then execute something based on the outcome of that, because the
 * file might have been created or removed in between the operations. The
 * general approach to handling that is to not check, but just do the
 * operation and handle the errors as they come.
 *
 * As an example of race-free checking, take the case of reading a file, and
 * if it doesn't exist, creating it. There are two racy versions: read it, and
 * on error create it; and: check if it exists, if not create it. These
 * can both result in two processes creating the file (with perhaps a partially
 * written file as the result). The correct approach is to always try to create
 * the file with g_file_create() which will either atomically create the file
 * or fail with a G_IO_ERROR_EXISTS error.
 *
 * However, in many cases an existence check is useful in a user
 * interface, for instance to make a menu item sensitive/insensitive, so that
 * you don't have to fool users that something is possible and then just show
 * and error dialog. If you do this, you should make sure to also handle the
 * errors that can happen due to races when you execute the operation.
 * 
 * Returns: %TRUE if the file exists (and can be detected without error), %FALSE otherwise (or if cancelled).
 */
gboolean
g_file_query_exists (GFile *file,
		     GCancellable *cancellable)
{
  GFileInfo *info;
  
  g_return_val_if_fail (G_IS_FILE(file), FALSE);
  
  info = g_file_query_info (file, G_FILE_ATTRIBUTE_STANDARD_TYPE,
			    G_FILE_QUERY_INFO_NONE, cancellable, NULL);
  if (info != NULL)
    {
      g_object_unref (info);
      return TRUE;
    }
  
  return FALSE;
}

/**
 * g_file_query_file_type:
 * @file: input #GFile.
 * @flags: a set of #GFileQueryInfoFlags passed to g_file_query_info().
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 *
 * Utility function to inspect the #GFileType of a file. This is
 * implemented using g_file_query_info() and as such does blocking I/O.
 *
 * The primary use case of this method is to check if a file is a regular file,
 * directory, or symlink.
 * 
 * Returns: The #GFileType of the file and #G_FILE_TYPE_UNKNOWN if the file
 *          does not exist
 *
 * Since: 2.18
 */
GFileType
g_file_query_file_type (GFile *file,
                        GFileQueryInfoFlags   flags,
		 	GCancellable *cancellable)
{
  GFileInfo *info;
  GFileType file_type;
  
  g_return_val_if_fail (G_IS_FILE(file), G_FILE_TYPE_UNKNOWN);
  info = g_file_query_info (file, G_FILE_ATTRIBUTE_STANDARD_TYPE, flags,
			    cancellable, NULL);
  if (info != NULL)
    {
      file_type = g_file_info_get_file_type (info);
      g_object_unref (info);
    }
  else
    file_type = G_FILE_TYPE_UNKNOWN;
  
  return file_type;
}

/**
 * g_file_query_info:
 * @file: input #GFile.
 * @attributes: an attribute query string.
 * @flags: a set of #GFileQueryInfoFlags.
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * @error: a #GError.
 *
 * Gets the requested information about specified @file. The result
 * is a #GFileInfo object that contains key-value attributes (such as 
 * the type or size of the file).
 *
 * The @attribute value is a string that specifies the file attributes that
 * should be gathered. It is not an error if it's not possible to read a particular
 * requested attribute from a file - it just won't be set. @attribute should
 * be a comma-separated list of attribute or attribute wildcards. The wildcard "*"
 * means all attributes, and a wildcard like "standard::*" means all attributes in the standard
 * namespace. An example attribute query be "standard::*,owner::user".
 * The standard attributes are available as defines, like #G_FILE_ATTRIBUTE_STANDARD_NAME.
 * 
 * If @cancellable is not %NULL, then the operation can be cancelled by
 * triggering the cancellable object from another thread. If the operation
 * was cancelled, the error %G_IO_ERROR_CANCELLED will be returned. 
 *
 * For symlinks, normally the information about the target of the
 * symlink is returned, rather than information about the symlink itself.
 * However if you pass #G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS in @flags the
 * information about the symlink itself will be returned. Also, for symlinks
 * that point to non-existing files the information about the symlink itself
 * will be returned.
 *
 * If the file does not exist, the G_IO_ERROR_NOT_FOUND error will be returned.
 * Other errors are possible too, and depend on what kind of filesystem the file is on.
 *
 * Returns: a #GFileInfo for the given @file, or %NULL on error.
 *     Free the returned object with g_object_unref().
 **/
GFileInfo *
g_file_query_info (GFile                *file,
		   const char           *attributes,
		   GFileQueryInfoFlags   flags,
		   GCancellable         *cancellable,
		   GError              **error)
{
  GFileIface *iface;
  
  g_return_val_if_fail (G_IS_FILE (file), NULL);

  if (g_cancellable_set_error_if_cancelled (cancellable, error))
    return NULL;
  
  iface = G_FILE_GET_IFACE (file);

  if (iface->query_info == NULL)
    {
      g_set_error_literal (error, G_IO_ERROR,
                           G_IO_ERROR_NOT_SUPPORTED,
                           _("Operation not supported"));
      return NULL;
    }
  
  return (* iface->query_info) (file, attributes, flags, cancellable, error);
}

/**
 * g_file_query_info_async:
 * @file: input #GFile.
 * @attributes: an attribute query string.
 * @flags: a set of #GFileQueryInfoFlags.
 * @io_priority: the <link linkend="io-priority">I/O priority</link> 
 *     of the request.
 * @cancellable: optional #GCancellable object, %NULL to ignore. 
 * @callback: a #GAsyncReadyCallback to call when the request is satisfied
 * @user_data: the data to pass to callback function
 * 
 * Asynchronously gets the requested information about specified @file. The result
 * is a #GFileInfo object that contains key-value attributes (such as type or size
 * for the file).
 * 
 * For more details, see g_file_query_info() which is
 * the synchronous version of this call.
 *
 * When the operation is finished, @callback will be called. You can then call
 * g_file_query_info_finish() to get the result of the operation.
 **/
void
g_file_query_info_async (GFile               *file,
			 const char          *attributes,
			 GFileQueryInfoFlags  flags,
			 int                  io_priority,
			 GCancellable        *cancellable,
			 GAsyncReadyCallback  callback,
			 gpointer             user_data)
{
  GFileIface *iface;
  
  g_return_if_fail (G_IS_FILE (file));

  iface = G_FILE_GET_IFACE (file);
  (* iface->query_info_async) (file,
			       attributes,
			       flags,
			       io_priority,
			       cancellable,
			       callback,
			       user_data);
}

/**
 * g_file_query_info_finish:
 * @file: input #GFile.
 * @res: a #GAsyncResult. 
 * @error: a #GError. 
 * 
 * Finishes an asynchronous file info query. 
 * See g_file_query_info_async().
 * 
 * Returns: #GFileInfo for given @file or %NULL on error.
 *     Free the returned object with g_object_unref().
 **/
GFileInfo *
g_file_query_info_finish (GFile         *file,
			  GAsyncResult  *res,
			  GError       **error)
{
  GFileIface *iface;

  g_return_val_if_fail (G_IS_FILE (file), NULL);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (res), NULL);

  if (G_IS_SIMPLE_ASYNC_RESULT (res))
    {
      GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (res);
      if (g_simple_async_result_propagate_error (simple, error))
	return NULL;
    }
  
  iface = G_FILE_GET_IFACE (file);
  return (* iface->query_info_finish) (file, res, error);
}

/**
 * g_file_query_filesystem_info:
 * @file: input #GFile.
 * @attributes:  an attribute query string.
 * @cancellable: optional #GCancellable object, %NULL to ignore. 
 * @error: a #GError. 
 * 
 * Similar to g_file_query_info(), but obtains information
 * about the filesystem the @file is on, rather than the file itself.
 * For instance the amount of space available and the type of
 * the filesystem.
 *
 * The @attribute value is a string that specifies the file attributes that
 * should be gathered. It is not an error if it's not possible to read a particular
 * requested attribute from a file - it just won't be set. @attribute should
 * be a comma-separated list of attribute or attribute wildcards. The wildcard "*"
 * means all attributes, and a wildcard like "fs:*" means all attributes in the fs
 * namespace. The standard namespace for filesystem attributes is "fs".
 * Common attributes of interest are #G_FILE_ATTRIBUTE_FILESYSTEM_SIZE
 * (the total size of the filesystem in bytes), #G_FILE_ATTRIBUTE_FILESYSTEM_FREE (number of
 * bytes available), and #G_FILE_ATTRIBUTE_FILESYSTEM_TYPE (type of the filesystem).
 * 
 * If @cancellable is not %NULL, then the operation can be cancelled by
 * triggering the cancellable object from another thread. If the operation
 * was cancelled, the error %G_IO_ERROR_CANCELLED will be returned. 
 *
 * If the file does not exist, the G_IO_ERROR_NOT_FOUND error will be returned.
 * Other errors are possible too, and depend on what kind of filesystem the file is on.
 *
 * Returns: a #GFileInfo or %NULL if there was an error.
 *     Free the returned object with g_object_unref().
 **/
GFileInfo *
g_file_query_filesystem_info (GFile         *file,
			      const char    *attributes,
			      GCancellable  *cancellable,
			      GError       **error)
{
  GFileIface *iface;
  
  g_return_val_if_fail (G_IS_FILE (file), NULL);

  if (g_cancellable_set_error_if_cancelled (cancellable, error))
    return NULL;
  
  iface = G_FILE_GET_IFACE (file);

  if (iface->query_filesystem_info == NULL)
    {
      g_set_error_literal (error, G_IO_ERROR,
                           G_IO_ERROR_NOT_SUPPORTED,
                           _("Operation not supported"));
      return NULL;
    }
  
  return (* iface->query_filesystem_info) (file, attributes, cancellable, error);
}

/**
 * g_file_query_filesystem_info_async:
 * @file: input #GFile.
 * @attributes: an attribute query string.
 * @io_priority: the <link linkend="io-priority">I/O priority</link> 
 *     of the request.
 * @cancellable: optional #GCancellable object, %NULL to ignore. 
 * @callback: a #GAsyncReadyCallback to call when the request is satisfied
 * @user_data: the data to pass to callback function
 * 
 * Asynchronously gets the requested information about the filesystem
 * that the specified @file is on. The result is a #GFileInfo object
 * that contains key-value attributes (such as type or size for the
 * file).
 * 
 * For more details, see g_file_query_filesystem_info() which is the
 * synchronous version of this call.
 *
 * When the operation is finished, @callback will be called. You can
 * then call g_file_query_info_finish() to get the result of the
 * operation.
 **/
void
g_file_query_filesystem_info_async (GFile               *file,
                                    const char          *attributes,
                                    int                  io_priority,
                                    GCancellable        *cancellable,
                                    GAsyncReadyCallback  callback,
                                    gpointer             user_data)
{
  GFileIface *iface;
  
  g_return_if_fail (G_IS_FILE (file));

  iface = G_FILE_GET_IFACE (file);
  (* iface->query_filesystem_info_async) (file,
                                          attributes,
                                          io_priority,
                                          cancellable,
                                          callback,
                                          user_data);
}

/**
 * g_file_query_filesystem_info_finish:
 * @file: input #GFile.
 * @res: a #GAsyncResult. 
 * @error: a #GError. 
 * 
 * Finishes an asynchronous filesystem info query.  See
 * g_file_query_filesystem_info_async().
 * 
 * Returns: #GFileInfo for given @file or %NULL on error.
 *     Free the returned object with g_object_unref().
 **/
GFileInfo *
g_file_query_filesystem_info_finish (GFile         *file,
                                     GAsyncResult  *res,
                                     GError       **error)
{
  GFileIface *iface;

  g_return_val_if_fail (G_IS_FILE (file), NULL);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (res), NULL);

  if (G_IS_SIMPLE_ASYNC_RESULT (res))
    {
      GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (res);
      if (g_simple_async_result_propagate_error (simple, error))
	return NULL;
    }
  
  iface = G_FILE_GET_IFACE (file);
  return (* iface->query_filesystem_info_finish) (file, res, error);
}

/**
 * g_file_find_enclosing_mount:
 * @file: input #GFile.
 * @cancellable: optional #GCancellable object, %NULL to ignore. 
 * @error: a #GError. 
 *
 * Gets a #GMount for the #GFile. 
 *
 * If the #GFileIface for @file does not have a mount (e.g. possibly a 
 * remote share), @error will be set to %G_IO_ERROR_NOT_FOUND and %NULL
 * will be returned.
 * 
 * If @cancellable is not %NULL, then the operation can be cancelled by
 * triggering the cancellable object from another thread. If the operation
 * was cancelled, the error %G_IO_ERROR_CANCELLED will be returned. 
 * 
 * Returns: a #GMount where the @file is located or %NULL on error.
 *     Free the returned object with g_object_unref().
 **/
GMount *
g_file_find_enclosing_mount (GFile         *file,
			     GCancellable  *cancellable,
			     GError       **error)
{
  GFileIface *iface;

  g_return_val_if_fail (G_IS_FILE (file), NULL);

  if (g_cancellable_set_error_if_cancelled (cancellable, error))
    return NULL;

  iface = G_FILE_GET_IFACE (file);
  if (iface->find_enclosing_mount == NULL)
    {

      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND,
			/* Translators: This is an error message when trying to find the
			 * enclosing (user visible) mount of a file, but none exists. */
		   _("Containing mount does not exist"));
      return NULL;
    }

  return (* iface->find_enclosing_mount) (file, cancellable, error);
}

/**
 * g_file_find_enclosing_mount_async:
 * @file: a #GFile
 * @io_priority: the <link linkend="io-priority">I/O priority</link> 
 *     of the request.
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * @callback: a #GAsyncReadyCallback to call when the request is satisfied
 * @user_data: the data to pass to callback function
 *
 * Asynchronously gets the mount for the file.
 *
 * For more details, see g_file_find_enclosing_mount() which is
 * the synchronous version of this call.
 *
 * When the operation is finished, @callback will be called. You can then call
 * g_file_find_enclosing_mount_finish() to get the result of the operation.
 */
void
g_file_find_enclosing_mount_async (GFile              *file,
				   int                   io_priority,
				   GCancellable         *cancellable,
				   GAsyncReadyCallback   callback,
				   gpointer              user_data)
{
  GFileIface *iface;

  g_return_if_fail (G_IS_FILE (file));

  iface = G_FILE_GET_IFACE (file);
  (* iface->find_enclosing_mount_async) (file,
					 io_priority,
					 cancellable,
					 callback,
					 user_data);
}

/**
 * g_file_find_enclosing_mount_finish:
 * @file: a #GFile
 * @res: a #GAsyncResult
 * @error: a #GError
 * 
 * Finishes an asynchronous find mount request. 
 * See g_file_find_enclosing_mount_async().
 * 
 * Returns: #GMount for given @file or %NULL on error.
 *     Free the returned object with g_object_unref().
 **/
GMount *
g_file_find_enclosing_mount_finish (GFile         *file,
				    GAsyncResult  *res,
				    GError       **error)
{
  GFileIface *iface;
  
  g_return_val_if_fail (G_IS_FILE (file), NULL);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (res), NULL);

  if (G_IS_SIMPLE_ASYNC_RESULT (res))
    {
      GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (res);
      if (g_simple_async_result_propagate_error (simple, error))
	return NULL;
    }
  
  iface = G_FILE_GET_IFACE (file);
  return (* iface->find_enclosing_mount_finish) (file, res, error);
}


/**
 * g_file_read:
 * @file: #GFile to read.
 * @cancellable: a #GCancellable
 * @error: a #GError, or %NULL
 *
 * Opens a file for reading. The result is a #GFileInputStream that
 * can be used to read the contents of the file.
 *
 * If @cancellable is not %NULL, then the operation can be cancelled by
 * triggering the cancellable object from another thread. If the operation
 * was cancelled, the error %G_IO_ERROR_CANCELLED will be returned. 
 * 
 * If the file does not exist, the G_IO_ERROR_NOT_FOUND error will be returned.
 * If the file is a directory, the G_IO_ERROR_IS_DIRECTORY error will be returned.
 * Other errors are possible too, and depend on what kind of filesystem the file is on.
 *
 * Returns: #GFileInputStream or %NULL on error.
 *     Free the returned object with g_object_unref().
 **/
GFileInputStream *
g_file_read (GFile         *file,
	     GCancellable  *cancellable,
	     GError       **error)
{
  GFileIface *iface;
  
  g_return_val_if_fail (G_IS_FILE (file), NULL);

  if (g_cancellable_set_error_if_cancelled (cancellable, error))
    return NULL;

  iface = G_FILE_GET_IFACE (file);

  if (iface->read_fn == NULL)
    {
      g_set_error_literal (error, G_IO_ERROR,
                           G_IO_ERROR_NOT_SUPPORTED,
                           _("Operation not supported"));
      return NULL;
    }
  
  return (* iface->read_fn) (file, cancellable, error);
}

/**
 * g_file_append_to:
 * @file: input #GFile.
 * @flags: a set of #GFileCreateFlags.
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * @error: a #GError, or %NULL
 *
 * Gets an output stream for appending data to the file. If
 * the file doesn't already exist it is created.
 *
 * By default files created are generally readable by everyone,
 * but if you pass #G_FILE_CREATE_PRIVATE in @flags the file
 * will be made readable only to the current user, to the level that
 * is supported on the target filesystem.
 *
 * If @cancellable is not %NULL, then the operation can be cancelled by
 * triggering the cancellable object from another thread. If the operation
 * was cancelled, the error %G_IO_ERROR_CANCELLED will be returned. 
 *
 * Some file systems don't allow all file names, and may
 * return an %G_IO_ERROR_INVALID_FILENAME error.
 * If the file is a directory the %G_IO_ERROR_IS_DIRECTORY error will be
 * returned. Other errors are possible too, and depend on what kind of
 * filesystem the file is on.
 * 
 * Returns: a #GFileOutputStream, or %NULL on error.
 *     Free the returned object with g_object_unref().
 **/
GFileOutputStream *
g_file_append_to (GFile             *file,
		  GFileCreateFlags   flags,
		  GCancellable      *cancellable,
		  GError           **error)
{
  GFileIface *iface;

  g_return_val_if_fail (G_IS_FILE (file), NULL);

  if (g_cancellable_set_error_if_cancelled (cancellable, error))
    return NULL;
  
  iface = G_FILE_GET_IFACE (file);

  if (iface->append_to == NULL)
    {
      g_set_error_literal (error, G_IO_ERROR,
                           G_IO_ERROR_NOT_SUPPORTED,
                           _("Operation not supported"));
      return NULL;
    }
  
  return (* iface->append_to) (file, flags, cancellable, error);
}

/**
 * g_file_create:
 * @file: input #GFile.
 * @flags: a set of #GFileCreateFlags.
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * @error: a #GError, or %NULL
 *
 * Creates a new file and returns an output stream for writing to it.
 * The file must not already exist.
 *
 * By default files created are generally readable by everyone,
 * but if you pass #G_FILE_CREATE_PRIVATE in @flags the file
 * will be made readable only to the current user, to the level that
 * is supported on the target filesystem.
 *
 * If @cancellable is not %NULL, then the operation can be cancelled by
 * triggering the cancellable object from another thread. If the operation
 * was cancelled, the error %G_IO_ERROR_CANCELLED will be returned. 
 *
 * If a file or directory with this name already exists the G_IO_ERROR_EXISTS
 * error will be returned.
 * Some file systems don't allow all file names, and may
 * return an G_IO_ERROR_INVALID_FILENAME error, and if the name
 * is to long G_IO_ERROR_FILENAME_TOO_LONG will be returned.
 * Other errors are possible too, and depend on what kind of
 * filesystem the file is on.
 * 
 * Returns: a #GFileOutputStream for the newly created file, or 
 *     %NULL on error.
 *     Free the returned object with g_object_unref().
 **/
GFileOutputStream *
g_file_create (GFile             *file,
	       GFileCreateFlags   flags,
	       GCancellable      *cancellable,
	       GError           **error)
{
  GFileIface *iface;
  
  g_return_val_if_fail (G_IS_FILE (file), NULL);

  if (g_cancellable_set_error_if_cancelled (cancellable, error))
    return NULL;
  
  iface = G_FILE_GET_IFACE (file);

  if (iface->create == NULL)
    {
      g_set_error_literal (error, G_IO_ERROR,
                           G_IO_ERROR_NOT_SUPPORTED,
                           _("Operation not supported"));
      return NULL;
    }
  
  return (* iface->create) (file, flags, cancellable, error);
}

/**
 * g_file_replace:
 * @file: input #GFile.
 * @etag: an optional <link linkend="gfile-etag">entity tag</link> for the 
 *     current #GFile, or #NULL to ignore.
 * @make_backup: %TRUE if a backup should be created.
 * @flags: a set of #GFileCreateFlags.
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * @error: a #GError, or %NULL
 *
 * Returns an output stream for overwriting the file, possibly
 * creating a backup copy of the file first. If the file doesn't exist,
 * it will be created.
 *
 * This will try to replace the file in the safest way possible so
 * that any errors during the writing will not affect an already
 * existing copy of the file. For instance, for local files it
 * may write to a temporary file and then atomically rename over
 * the destination when the stream is closed.
 * 
 * By default files created are generally readable by everyone,
 * but if you pass #G_FILE_CREATE_PRIVATE in @flags the file
 * will be made readable only to the current user, to the level that
 * is supported on the target filesystem.
 *
 * If @cancellable is not %NULL, then the operation can be cancelled by
 * triggering the cancellable object from another thread. If the operation
 * was cancelled, the error %G_IO_ERROR_CANCELLED will be returned. 
 * 
 * If you pass in a non-#NULL @etag value, then this value is
 * compared to the current entity tag of the file, and if they differ
 * an G_IO_ERROR_WRONG_ETAG error is returned. This generally means
 * that the file has been changed since you last read it. You can get
 * the new etag from g_file_output_stream_get_etag() after you've
 * finished writing and closed the #GFileOutputStream. When you load
 * a new file you can use g_file_input_stream_query_info() to get
 * the etag of the file.
 * 
 * If @make_backup is %TRUE, this function will attempt to make a backup
 * of the current file before overwriting it. If this fails a G_IO_ERROR_CANT_CREATE_BACKUP
 * error will be returned. If you want to replace anyway, try again with
 * @make_backup set to %FALSE.
 *
 * If the file is a directory the G_IO_ERROR_IS_DIRECTORY error will be returned,
 * and if the file is some other form of non-regular file then a
 * G_IO_ERROR_NOT_REGULAR_FILE error will be returned.
 * Some file systems don't allow all file names, and may
 * return an G_IO_ERROR_INVALID_FILENAME error, and if the name
 * is to long G_IO_ERROR_FILENAME_TOO_LONG will be returned.
 * Other errors are possible too, and depend on what kind of
 * filesystem the file is on.
 *
 * Returns: a #GFileOutputStream or %NULL on error. 
 *     Free the returned object with g_object_unref().
 **/
GFileOutputStream *
g_file_replace (GFile             *file,
		const char        *etag,
		gboolean           make_backup,
		GFileCreateFlags   flags,
		GCancellable      *cancellable,
		GError           **error)
{
  GFileIface *iface;

  g_return_val_if_fail (G_IS_FILE (file), NULL);

  if (g_cancellable_set_error_if_cancelled (cancellable, error))
    return NULL;
  
  iface = G_FILE_GET_IFACE (file);

  if (iface->replace == NULL)
    {
      g_set_error_literal (error, G_IO_ERROR,
                           G_IO_ERROR_NOT_SUPPORTED,
                           _("Operation not supported"));
      return NULL;
    }
  
  
  /* Handle empty tag string as NULL in consistent way. */
  if (etag && *etag == 0)
    etag = NULL;
  
  return (* iface->replace) (file, etag, make_backup, flags, cancellable, error);
}

/**
 * g_file_open_readwrite:
 * @file: #GFile to open
 * @cancellable: a #GCancellable
 * @error: a #GError, or %NULL
 *
 * Opens an existing file for reading and writing. The result is
 * a #GFileIOStream that can be used to read and write the contents of the file.
 *
 * If @cancellable is not %NULL, then the operation can be cancelled by
 * triggering the cancellable object from another thread. If the operation
 * was cancelled, the error %G_IO_ERROR_CANCELLED will be returned.
 *
 * If the file does not exist, the G_IO_ERROR_NOT_FOUND error will be returned.
 * If the file is a directory, the G_IO_ERROR_IS_DIRECTORY error will be returned.
 * Other errors are possible too, and depend on what kind of filesystem the file is on.
 * Note that in many non-local file cases read and write streams are not supported,
 * so make sure you really need to do read and write streaming, rather than
 * just opening for reading or writing.
 *
 * Returns: #GFileIOStream or %NULL on error.
 *     Free the returned object with g_object_unref().
 *
 * Since: 2.22
 **/
GFileIOStream *
g_file_open_readwrite (GFile                      *file,
                       GCancellable               *cancellable,
                       GError                    **error)
{
  GFileIface *iface;

  g_return_val_if_fail (G_IS_FILE (file), NULL);

  if (g_cancellable_set_error_if_cancelled (cancellable, error))
    return NULL;

  iface = G_FILE_GET_IFACE (file);

  if (iface->open_readwrite == NULL)
    {
      g_set_error_literal (error, G_IO_ERROR,
                           G_IO_ERROR_NOT_SUPPORTED,
                           _("Operation not supported"));
      return NULL;
    }

  return (* iface->open_readwrite) (file, cancellable, error);
}

/**
 * g_file_create_readwrite:
 * @file: a #GFile
 * @flags: a set of #GFileCreateFlags
 * @cancellable: optional #GCancellable object, %NULL to ignore
 * @error: return location for a #GError, or %NULL
 *
 * Creates a new file and returns a stream for reading and writing to it.
 * The file must not already exist.
 *
 * By default files created are generally readable by everyone,
 * but if you pass #G_FILE_CREATE_PRIVATE in @flags the file
 * will be made readable only to the current user, to the level that
 * is supported on the target filesystem.
 *
 * If @cancellable is not %NULL, then the operation can be cancelled by
 * triggering the cancellable object from another thread. If the operation
 * was cancelled, the error %G_IO_ERROR_CANCELLED will be returned.
 *
 * If a file or directory with this name already exists the %G_IO_ERROR_EXISTS
 * error will be returned. Some file systems don't allow all file names,
 * and may return an %G_IO_ERROR_INVALID_FILENAME error, and if the name
 * is too long, %G_IO_ERROR_FILENAME_TOO_LONG will be returned. Other errors
 * are possible too, and depend on what kind of filesystem the file is on.
 *
 * Note that in many non-local file cases read and write streams are not
 * supported, so make sure you really need to do read and write streaming,
 * rather than just opening for reading or writing.
 *
 * Returns: a #GFileIOStream for the newly created file, or %NULL on error.
 *     Free the returned object with g_object_unref().
 *
 * Since: 2.22
 */
GFileIOStream *
g_file_create_readwrite (GFile             *file,
                         GFileCreateFlags   flags,
                         GCancellable      *cancellable,
                         GError           **error)
{
  GFileIface *iface;

  g_return_val_if_fail (G_IS_FILE (file), NULL);

  if (g_cancellable_set_error_if_cancelled (cancellable, error))
    return NULL;

  iface = G_FILE_GET_IFACE (file);

  if (iface->create_readwrite == NULL)
    {
      g_set_error_literal (error, G_IO_ERROR,
                           G_IO_ERROR_NOT_SUPPORTED,
                           _("Operation not supported"));
      return NULL;
    }

  return (* iface->create_readwrite) (file, flags, cancellable, error);
}

/**
 * g_file_replace_readwrite:
 * @file: a #GFile
 * @etag: an optional <link linkend="gfile-etag">entity tag</link> for the
 *     current #GFile, or #NULL to ignore
 * @make_backup: %TRUE if a backup should be created
 * @flags: a set of #GFileCreateFlags
 * @cancellable: optional #GCancellable object, %NULL to ignore
 * @error: return location for a #GError, or %NULL
 *
 * Returns an output stream for overwriting the file in readwrite mode,
 * possibly creating a backup copy of the file first. If the file doesn't
 * exist, it will be created.
 *
 * For details about the behaviour, see g_file_replace() which does the same
 * thing but returns an output stream only.
 *
 * Note that in many non-local file cases read and write streams are not
 * supported, so make sure you really need to do read and write streaming,
 * rather than just opening for reading or writing.
 *
 * Returns: a #GFileIOStream or %NULL on error.
 *     Free the returned object with g_object_unref().
 *
 * Since: 2.22
 */
GFileIOStream *
g_file_replace_readwrite (GFile             *file,
                          const char        *etag,
                          gboolean           make_backup,
                          GFileCreateFlags   flags,
                          GCancellable      *cancellable,
                          GError           **error)
{
  GFileIface *iface;

  g_return_val_if_fail (G_IS_FILE (file), NULL);

  if (g_cancellable_set_error_if_cancelled (cancellable, error))
    return NULL;

  iface = G_FILE_GET_IFACE (file);

  if (iface->replace_readwrite == NULL)
    {
      g_set_error_literal (error, G_IO_ERROR,
                           G_IO_ERROR_NOT_SUPPORTED,
                           _("Operation not supported"));
      return NULL;
    }

  return (* iface->replace_readwrite) (file, etag, make_backup, flags, cancellable, error);
}

/**
 * g_file_read_async:
 * @file: input #GFile
 * @io_priority: the <link linkend="io-priority">I/O priority</link>
 *     of the request.
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * @callback: a #GAsyncReadyCallback to call when the request is satisfied
 * @user_data: the data to pass to callback function
 *
 * Asynchronously opens @file for reading.
 *
 * For more details, see g_file_read() which is
 * the synchronous version of this call.
 *
 * When the operation is finished, @callback will be called. You can then call
 * g_file_read_finish() to get the result of the operation.
 **/
void
g_file_read_async (GFile               *file,
		   int                  io_priority,
		   GCancellable        *cancellable,
		   GAsyncReadyCallback  callback,
		   gpointer             user_data)
{
  GFileIface *iface;
  
  g_return_if_fail (G_IS_FILE (file));

  iface = G_FILE_GET_IFACE (file);
  (* iface->read_async) (file,
			 io_priority,
			 cancellable,
			 callback,
			 user_data);
}

/**
 * g_file_read_finish:
 * @file: input #GFile.
 * @res: a #GAsyncResult. 
 * @error: a #GError, or %NULL
 *
 * Finishes an asynchronous file read operation started with 
 * g_file_read_async(). 
 *  
 * Returns: a #GFileInputStream or %NULL on error.
 *     Free the returned object with g_object_unref().
 **/
GFileInputStream *
g_file_read_finish (GFile         *file,
		    GAsyncResult  *res,
		    GError       **error)
{
  GFileIface *iface;
  
  g_return_val_if_fail (G_IS_FILE (file), NULL);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (res), NULL);

  if (G_IS_SIMPLE_ASYNC_RESULT (res))
    {
      GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (res);
      if (g_simple_async_result_propagate_error (simple, error))
	return NULL;
    }
  
  iface = G_FILE_GET_IFACE (file);
  return (* iface->read_finish) (file, res, error);
}

/**
 * g_file_append_to_async:
 * @file: input #GFile.
 * @flags: a set of #GFileCreateFlags.
 * @io_priority: the <link linkend="io-priority">I/O priority</link> 
 *     of the request. 
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * @callback: a #GAsyncReadyCallback to call when the request is satisfied
 * @user_data: the data to pass to callback function
 * 
 * Asynchronously opens @file for appending.
 *
 * For more details, see g_file_append_to() which is
 * the synchronous version of this call.
 *
 * When the operation is finished, @callback will be called. You can then call
 * g_file_append_to_finish() to get the result of the operation.
 **/
void
g_file_append_to_async (GFile               *file,
			GFileCreateFlags     flags,
			int                  io_priority,
			GCancellable        *cancellable,
			GAsyncReadyCallback  callback,
			gpointer             user_data)
{
  GFileIface *iface;
  
  g_return_if_fail (G_IS_FILE (file));

  iface = G_FILE_GET_IFACE (file);
  (* iface->append_to_async) (file,
			      flags,
			      io_priority,
			      cancellable,
			      callback,
			      user_data);
}

/**
 * g_file_append_to_finish:
 * @file: input #GFile.
 * @res: #GAsyncResult
 * @error: a #GError, or %NULL
 * 
 * Finishes an asynchronous file append operation started with 
 * g_file_append_to_async(). 
 * 
 * Returns: a valid #GFileOutputStream or %NULL on error.
 *     Free the returned object with g_object_unref().
 **/
GFileOutputStream *
g_file_append_to_finish (GFile         *file,
			 GAsyncResult  *res,
			 GError       **error)
{
  GFileIface *iface;
  
  g_return_val_if_fail (G_IS_FILE (file), NULL);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (res), NULL);

  if (G_IS_SIMPLE_ASYNC_RESULT (res))
    {
      GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (res);
      if (g_simple_async_result_propagate_error (simple, error))
	return NULL;
    }
  
  iface = G_FILE_GET_IFACE (file);
  return (* iface->append_to_finish) (file, res, error);
}

/**
 * g_file_create_async:
 * @file: input #GFile.
 * @flags: a set of #GFileCreateFlags.
 * @io_priority: the <link linkend="io-priority">I/O priority</link> 
 *     of the request.
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * @callback: a #GAsyncReadyCallback to call when the request is satisfied
 * @user_data: the data to pass to callback function
 * 
 * Asynchronously creates a new file and returns an output stream for writing to it.
 * The file must not already exist.
 *
 * For more details, see g_file_create() which is
 * the synchronous version of this call.
 *
 * When the operation is finished, @callback will be called. You can then call
 * g_file_create_finish() to get the result of the operation.
 **/
void
g_file_create_async (GFile               *file,
		     GFileCreateFlags     flags,
		     int                  io_priority,
		     GCancellable        *cancellable,
		     GAsyncReadyCallback  callback,
		     gpointer             user_data)
{
  GFileIface *iface;
  
  g_return_if_fail (G_IS_FILE (file));

  iface = G_FILE_GET_IFACE (file);
  (* iface->create_async) (file,
			   flags,
			   io_priority,
			   cancellable,
			   callback,
			   user_data);
}

/**
 * g_file_create_finish:
 * @file: input #GFile.
 * @res: a #GAsyncResult. 
 * @error: a #GError, or %NULL
 * 
 * Finishes an asynchronous file create operation started with 
 * g_file_create_async(). 
 * 
 * Returns: a #GFileOutputStream or %NULL on error.
 *     Free the returned object with g_object_unref().
 **/
GFileOutputStream *
g_file_create_finish (GFile         *file,
		      GAsyncResult  *res,
		      GError       **error)
{
  GFileIface *iface;
  
  g_return_val_if_fail (G_IS_FILE (file), NULL);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (res), NULL);

  if (G_IS_SIMPLE_ASYNC_RESULT (res))
    {
      GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (res);
      if (g_simple_async_result_propagate_error (simple, error))
	return NULL;
    }
  
  iface = G_FILE_GET_IFACE (file);
  return (* iface->create_finish) (file, res, error);
}

/**
 * g_file_replace_async:
 * @file: input #GFile.
 * @etag: an <link linkend="gfile-etag">entity tag</link> for the 
 *     current #GFile, or NULL to ignore.
 * @make_backup: %TRUE if a backup should be created.
 * @flags: a set of #GFileCreateFlags.
 * @io_priority: the <link linkend="io-priority">I/O priority</link> 
 *     of the request.
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * @callback: a #GAsyncReadyCallback to call when the request is satisfied
 * @user_data: the data to pass to callback function
 *
 * Asynchronously overwrites the file, replacing the contents, possibly
 * creating a backup copy of the file first.
 *
 * For more details, see g_file_replace() which is
 * the synchronous version of this call.
 *
 * When the operation is finished, @callback will be called. You can then call
 * g_file_replace_finish() to get the result of the operation.
 **/
void
g_file_replace_async (GFile               *file,
		      const char          *etag,
		      gboolean             make_backup,
		      GFileCreateFlags     flags,
		      int                  io_priority,
		      GCancellable        *cancellable,
		      GAsyncReadyCallback  callback,
		      gpointer             user_data)
{
  GFileIface *iface;
  
  g_return_if_fail (G_IS_FILE (file));

  iface = G_FILE_GET_IFACE (file);
  (* iface->replace_async) (file,
			    etag,
			    make_backup,
			    flags,
			    io_priority,
			    cancellable,
			    callback,
			    user_data);
}

/**
 * g_file_replace_finish:
 * @file: input #GFile.
 * @res: a #GAsyncResult. 
 * @error: a #GError, or %NULL
 * 
 * Finishes an asynchronous file replace operation started with 
 * g_file_replace_async(). 
 * 
 * Returns: a #GFileOutputStream, or %NULL on error.
 *     Free the returned object with g_object_unref().
 **/
GFileOutputStream *
g_file_replace_finish (GFile         *file,
		       GAsyncResult  *res,
		       GError       **error)
{
  GFileIface *iface;
  
  g_return_val_if_fail (G_IS_FILE (file), NULL);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (res), NULL);

  if (G_IS_SIMPLE_ASYNC_RESULT (res))
    {
      GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (res);
      if (g_simple_async_result_propagate_error (simple, error))
	return NULL;
    }

  iface = G_FILE_GET_IFACE (file);
  return (* iface->replace_finish) (file, res, error);
}


/**
 * g_file_open_readwrite_async:
 * @file: input #GFile.
 * @io_priority: the <link linkend="io-priority">I/O priority</link>
 *     of the request.
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * @callback: a #GAsyncReadyCallback to call when the request is satisfied
 * @user_data: the data to pass to callback function
 *
 * Asynchronously opens @file for reading and writing.
 *
 * For more details, see g_file_open_readwrite() which is
 * the synchronous version of this call.
 *
 * When the operation is finished, @callback will be called. You can then call
 * g_file_open_readwrite_finish() to get the result of the operation.
 *
 * Since: 2.22
 **/
void
g_file_open_readwrite_async (GFile                      *file,
                             int                         io_priority,
                             GCancellable               *cancellable,
                             GAsyncReadyCallback         callback,
                             gpointer                    user_data)
{
  GFileIface *iface;

  g_return_if_fail (G_IS_FILE (file));

  iface = G_FILE_GET_IFACE (file);
  (* iface->open_readwrite_async) (file,
                                   io_priority,
                                   cancellable,
                                   callback,
                                   user_data);
}

/**
 * g_file_open_readwrite_finish:
 * @file: input #GFile.
 * @res: a #GAsyncResult.
 * @error: a #GError, or %NULL
 *
 * Finishes an asynchronous file read operation started with
 * g_file_open_readwrite_async().
 *
 * Returns: a #GFileIOStream or %NULL on error.
 *     Free the returned object with g_object_unref().
 *
 * Since: 2.22
 **/
GFileIOStream *
g_file_open_readwrite_finish (GFile                      *file,
                              GAsyncResult               *res,
                              GError                    **error)
{
  GFileIface *iface;

  g_return_val_if_fail (G_IS_FILE (file), NULL);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (res), NULL);

  if (G_IS_SIMPLE_ASYNC_RESULT (res))
    {
      GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (res);
      if (g_simple_async_result_propagate_error (simple, error))
        return NULL;
    }

  iface = G_FILE_GET_IFACE (file);
  return (* iface->open_readwrite_finish) (file, res, error);
}


/**
 * g_file_create_readwrite_async:
 * @file: input #GFile
 * @flags: a set of #GFileCreateFlags
 * @io_priority: the <link linkend="io-priority">I/O priority</link>
 *     of the request
 * @cancellable: optional #GCancellable object, %NULL to ignore
 * @callback: a #GAsyncReadyCallback to call when the request is satisfied
 * @user_data: the data to pass to callback function
 *
 * Asynchronously creates a new file and returns a stream for reading and
 * writing to it. The file must not already exist.
 *
 * For more details, see g_file_create_readwrite() which is
 * the synchronous version of this call.
 *
 * When the operation is finished, @callback will be called. You can then
 * call g_file_create_readwrite_finish() to get the result of the operation.
 *
 * Since: 2.22
 */
void
g_file_create_readwrite_async (GFile               *file,
                               GFileCreateFlags     flags,
                               int                  io_priority,
                               GCancellable        *cancellable,
                               GAsyncReadyCallback  callback,
                               gpointer             user_data)
{
  GFileIface *iface;

  g_return_if_fail (G_IS_FILE (file));

  iface = G_FILE_GET_IFACE (file);
  (* iface->create_readwrite_async) (file,
                                     flags,
                                     io_priority,
                                     cancellable,
                                     callback,
                                     user_data);
}

/**
 * g_file_create_readwrite_finish:
 * @file: input #GFile
 * @res: a #GAsyncResult
 * @error: a #GError, or %NULL
 *
 * Finishes an asynchronous file create operation started with
 * g_file_create_readwrite_async().
 *
 * Returns: a #GFileIOStream or %NULL on error.
 *     Free the returned object with g_object_unref().
 *
 * Since: 2.22
 **/
GFileIOStream *
g_file_create_readwrite_finish (GFile         *file,
                                GAsyncResult  *res,
                                GError       **error)
{
  GFileIface *iface;

  g_return_val_if_fail (G_IS_FILE (file), NULL);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (res), NULL);

  if (G_IS_SIMPLE_ASYNC_RESULT (res))
    {
      GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (res);
      if (g_simple_async_result_propagate_error (simple, error))
        return NULL;
    }

  iface = G_FILE_GET_IFACE (file);
  return (* iface->create_readwrite_finish) (file, res, error);
}

/**
 * g_file_replace_readwrite_async:
 * @file: input #GFile.
 * @etag: an <link linkend="gfile-etag">entity tag</link> for the
 *     current #GFile, or NULL to ignore.
 * @make_backup: %TRUE if a backup should be created.
 * @flags: a set of #GFileCreateFlags.
 * @io_priority: the <link linkend="io-priority">I/O priority</link>
 *     of the request.
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * @callback: a #GAsyncReadyCallback to call when the request is satisfied
 * @user_data: the data to pass to callback function
 *
 * Asynchronously overwrites the file in read-write mode, replacing the
 * contents, possibly creating a backup copy of the file first.
 *
 * For more details, see g_file_replace_readwrite() which is
 * the synchronous version of this call.
 *
 * When the operation is finished, @callback will be called. You can then
 * call g_file_replace_readwrite_finish() to get the result of the operation.
 *
 * Since: 2.22
 */
void
g_file_replace_readwrite_async (GFile               *file,
                                const char          *etag,
                                gboolean             make_backup,
                                GFileCreateFlags     flags,
                                int                  io_priority,
                                GCancellable        *cancellable,
                                GAsyncReadyCallback  callback,
                                gpointer             user_data)
{
  GFileIface *iface;

  g_return_if_fail (G_IS_FILE (file));

  iface = G_FILE_GET_IFACE (file);
  (* iface->replace_readwrite_async) (file,
                                      etag,
                                      make_backup,
                                      flags,
                                      io_priority,
                                      cancellable,
                                      callback,
                                      user_data);
}

/**
 * g_file_replace_readwrite_finish:
 * @file: input #GFile.
 * @res: a #GAsyncResult.
 * @error: a #GError, or %NULL
 *
 * Finishes an asynchronous file replace operation started with
 * g_file_replace_readwrite_async().
 *
 * Returns: a #GFileIOStream, or %NULL on error.
 *     Free the returned object with g_object_unref().
 *
 * Since: 2.22
 */
GFileIOStream *
g_file_replace_readwrite_finish (GFile         *file,
                                 GAsyncResult  *res,
                                 GError       **error)
{
  GFileIface *iface;

  g_return_val_if_fail (G_IS_FILE (file), NULL);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (res), NULL);

  if (G_IS_SIMPLE_ASYNC_RESULT (res))
    {
      GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (res);
      if (g_simple_async_result_propagate_error (simple, error))
        return NULL;
    }

  iface = G_FILE_GET_IFACE (file);
  return (* iface->replace_readwrite_finish) (file, res, error);
}

static gboolean
copy_symlink (GFile           *destination,
	      GFileCopyFlags   flags,
	      GCancellable    *cancellable,
	      const char      *target,
	      GError         **error)
{
  GError *my_error;
  gboolean tried_delete;
  GFileInfo *info;
  GFileType file_type;

  tried_delete = FALSE;

 retry:
  my_error = NULL;
  if (!g_file_make_symbolic_link (destination, target, cancellable, &my_error))
    {
      /* Maybe it already existed, and we want to overwrite? */
      if (!tried_delete && (flags & G_FILE_COPY_OVERWRITE) && 
	  my_error->domain == G_IO_ERROR && my_error->code == G_IO_ERROR_EXISTS)
	{
	  g_error_free (my_error);


	  /* Don't overwrite if the destination is a directory */
	  info = g_file_query_info (destination, G_FILE_ATTRIBUTE_STANDARD_TYPE,
				    G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
				    cancellable, &my_error);
	  if (info != NULL)
	    {
	      file_type = g_file_info_get_file_type (info);
	      g_object_unref (info);
	      
	      if (file_type == G_FILE_TYPE_DIRECTORY)
		{
		  g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_IS_DIRECTORY,
                                       _("Can't copy over directory"));
		  return FALSE;
		}
	    }
	  
	  if (!g_file_delete (destination, cancellable, error))
	    return FALSE;
	  
	  tried_delete = TRUE;
	  goto retry;
	}
            /* Nah, fail */
      g_propagate_error (error, my_error);
      return FALSE;
    }

  return TRUE;
}

static GInputStream *
open_source_for_copy (GFile           *source,
		      GFile           *destination,
		      GFileCopyFlags   flags,
		      GCancellable    *cancellable,
		      GError         **error)
{
  GError *my_error;
  GInputStream *in;
  GFileInfo *info;
  GFileType file_type;
  
  my_error = NULL;
  in = (GInputStream *)g_file_read (source, cancellable, &my_error);
  if (in != NULL)
    return in;

  /* There was an error opening the source, try to set a good error for it: */

  if (my_error->domain == G_IO_ERROR && my_error->code == G_IO_ERROR_IS_DIRECTORY)
    {
      /* The source is a directory, don't fail with WOULD_RECURSE immediately, 
       * as that is less useful to the app. Better check for errors on the 
       * target instead. 
       */
      g_error_free (my_error);
      my_error = NULL;
      
      info = g_file_query_info (destination, G_FILE_ATTRIBUTE_STANDARD_TYPE,
				G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
				cancellable, &my_error);
      if (info != NULL)
	{
	  file_type = g_file_info_get_file_type (info);
	  g_object_unref (info);
	  
	  if (flags & G_FILE_COPY_OVERWRITE)
	    {
	      if (file_type == G_FILE_TYPE_DIRECTORY)
		{
		  g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_WOULD_MERGE,
                                       _("Can't copy directory over directory"));
		  return NULL;
		}
	      /* continue to would_recurse error */
	    }
	  else
	    {
	      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_EXISTS,
                                   _("Target file exists"));
	      return NULL;
	    }
	}
      else
	{
	  /* Error getting info from target, return that error 
           * (except for NOT_FOUND, which is no error here) 
           */
	  if (my_error->domain != G_IO_ERROR && my_error->code != G_IO_ERROR_NOT_FOUND)
	    {
	      g_propagate_error (error, my_error);
	      return NULL;
	    }
	  g_error_free (my_error);
	}
      
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_WOULD_RECURSE,
                           _("Can't recursively copy directory"));
      return NULL;
    }

  g_propagate_error (error, my_error);
  return NULL;
}

static gboolean
should_copy (GFileAttributeInfo *info, 
             gboolean            as_move,
             gboolean            skip_perms)
{
  if (skip_perms && strcmp(info->name, "unix::mode") == 0)
        return FALSE;

  if (as_move)
    return info->flags & G_FILE_ATTRIBUTE_INFO_COPY_WHEN_MOVED;
  return info->flags & G_FILE_ATTRIBUTE_INFO_COPY_WITH_FILE;
}

static char *
build_attribute_list_for_copy (GFileAttributeInfoList *attributes,
			       GFileAttributeInfoList *namespaces,
			       gboolean                as_move,
			       gboolean                skip_perms)
{
  GString *s;
  gboolean first;
  int i;
  
  first = TRUE;
  s = g_string_new ("");

  if (attributes)
    {
      for (i = 0; i < attributes->n_infos; i++)
	{
	  if (should_copy (&attributes->infos[i], as_move, skip_perms))
	    {
	      if (first)
		first = FALSE;
	      else
		g_string_append_c (s, ',');
		
	      g_string_append (s, attributes->infos[i].name);
	    }
	}
    }

  if (namespaces)
    {
      for (i = 0; i < namespaces->n_infos; i++)
	{
	  if (should_copy (&namespaces->infos[i], as_move, FALSE))
	    {
	      if (first)
		first = FALSE;
	      else
		g_string_append_c (s, ',');
		
	      g_string_append (s, namespaces->infos[i].name);
	      g_string_append (s, "::*");
	    }
	}
    }

  return g_string_free (s, FALSE);
}

/**
 * g_file_copy_attributes:
 * @source: a #GFile with attributes.
 * @destination: a #GFile to copy attributes to.
 * @flags: a set of #GFileCopyFlags.
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * @error: a #GError, %NULL to ignore.
 *
 * Copies the file attributes from @source to @destination. 
 *
 * Normally only a subset of the file attributes are copied,
 * those that are copies in a normal file copy operation
 * (which for instance does not include e.g. owner). However
 * if #G_FILE_COPY_ALL_METADATA is specified in @flags, then
 * all the metadata that is possible to copy is copied. This
 * is useful when implementing move by copy + delete source.
 *
 * Returns: %TRUE if the attributes were copied successfully, %FALSE otherwise.
 **/
gboolean
g_file_copy_attributes (GFile           *source,
			GFile           *destination,
			GFileCopyFlags   flags,
			GCancellable    *cancellable,
			GError         **error)
{
  GFileAttributeInfoList *attributes, *namespaces;
  char *attrs_to_read;
  gboolean res;
  GFileInfo *info;
  gboolean as_move;
  gboolean source_nofollow_symlinks;
  gboolean skip_perms;

  as_move = flags & G_FILE_COPY_ALL_METADATA;
  source_nofollow_symlinks = flags & G_FILE_COPY_NOFOLLOW_SYMLINKS;
  skip_perms = (flags & G_FILE_COPY_TARGET_DEFAULT_PERMS) != 0;

  /* Ignore errors here, if the target supports no attributes there is nothing to copy */
  attributes = g_file_query_settable_attributes (destination, cancellable, NULL);
  namespaces = g_file_query_writable_namespaces (destination, cancellable, NULL);

  if (attributes == NULL && namespaces == NULL)
    return TRUE;

  attrs_to_read = build_attribute_list_for_copy (attributes, namespaces, as_move, skip_perms);

  /* Ignore errors here, if we can't read some info (e.g. if it doesn't exist)
   * we just don't copy it. 
   */
  info = g_file_query_info (source, attrs_to_read,
			    source_nofollow_symlinks ? G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS:0,
			    cancellable,
			    NULL);

  g_free (attrs_to_read);
  
  res = TRUE;
  if  (info)
    {
      res = g_file_set_attributes_from_info (destination,
					     info,
                         G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
					     cancellable,
					     error);
      g_object_unref (info);
    }
  
  g_file_attribute_info_list_unref (attributes);
  g_file_attribute_info_list_unref (namespaces);
  
  return res;
}

static gboolean
copy_stream_with_progress (GInputStream           *in,
			   GOutputStream          *out,
                           GFile                  *source,
			   GCancellable           *cancellable,
			   GFileProgressCallback   progress_callback,
			   gpointer                progress_callback_data,
			   GError                **error)
{
  gssize n_read, n_written;
  goffset current_size;
  char buffer[1024*64], *p;
  gboolean res;
  goffset total_size;
  GFileInfo *info;

  total_size = -1;
  /* avoid performance impact of querying total size when it's not needed */
  if (progress_callback)
    {
      info = g_file_input_stream_query_info (G_FILE_INPUT_STREAM (in),
                                             G_FILE_ATTRIBUTE_STANDARD_SIZE,
                                             cancellable, NULL);
      if (info)
        {
          if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_STANDARD_SIZE))
            total_size = g_file_info_get_size (info);
          g_object_unref (info);
        }

      if (total_size == -1)
        {
          info = g_file_query_info (source, 
                                    G_FILE_ATTRIBUTE_STANDARD_SIZE,
                                    G_FILE_QUERY_INFO_NONE,
                                    cancellable, NULL);
          if (info)
            {
              if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_STANDARD_SIZE))
                total_size = g_file_info_get_size (info);
              g_object_unref (info);
            }
        }
    }

  if (total_size == -1)
    total_size = 0;
  
  current_size = 0;
  res = TRUE;
  while (TRUE)
    {
      n_read = g_input_stream_read (in, buffer, sizeof (buffer), cancellable, error);
      if (n_read == -1)
	{
	  res = FALSE;
	  break;
	}
	
      if (n_read == 0)
	break;

      current_size += n_read;

      p = buffer;
      while (n_read > 0)
	{
	  n_written = g_output_stream_write (out, p, n_read, cancellable, error);
	  if (n_written == -1)
	    {
	      res = FALSE;
	      break;
	    }

	  p += n_written;
	  n_read -= n_written;
	}

      if (!res)
        break;

      if (progress_callback)
	progress_callback (current_size, total_size, progress_callback_data);
    }

  /* Make sure we send full copied size */
  if (progress_callback)
    progress_callback (current_size, total_size, progress_callback_data);

  return res;
}

#ifdef HAVE_SPLICE

static gboolean
do_splice (int     fd_in,
	   loff_t *off_in,
           int     fd_out,
	   loff_t *off_out,
           size_t  len,
           long   *bytes_transferd,
           GError **error)
{
  long result;

retry:
  result = splice (fd_in, off_in, fd_out, off_out, len, SPLICE_F_MORE);

  if (result == -1)
    {
      int errsv = errno;

      if (errsv == EINTR)
        goto retry;
      else if (errsv == ENOSYS || errsv == EINVAL)
        g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                             _("Splice not supported"));
      else
        g_set_error (error, G_IO_ERROR,
                     g_io_error_from_errno (errsv),
                     _("Error splicing file: %s"),
                     g_strerror (errsv));

      return FALSE;
    }

  *bytes_transferd = result;
  return TRUE;
}

static gboolean
splice_stream_with_progress (GInputStream           *in,
                             GOutputStream          *out,
                             GCancellable           *cancellable,
                             GFileProgressCallback   progress_callback,
                             gpointer                progress_callback_data,
                             GError                **error)
{
  int buffer[2];
  gboolean res;
  goffset total_size;
  loff_t offset_in;
  loff_t offset_out;
  int fd_in, fd_out;

  fd_in = g_file_descriptor_based_get_fd (G_FILE_DESCRIPTOR_BASED (in));
  fd_out = g_file_descriptor_based_get_fd (G_FILE_DESCRIPTOR_BASED (out));

  if (pipe (buffer) != 0)
    {
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                           "Pipe creation failed");
      return FALSE;
    }

  total_size = -1;
  /* avoid performance impact of querying total size when it's not needed */
  if (progress_callback)
    {
      struct stat sbuf;

      if (fstat (fd_in, &sbuf) == 0)
        total_size = sbuf.st_size;
    }

  if (total_size == -1)
    total_size = 0;

  offset_in = offset_out = 0;
  res = FALSE;
  while (TRUE)
    {
      long n_read;
      long n_written;

      if (g_cancellable_set_error_if_cancelled (cancellable, error))
        break;

      if (!do_splice (fd_in, &offset_in, buffer[1], NULL, 1024*64, &n_read, error))
        break;

      if (n_read == 0)
        {
          res = TRUE;
          break;
        }

      while (n_read > 0)
        {
          if (g_cancellable_set_error_if_cancelled (cancellable, error))
            goto out;

          if (!do_splice (buffer[0], NULL, fd_out, &offset_out, n_read, &n_written, error))
            goto out;

          n_read -= n_written;
        }

      if (progress_callback)
        progress_callback (offset_in, total_size, progress_callback_data);
    }

  /* Make sure we send full copied size */
  if (progress_callback)
    progress_callback (offset_in, total_size, progress_callback_data);

 out:
  close (buffer[0]);
  close (buffer[1]);

  return res;
}
#endif

static gboolean
file_copy_fallback (GFile                  *source,
		    GFile                  *destination,
		    GFileCopyFlags          flags,
		    GCancellable           *cancellable,
		    GFileProgressCallback   progress_callback,
		    gpointer                progress_callback_data,
		    GError                **error)
{
  GInputStream *in;
  GOutputStream *out;
  GFileInfo *info;
  const char *target;
  gboolean result;
#ifdef HAVE_SPLICE
  gboolean fallback = TRUE;
#endif

  /* need to know the file type */
  info = g_file_query_info (source,
			    G_FILE_ATTRIBUTE_STANDARD_TYPE "," G_FILE_ATTRIBUTE_STANDARD_SYMLINK_TARGET,
			    G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
			    cancellable,
			    error);

  if (info == NULL)
	  return FALSE;

  /* Maybe copy the symlink? */
  if ((flags & G_FILE_COPY_NOFOLLOW_SYMLINKS) &&
      g_file_info_get_file_type (info) == G_FILE_TYPE_SYMBOLIC_LINK)
    {
      target = g_file_info_get_symlink_target (info);
      if (target)
	{
	  if (!copy_symlink (destination, flags, cancellable, target, error))
	    {
	      g_object_unref (info);
	      return FALSE;
	    }
	  
	  g_object_unref (info);
	  goto copied_file;
	}
        /* ... else fall back on a regular file copy */
	g_object_unref (info);
    }
  /* Handle "special" files (pipes, device nodes, ...)? */
  else if (g_file_info_get_file_type (info) == G_FILE_TYPE_SPECIAL)
    {
      /* FIXME: could try to recreate device nodes and others? */

      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                           _("Can't copy special file"));
      g_object_unref (info);
      return FALSE;
    }
  /* Everything else should just fall back on a regular copy. */
  else
    g_object_unref (info);

  in = open_source_for_copy (source, destination, flags, cancellable, error);
  if (in == NULL)
    return FALSE;
  
  if (flags & G_FILE_COPY_OVERWRITE)
    {
      out = (GOutputStream *)g_file_replace (destination,
					     NULL,
					     flags & G_FILE_COPY_BACKUP,
                                             G_FILE_CREATE_REPLACE_DESTINATION,
					     cancellable, error);
    }
  else
    {
      out = (GOutputStream *)g_file_create (destination, 0, cancellable, error);
    }

  if (out == NULL)
    {
      g_object_unref (in);
      return FALSE;
    }

#ifdef HAVE_SPLICE
  if (G_IS_FILE_DESCRIPTOR_BASED (in) && G_IS_FILE_DESCRIPTOR_BASED (out))
    {
      GError *splice_err = NULL;

      result = splice_stream_with_progress (in, out, cancellable,
                                            progress_callback, progress_callback_data,
                                            &splice_err);

      if (result || !g_error_matches (splice_err, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED))
        {
          fallback = FALSE;
          if (!result)
            g_propagate_error (error, splice_err);
        }
      else
        g_clear_error (&splice_err);
    }

  if (fallback)
#endif
    result = copy_stream_with_progress (in, out, source, cancellable,
		                        progress_callback, progress_callback_data,
		                        error);

  /* Don't care about errors in source here */
  g_input_stream_close (in, cancellable, NULL);

  /* But write errors on close are bad! */
  if (!g_output_stream_close (out, cancellable, result ? error : NULL))
    result = FALSE;

  g_object_unref (in);
  g_object_unref (out);

  if (result == FALSE)
    return FALSE;

 copied_file:
  /* Ignore errors here. Failure to copy metadata is not a hard error */
  g_file_copy_attributes (source, destination,
			  flags, cancellable, NULL);
  
  return TRUE;
}

/**
 * g_file_copy:
 * @source: input #GFile.
 * @destination: destination #GFile
 * @flags: set of #GFileCopyFlags
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * @progress_callback: function to callback with progress information
 * @progress_callback_data: user data to pass to @progress_callback
 * @error: #GError to set on error, or %NULL
 *
 * Copies the file @source to the location specified by @destination.
 * Can not handle recursive copies of directories.
 *
 * If the flag #G_FILE_COPY_OVERWRITE is specified an already
 * existing @destination file is overwritten.
 *
 * If the flag #G_FILE_COPY_NOFOLLOW_SYMLINKS is specified then symlinks
 * will be copied as symlinks, otherwise the target of the
 * @source symlink will be copied.
 *
 * If @cancellable is not %NULL, then the operation can be cancelled by
 * triggering the cancellable object from another thread. If the operation
 * was cancelled, the error %G_IO_ERROR_CANCELLED will be returned. 
 * 
 * If @progress_callback is not %NULL, then the operation can be monitored by
 * setting this to a #GFileProgressCallback function. @progress_callback_data
 * will be passed to this function. It is guaranteed that this callback will
 * be called after all data has been transferred with the total number of bytes
 * copied during the operation.
 * 
 * If the @source file does not exist then the G_IO_ERROR_NOT_FOUND
 * error is returned, independent on the status of the @destination.
 *
 * If #G_FILE_COPY_OVERWRITE is not specified and the target exists, then the
 * error G_IO_ERROR_EXISTS is returned.
 *
 * If trying to overwrite a file over a directory the G_IO_ERROR_IS_DIRECTORY
 * error is returned. If trying to overwrite a directory with a directory the
 * G_IO_ERROR_WOULD_MERGE error is returned.
 *
 * If the source is a directory and the target does not exist, or #G_FILE_COPY_OVERWRITE is
 * specified and the target is a file, then the G_IO_ERROR_WOULD_RECURSE error
 * is returned.
 *
 * If you are interested in copying the #GFile object itself (not the on-disk
 * file), see g_file_dup().
 *
 * Returns: %TRUE on success, %FALSE otherwise.
 **/
gboolean
g_file_copy (GFile                  *source,
	     GFile                  *destination,
	     GFileCopyFlags          flags,
	     GCancellable           *cancellable,
	     GFileProgressCallback   progress_callback,
	     gpointer                progress_callback_data,
	     GError                **error)
{
  GFileIface *iface;
  GError *my_error;
  gboolean res;

  g_return_val_if_fail (G_IS_FILE (source), FALSE);
  g_return_val_if_fail (G_IS_FILE (destination), FALSE);

  if (g_cancellable_set_error_if_cancelled (cancellable, error))
    return FALSE;
  
  iface = G_FILE_GET_IFACE (destination);
  if (iface->copy)
    {
      my_error = NULL;
      res = (* iface->copy) (source, destination,
			     flags, cancellable,
			     progress_callback, progress_callback_data,
			     &my_error);
      
      if (res)
	return TRUE;
      
      if (my_error->domain != G_IO_ERROR || my_error->code != G_IO_ERROR_NOT_SUPPORTED)
	{
	  g_propagate_error (error, my_error);
	      return FALSE;
	}
      else
	g_clear_error (&my_error);
    }

  /* If the types are different, and the destination method failed
     also try the source method */
  if (G_OBJECT_TYPE (source) != G_OBJECT_TYPE (destination))
    {
      iface = G_FILE_GET_IFACE (source);
      
      if (iface->copy)
	{
	  my_error = NULL;
	  res = (* iface->copy) (source, destination,
				 flags, cancellable,
				 progress_callback, progress_callback_data,
				 &my_error);
	  
	  if (res)
	    return TRUE;
	  
	  if (my_error->domain != G_IO_ERROR || my_error->code != G_IO_ERROR_NOT_SUPPORTED)
	    {
	      g_propagate_error (error, my_error);
	      return FALSE;
	    }
	  else
	    g_clear_error (&my_error);
	}
    }
  
  return file_copy_fallback (source, destination, flags, cancellable,
			     progress_callback, progress_callback_data,
			     error);
}

/**
 * g_file_copy_async:
 * @source: input #GFile.
 * @destination: destination #GFile
 * @flags: set of #GFileCopyFlags
 * @io_priority: the <link linkend="io-priority">I/O priority</link> 
 *     of the request. 
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * @progress_callback: function to callback with progress information
 * @progress_callback_data: user data to pass to @progress_callback
 * @callback: a #GAsyncReadyCallback to call when the request is satisfied
 * @user_data: the data to pass to callback function
 *
 * Copies the file @source to the location specified by @destination 
 * asynchronously. For details of the behaviour, see g_file_copy().
 *
 * If @progress_callback is not %NULL, then that function that will be called
 * just like in g_file_copy(), however the callback will run in the main loop,
 * not in the thread that is doing the I/O operation.
 *
 * When the operation is finished, @callback will be called. You can then call
 * g_file_copy_finish() to get the result of the operation.
 **/
void
g_file_copy_async (GFile                  *source,
		   GFile                  *destination,
		   GFileCopyFlags          flags,
		   int                     io_priority,
		   GCancellable           *cancellable,
		   GFileProgressCallback   progress_callback,
		   gpointer                progress_callback_data,
		   GAsyncReadyCallback     callback,
		   gpointer                user_data)
{
  GFileIface *iface;

  g_return_if_fail (G_IS_FILE (source));
  g_return_if_fail (G_IS_FILE (destination));

  iface = G_FILE_GET_IFACE (source);
  (* iface->copy_async) (source,
			 destination,
			 flags,
			 io_priority,
			 cancellable,
			 progress_callback,
			 progress_callback_data,
			 callback,
			 user_data);
}

/**
 * g_file_copy_finish:
 * @file: input #GFile.
 * @res: a #GAsyncResult. 
 * @error: a #GError, or %NULL
 * 
 * Finishes copying the file started with 
 * g_file_copy_async().
 * 
 * Returns: a %TRUE on success, %FALSE on error.
 **/
gboolean
g_file_copy_finish (GFile        *file,
		    GAsyncResult *res,
		    GError      **error)
{
  GFileIface *iface;
  
  g_return_val_if_fail (G_IS_FILE (file), FALSE);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (res), FALSE);

  if (G_IS_SIMPLE_ASYNC_RESULT (res))
    {
      GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (res);
      
      if (g_simple_async_result_propagate_error (simple, error))
	return FALSE;
    }
  
  iface = G_FILE_GET_IFACE (file);
  return (* iface->copy_finish) (file, res, error);
}

/**
 * g_file_move:
 * @source: #GFile pointing to the source location.
 * @destination: #GFile pointing to the destination location.
 * @flags: set of #GFileCopyFlags.
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * @progress_callback: #GFileProgressCallback function for updates.
 * @progress_callback_data: gpointer to user data for the callback function.
 * @error: #GError for returning error conditions, or %NULL
 *
 *
 * Tries to move the file or directory @source to the location specified by @destination.
 * If native move operations are supported then this is used, otherwise a copy + delete
 * fallback is used. The native implementation may support moving directories (for instance
 * on moves inside the same filesystem), but the fallback code does not.
 * 
 * If the flag #G_FILE_COPY_OVERWRITE is specified an already
 * existing @destination file is overwritten.
 *
 * If the flag #G_FILE_COPY_NOFOLLOW_SYMLINKS is specified then symlinks
 * will be copied as symlinks, otherwise the target of the
 * @source symlink will be copied.
 *
 * If @cancellable is not %NULL, then the operation can be cancelled by
 * triggering the cancellable object from another thread. If the operation
 * was cancelled, the error %G_IO_ERROR_CANCELLED will be returned. 
 * 
 * If @progress_callback is not %NULL, then the operation can be monitored by
 * setting this to a #GFileProgressCallback function. @progress_callback_data
 * will be passed to this function. It is guaranteed that this callback will
 * be called after all data has been transferred with the total number of bytes
 * copied during the operation.
 * 
 * If the @source file does not exist then the G_IO_ERROR_NOT_FOUND
 * error is returned, independent on the status of the @destination.
 *
 * If #G_FILE_COPY_OVERWRITE is not specified and the target exists, then the
 * error G_IO_ERROR_EXISTS is returned.
 *
 * If trying to overwrite a file over a directory the G_IO_ERROR_IS_DIRECTORY
 * error is returned. If trying to overwrite a directory with a directory the
 * G_IO_ERROR_WOULD_MERGE error is returned.
 *
 * If the source is a directory and the target does not exist, or #G_FILE_COPY_OVERWRITE is
 * specified and the target is a file, then the G_IO_ERROR_WOULD_RECURSE error
 * may be returned (if the native move operation isn't available).
 *
 * Returns: %TRUE on successful move, %FALSE otherwise.
 **/
gboolean
g_file_move (GFile                  *source,
	     GFile                  *destination,
	     GFileCopyFlags          flags,
	     GCancellable           *cancellable,
	     GFileProgressCallback   progress_callback,
	     gpointer                progress_callback_data,
	     GError                **error)
{
  GFileIface *iface;
  GError *my_error;
  gboolean res;

  g_return_val_if_fail (G_IS_FILE (source), FALSE);
  g_return_val_if_fail (G_IS_FILE (destination), FALSE);

  if (g_cancellable_set_error_if_cancelled (cancellable, error))
    return FALSE;
  
  iface = G_FILE_GET_IFACE (destination);
  if (iface->move)
    {
      my_error = NULL;
      res = (* iface->move) (source, destination,
			     flags, cancellable,
			     progress_callback, progress_callback_data,
			     &my_error);
      
      if (res)
	return TRUE;
      
      if (my_error->domain != G_IO_ERROR || my_error->code != G_IO_ERROR_NOT_SUPPORTED)
	{
	  g_propagate_error (error, my_error);
	  return FALSE;
	}
    }

  /* If the types are different, and the destination method failed
     also try the source method */
  if (G_OBJECT_TYPE (source) != G_OBJECT_TYPE (destination))
    {
      iface = G_FILE_GET_IFACE (source);
      
      if (iface->move)
	{
	  my_error = NULL;
	  res = (* iface->move) (source, destination,
				 flags, cancellable,
				 progress_callback, progress_callback_data,
				 &my_error);
	  
	  if (res)
	    return TRUE;
	  
	  if (my_error->domain != G_IO_ERROR || my_error->code != G_IO_ERROR_NOT_SUPPORTED)
	    {
	      g_propagate_error (error, my_error);
	      return FALSE;
	    }
	}
    }
  
  if (flags & G_FILE_COPY_NO_FALLBACK_FOR_MOVE)
    {  
      g_set_error_literal (error, G_IO_ERROR,
                           G_IO_ERROR_NOT_SUPPORTED,
                           _("Operation not supported"));
      return FALSE;
    }
  
  flags |= G_FILE_COPY_ALL_METADATA;
  if (!g_file_copy (source, destination, flags, cancellable,
		    progress_callback, progress_callback_data,
		    error))
    return FALSE;

  return g_file_delete (source, cancellable, error);
}

/**
 * g_file_make_directory
 * @file: input #GFile.
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * @error: a #GError, or %NULL 
 *
 * Creates a directory. Note that this will only create a child directory of
 * the immediate parent directory of the path or URI given by the #GFile. To 
 * recursively create directories, see g_file_make_directory_with_parents().
 * This function will fail if the parent directory does not exist, setting 
 * @error to %G_IO_ERROR_NOT_FOUND. If the file system doesn't support creating
 * directories, this function will fail, setting @error to 
 * %G_IO_ERROR_NOT_SUPPORTED.
 *
 * For a local #GFile the newly created directory will have the default
 * (current) ownership and permissions of the current process.
 * 
 * If @cancellable is not %NULL, then the operation can be cancelled by
 * triggering the cancellable object from another thread. If the operation
 * was cancelled, the error %G_IO_ERROR_CANCELLED will be returned. 
 * 
 * Returns: %TRUE on successful creation, %FALSE otherwise.
 **/
gboolean
g_file_make_directory (GFile         *file,
		       GCancellable  *cancellable,
		       GError       **error)
{
  GFileIface *iface;

  g_return_val_if_fail (G_IS_FILE (file), FALSE);

  if (g_cancellable_set_error_if_cancelled (cancellable, error))
    return FALSE;
  
  iface = G_FILE_GET_IFACE (file);

  if (iface->make_directory == NULL)
    {
      g_set_error_literal (error, G_IO_ERROR,
                           G_IO_ERROR_NOT_SUPPORTED,
                           _("Operation not supported"));
      return FALSE;
    }
  
  return (* iface->make_directory) (file, cancellable, error);
}

/**
 * g_file_make_directory_with_parents:
 * @file: input #GFile.
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * @error: a #GError, or %NULL 
 *
 * Creates a directory and any parent directories that may not exist similar to
 * 'mkdir -p'. If the file system does not support creating directories, this
 * function will fail, setting @error to %G_IO_ERROR_NOT_SUPPORTED.
 * 
 * For a local #GFile the newly created directories will have the default
 * (current) ownership and permissions of the current process.
 * 
 * If @cancellable is not %NULL, then the operation can be cancelled by
 * triggering the cancellable object from another thread. If the operation
 * was cancelled, the error %G_IO_ERROR_CANCELLED will be returned. 
 * 
 * Returns: %TRUE if all directories have been successfully created, %FALSE
 * otherwise.
 *
 * Since: 2.18
 **/
gboolean
g_file_make_directory_with_parents (GFile         *file,
		                    GCancellable  *cancellable,
		                    GError       **error)
{
  gboolean result;
  GFile *parent_file, *work_file;
  GList *list = NULL, *l;
  GError *my_error = NULL;

  if (g_cancellable_set_error_if_cancelled (cancellable, error))
    return FALSE;
  
  result = g_file_make_directory (file, cancellable, &my_error);
  if (result || my_error->code != G_IO_ERROR_NOT_FOUND) 
    {
      if (my_error)
        g_propagate_error (error, my_error);
      return result;
    }
  
  work_file = file;
  
  while (!result && my_error->code == G_IO_ERROR_NOT_FOUND) 
    {
      g_clear_error (&my_error);
    
      parent_file = g_file_get_parent (work_file);
      if (parent_file == NULL)
        break;
      result = g_file_make_directory (parent_file, cancellable, &my_error);
    
      if (!result && my_error->code == G_IO_ERROR_NOT_FOUND)
        list = g_list_prepend (list, parent_file);

      work_file = parent_file;
    }

  for (l = list; result && l; l = l->next)
    {
      result = g_file_make_directory ((GFile *) l->data, cancellable, &my_error);
    }
  
  /* Clean up */
  while (list != NULL) 
    {
      g_object_unref ((GFile *) list->data);
      list = g_list_remove (list, list->data);
    }

  if (!result) 
    {
      g_propagate_error (error, my_error);
      return result;
    }
  
  return g_file_make_directory (file, cancellable, error);
}

/**
 * g_file_make_symbolic_link:
 * @file: input #GFile.
 * @symlink_value: a string with the value of the new symlink.
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * @error: a #GError. 
 * 
 * Creates a symbolic link.
 *
 * If @cancellable is not %NULL, then the operation can be cancelled by
 * triggering the cancellable object from another thread. If the operation
 * was cancelled, the error %G_IO_ERROR_CANCELLED will be returned. 
 * 
 * Returns: %TRUE on the creation of a new symlink, %FALSE otherwise.
 **/
gboolean
g_file_make_symbolic_link (GFile         *file,
			   const char    *symlink_value,
			   GCancellable  *cancellable,
			   GError       **error)
{
  GFileIface *iface;

  g_return_val_if_fail (G_IS_FILE (file), FALSE);
  g_return_val_if_fail (symlink_value != NULL, FALSE);

  if (g_cancellable_set_error_if_cancelled (cancellable, error))
    return FALSE;

  if (*symlink_value == '\0')
    {
      g_set_error_literal (error, G_IO_ERROR,
                           G_IO_ERROR_INVALID_ARGUMENT,
                           _("Invalid symlink value given"));
      return FALSE;
    }
  
  iface = G_FILE_GET_IFACE (file);

  if (iface->make_symbolic_link == NULL)
    {
      g_set_error_literal (error, G_IO_ERROR,
                           G_IO_ERROR_NOT_SUPPORTED,
                           _("Operation not supported"));
      return FALSE;
    }
  
  return (* iface->make_symbolic_link) (file, symlink_value, cancellable, error);
}

/**
 * g_file_delete:
 * @file: input #GFile.
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * @error: a #GError, or %NULL 
 * 
 * Deletes a file. If the @file is a directory, it will only be deleted if it 
 * is empty.
 * 
 * If @cancellable is not %NULL, then the operation can be cancelled by
 * triggering the cancellable object from another thread. If the operation
 * was cancelled, the error %G_IO_ERROR_CANCELLED will be returned. 
 * 
 * Returns: %TRUE if the file was deleted. %FALSE otherwise.
 **/
gboolean
g_file_delete (GFile         *file,
	       GCancellable  *cancellable,
	       GError       **error)
{
  GFileIface *iface;
  
  g_return_val_if_fail (G_IS_FILE (file), FALSE);

  if (g_cancellable_set_error_if_cancelled (cancellable, error))
    return FALSE;
  
  iface = G_FILE_GET_IFACE (file);

  if (iface->delete_file == NULL)
    {
      g_set_error_literal (error, G_IO_ERROR,
                           G_IO_ERROR_NOT_SUPPORTED,
                           _("Operation not supported"));
      return FALSE;
    }
  
  return (* iface->delete_file) (file, cancellable, error);
}

/**
 * g_file_trash:
 * @file: #GFile to send to trash.
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * @error: a #GError, or %NULL
 *
 * Sends @file to the "Trashcan", if possible. This is similar to
 * deleting it, but the user can recover it before emptying the trashcan.
 * Not all file systems support trashing, so this call can return the
 * %G_IO_ERROR_NOT_SUPPORTED error.
 *
 *
 * If @cancellable is not %NULL, then the operation can be cancelled by
 * triggering the cancellable object from another thread. If the operation
 * was cancelled, the error %G_IO_ERROR_CANCELLED will be returned. 
 * 
 * Returns: %TRUE on successful trash, %FALSE otherwise.
 **/
gboolean
g_file_trash (GFile         *file,
	      GCancellable  *cancellable,
	      GError       **error)
{
  GFileIface *iface;
  
  g_return_val_if_fail (G_IS_FILE (file), FALSE);

  if (g_cancellable_set_error_if_cancelled (cancellable, error))
    return FALSE;
  
  iface = G_FILE_GET_IFACE (file);

  if (iface->trash == NULL)
    {
      g_set_error_literal (error,
                           G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                           _("Trash not supported"));
      return FALSE;
    }
  
  return (* iface->trash) (file, cancellable, error);
}

/**
 * g_file_set_display_name:
 * @file: input #GFile.
 * @display_name: a string.
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * @error: a #GError, or %NULL
 * 
 * Renames @file to the specified display name.
 *
 * The display name is converted from UTF8 to the correct encoding for the target
 * filesystem if possible and the @file is renamed to this.
 * 
 * If you want to implement a rename operation in the user interface the edit name
 * (#G_FILE_ATTRIBUTE_STANDARD_EDIT_NAME) should be used as the initial value in the rename
 * widget, and then the result after editing should be passed to g_file_set_display_name().
 *
 * On success the resulting converted filename is returned.
 * 
 * If @cancellable is not %NULL, then the operation can be cancelled by
 * triggering the cancellable object from another thread. If the operation
 * was cancelled, the error %G_IO_ERROR_CANCELLED will be returned. 
 * 
 * Returns: a #GFile specifying what @file was renamed to, or %NULL 
 *     if there was an error.
 *     Free the returned object with g_object_unref().
 **/
GFile *
g_file_set_display_name (GFile         *file,
			 const char    *display_name,
			 GCancellable  *cancellable,
			 GError       **error)
{
  GFileIface *iface;
  
  g_return_val_if_fail (G_IS_FILE (file), NULL);
  g_return_val_if_fail (display_name != NULL, NULL);

  if (strchr (display_name, G_DIR_SEPARATOR) != NULL)
    {
      g_set_error (error,
		   G_IO_ERROR,
		   G_IO_ERROR_INVALID_ARGUMENT,
		   _("File names cannot contain '%c'"), G_DIR_SEPARATOR);
      return NULL;
    }
  
  if (g_cancellable_set_error_if_cancelled (cancellable, error))
    return NULL;
  
  iface = G_FILE_GET_IFACE (file);

  return (* iface->set_display_name) (file, display_name, cancellable, error);
}

/**
 * g_file_set_display_name_async:
 * @file: input #GFile.
 * @display_name: a string.
 * @io_priority: the <link linkend="io-priority">I/O priority</link> 
 *     of the request. 
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * @callback: a #GAsyncReadyCallback to call when the request is satisfied
 * @user_data: the data to pass to callback function
 * 
 * Asynchronously sets the display name for a given #GFile.
 * 
 * For more details, see g_file_set_display_name() which is
 * the synchronous version of this call.
 *
 * When the operation is finished, @callback will be called. You can then call
 * g_file_set_display_name_finish() to get the result of the operation.
 **/
void
g_file_set_display_name_async (GFile               *file,
			       const char          *display_name,
			       int                  io_priority,
			       GCancellable        *cancellable,
			       GAsyncReadyCallback  callback,
			       gpointer             user_data)
{
  GFileIface *iface;
  
  g_return_if_fail (G_IS_FILE (file));
  g_return_if_fail (display_name != NULL);

  iface = G_FILE_GET_IFACE (file);
  (* iface->set_display_name_async) (file,
				     display_name,
				     io_priority,
				     cancellable,
				     callback,
				     user_data);
}

/**
 * g_file_set_display_name_finish:
 * @file: input #GFile.
 * @res: a #GAsyncResult. 
 * @error: a #GError, or %NULL
 * 
 * Finishes setting a display name started with 
 * g_file_set_display_name_async().
 * 
 * Returns: a #GFile or %NULL on error.
 *     Free the returned object with g_object_unref().
 **/
GFile *
g_file_set_display_name_finish (GFile         *file,
				GAsyncResult  *res,
				GError       **error)
{
  GFileIface *iface;
  
  g_return_val_if_fail (G_IS_FILE (file), NULL);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (res), NULL);

  if (G_IS_SIMPLE_ASYNC_RESULT (res))
    {
      GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (res);
      if (g_simple_async_result_propagate_error (simple, error))
	return NULL;
    }
  
  iface = G_FILE_GET_IFACE (file);
  return (* iface->set_display_name_finish) (file, res, error);
}

/**
 * g_file_query_settable_attributes:
 * @file: input #GFile.
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * @error: a #GError, or %NULL
 * 
 * Obtain the list of settable attributes for the file.
 *
 * Returns the type and full attribute name of all the attributes 
 * that can be set on this file. This doesn't mean setting it will always 
 * succeed though, you might get an access failure, or some specific 
 * file may not support a specific attribute.
 *
 * If @cancellable is not %NULL, then the operation can be cancelled by
 * triggering the cancellable object from another thread. If the operation
 * was cancelled, the error %G_IO_ERROR_CANCELLED will be returned. 
 * 
 * Returns: a #GFileAttributeInfoList describing the settable attributes.
 * When you are done with it, release it with g_file_attribute_info_list_unref()
 **/
GFileAttributeInfoList *
g_file_query_settable_attributes (GFile         *file,
				  GCancellable  *cancellable,
				  GError       **error)
{
  GFileIface *iface;
  GError *my_error;
  GFileAttributeInfoList *list;

  g_return_val_if_fail (G_IS_FILE (file), NULL);

  if (g_cancellable_set_error_if_cancelled (cancellable, error))
    return NULL;
  
  iface = G_FILE_GET_IFACE (file);

  if (iface->query_settable_attributes == NULL)
    return g_file_attribute_info_list_new ();

  my_error = NULL;
  list = (* iface->query_settable_attributes) (file, cancellable, &my_error);
  
  if (list == NULL)
    {
      if (my_error->domain == G_IO_ERROR && my_error->code == G_IO_ERROR_NOT_SUPPORTED)
	{
	  list = g_file_attribute_info_list_new ();
	  g_error_free (my_error);
	}
      else
	g_propagate_error (error, my_error);
    }
  
  return list;
}

/**
 * g_file_query_writable_namespaces:
 * @file: input #GFile.
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * @error: a #GError, or %NULL
 * 
 * Obtain the list of attribute namespaces where new attributes 
 * can be created by a user. An example of this is extended
 * attributes (in the "xattr" namespace).
 *
 * If @cancellable is not %NULL, then the operation can be cancelled by
 * triggering the cancellable object from another thread. If the operation
 * was cancelled, the error %G_IO_ERROR_CANCELLED will be returned. 
 * 
 * Returns: a #GFileAttributeInfoList describing the writable namespaces.
 * When you are done with it, release it with g_file_attribute_info_list_unref()
 **/
GFileAttributeInfoList *
g_file_query_writable_namespaces (GFile         *file,
				  GCancellable  *cancellable,
				  GError       **error)
{
  GFileIface *iface;
  GError *my_error;
  GFileAttributeInfoList *list;
  
  g_return_val_if_fail (G_IS_FILE (file), NULL);

  if (g_cancellable_set_error_if_cancelled (cancellable, error))
    return NULL;
  
  iface = G_FILE_GET_IFACE (file);

  if (iface->query_writable_namespaces == NULL)
    return g_file_attribute_info_list_new ();

  my_error = NULL;
  list = (* iface->query_writable_namespaces) (file, cancellable, &my_error);
  
  if (list == NULL)
    {
      if (my_error->domain == G_IO_ERROR && my_error->code == G_IO_ERROR_NOT_SUPPORTED)
	{
	  list = g_file_attribute_info_list_new ();
	  g_error_free (my_error);
	}
      else
	g_propagate_error (error, my_error);
    }
  
  return list;
}

/**
 * g_file_set_attribute:
 * @file: input #GFile.
 * @attribute: a string containing the attribute's name.
 * @type: The type of the attribute
 * @value_p: a pointer to the value (or the pointer itself if the type is a pointer type)
 * @flags: a set of #GFileQueryInfoFlags.
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * @error: a #GError, or %NULL
 * 
 * Sets an attribute in the file with attribute name @attribute to @value.
 * 
 * If @cancellable is not %NULL, then the operation can be cancelled by
 * triggering the cancellable object from another thread. If the operation
 * was cancelled, the error %G_IO_ERROR_CANCELLED will be returned. 
 * 
 * Returns: %TRUE if the attribute was set, %FALSE otherwise.
 **/
gboolean
g_file_set_attribute (GFile                      *file,
		      const char                 *attribute,
		      GFileAttributeType          type,
		      gpointer                    value_p,
		      GFileQueryInfoFlags         flags,
		      GCancellable               *cancellable,
		      GError                    **error)
{
  GFileIface *iface;
  
  g_return_val_if_fail (G_IS_FILE (file), FALSE);
  g_return_val_if_fail (attribute != NULL && *attribute != '\0', FALSE);

  if (g_cancellable_set_error_if_cancelled (cancellable, error))
    return FALSE;
  
  iface = G_FILE_GET_IFACE (file);

  if (iface->set_attribute == NULL)
    {
      g_set_error_literal (error, G_IO_ERROR,
                           G_IO_ERROR_NOT_SUPPORTED,
                           _("Operation not supported"));
      return FALSE;
    }

  return (* iface->set_attribute) (file, attribute, type, value_p, flags, cancellable, error);
}

/**
 * g_file_set_attributes_from_info:
 * @file: input #GFile.
 * @info: a #GFileInfo.
 * @flags: #GFileQueryInfoFlags
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * @error: a #GError, or %NULL 
 * 
 * Tries to set all attributes in the #GFileInfo on the target values, 
 * not stopping on the first error.
 * 
 * If there is any error during this operation then @error will be set to
 * the first error. Error on particular fields are flagged by setting 
 * the "status" field in the attribute value to 
 * %G_FILE_ATTRIBUTE_STATUS_ERROR_SETTING, which means you can also detect
 * further errors.
 *
 * If @cancellable is not %NULL, then the operation can be cancelled by
 * triggering the cancellable object from another thread. If the operation
 * was cancelled, the error %G_IO_ERROR_CANCELLED will be returned. 
 * 
 * Returns: %TRUE if there was any error, %FALSE otherwise.
 **/
gboolean
g_file_set_attributes_from_info (GFile                *file,
				 GFileInfo            *info,
				 GFileQueryInfoFlags   flags,
				 GCancellable         *cancellable,
				 GError              **error)
{
  GFileIface *iface;

  g_return_val_if_fail (G_IS_FILE (file), FALSE);
  g_return_val_if_fail (G_IS_FILE_INFO (info), FALSE);

  if (g_cancellable_set_error_if_cancelled (cancellable, error))
    return FALSE;
  
  g_file_info_clear_status (info);
  
  iface = G_FILE_GET_IFACE (file);

  return (* iface->set_attributes_from_info) (file, 
                                              info, 
                                              flags, 
                                              cancellable, 
                                              error);
}


static gboolean
g_file_real_set_attributes_from_info (GFile                *file,
				      GFileInfo            *info,
				      GFileQueryInfoFlags   flags,
				      GCancellable         *cancellable,
				      GError              **error)
{
  char **attributes;
  int i;
  gboolean res;
  GFileAttributeValue *value;
  
  res = TRUE;
  
  attributes = g_file_info_list_attributes (info, NULL);

  for (i = 0; attributes[i] != NULL; i++)
    {
      value = _g_file_info_get_attribute_value (info, attributes[i]);

      if (value->status != G_FILE_ATTRIBUTE_STATUS_UNSET)
	continue;

      if (!g_file_set_attribute (file, attributes[i],
				 value->type, _g_file_attribute_value_peek_as_pointer (value),
				 flags, cancellable, error))
	{
	  value->status = G_FILE_ATTRIBUTE_STATUS_ERROR_SETTING;
	  res = FALSE;
	  /* Don't set error multiple times */
	  error = NULL;
	}
      else
	value->status = G_FILE_ATTRIBUTE_STATUS_SET;
    }
  
  g_strfreev (attributes);
  
  return res;
}

/**
 * g_file_set_attributes_async:
 * @file: input #GFile.
 * @info: a #GFileInfo.
 * @flags: a #GFileQueryInfoFlags.
 * @io_priority: the <link linkend="io-priority">I/O priority</link> 
 *     of the request. 
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * @callback: a #GAsyncReadyCallback. 
 * @user_data: a #gpointer.
 *
 * Asynchronously sets the attributes of @file with @info.
 * 
 * For more details, see g_file_set_attributes_from_info() which is
 * the synchronous version of this call.
 *
 * When the operation is finished, @callback will be called. You can then call
 * g_file_set_attributes_finish() to get the result of the operation.
 **/
void
g_file_set_attributes_async (GFile               *file,
			     GFileInfo           *info,
			     GFileQueryInfoFlags  flags,
			     int                  io_priority,
			     GCancellable        *cancellable,
			     GAsyncReadyCallback  callback,
			     gpointer             user_data)
{
  GFileIface *iface;
  
  g_return_if_fail (G_IS_FILE (file));
  g_return_if_fail (G_IS_FILE_INFO (info));

  iface = G_FILE_GET_IFACE (file);
  (* iface->set_attributes_async) (file, 
                                   info, 
                                   flags, 
                                   io_priority, 
                                   cancellable, 
                                   callback, 
                                   user_data);
}

/**
 * g_file_set_attributes_finish:
 * @file: input #GFile.
 * @result: a #GAsyncResult.
 * @info: a #GFileInfo.
 * @error: a #GError, or %NULL
 * 
 * Finishes setting an attribute started in g_file_set_attributes_async().
 * 
 * Returns: %TRUE if the attributes were set correctly, %FALSE otherwise.
 **/
gboolean
g_file_set_attributes_finish (GFile         *file,
			      GAsyncResult  *result,
			      GFileInfo    **info,
			      GError       **error)
{
  GFileIface *iface;
  
  g_return_val_if_fail (G_IS_FILE (file), FALSE);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

  /* No standard handling of errors here, as we must set info even
   * on errors 
   */
  iface = G_FILE_GET_IFACE (file);
  return (* iface->set_attributes_finish) (file, result, info, error);
}

/**
 * g_file_set_attribute_string:
 * @file: input #GFile.
 * @attribute: a string containing the attribute's name.
 * @value: a string containing the attribute's value.
 * @flags: #GFileQueryInfoFlags.
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * @error: a #GError, or %NULL
 * 
 * Sets @attribute of type %G_FILE_ATTRIBUTE_TYPE_STRING to @value. 
 * If @attribute is of a different type, this operation will fail.
 * 
 * If @cancellable is not %NULL, then the operation can be cancelled by
 * triggering the cancellable object from another thread. If the operation
 * was cancelled, the error %G_IO_ERROR_CANCELLED will be returned. 
 * 
 * Returns: %TRUE if the @attribute was successfully set, %FALSE otherwise.
 **/
gboolean
g_file_set_attribute_string (GFile                *file,
			     const char           *attribute,
			     const char           *value,
			     GFileQueryInfoFlags   flags,
			     GCancellable         *cancellable,
			     GError              **error)
{
  return g_file_set_attribute (file, attribute,
			       G_FILE_ATTRIBUTE_TYPE_STRING, (gpointer)value,
			       flags, cancellable, error);
}

/**
 * g_file_set_attribute_byte_string:
 * @file: input #GFile.
 * @attribute: a string containing the attribute's name.
 * @value: a string containing the attribute's new value.
 * @flags: a #GFileQueryInfoFlags.
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * @error: a #GError, or %NULL
 * 
 * Sets @attribute of type %G_FILE_ATTRIBUTE_TYPE_BYTE_STRING to @value. 
 * If @attribute is of a different type, this operation will fail, 
 * returning %FALSE. 
 * 
 * If @cancellable is not %NULL, then the operation can be cancelled by
 * triggering the cancellable object from another thread. If the operation
 * was cancelled, the error %G_IO_ERROR_CANCELLED will be returned. 
 * 
 * Returns: %TRUE if the @attribute was successfully set to @value 
 * in the @file, %FALSE otherwise.
 **/
gboolean
g_file_set_attribute_byte_string  (GFile                *file,
				   const char           *attribute,
				   const char           *value,
				   GFileQueryInfoFlags   flags,
				   GCancellable         *cancellable,
				   GError              **error)
{
  return g_file_set_attribute (file, attribute,
			       G_FILE_ATTRIBUTE_TYPE_BYTE_STRING, (gpointer)value,
			       flags, cancellable, error);
}

/**
 * g_file_set_attribute_uint32:
 * @file: input #GFile.
 * @attribute: a string containing the attribute's name.
 * @value: a #guint32 containing the attribute's new value.
 * @flags: a #GFileQueryInfoFlags.
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * @error: a #GError, or %NULL
 * 
 * Sets @attribute of type %G_FILE_ATTRIBUTE_TYPE_UINT32 to @value. 
 * If @attribute is of a different type, this operation will fail.
 * 
 * If @cancellable is not %NULL, then the operation can be cancelled by
 * triggering the cancellable object from another thread. If the operation
 * was cancelled, the error %G_IO_ERROR_CANCELLED will be returned. 
 * 
 * Returns: %TRUE if the @attribute was successfully set to @value 
 * in the @file, %FALSE otherwise.
 **/
gboolean
g_file_set_attribute_uint32 (GFile                *file,
			     const char           *attribute,
			     guint32               value,
			     GFileQueryInfoFlags   flags,
			     GCancellable         *cancellable,
			     GError              **error)
{
  return g_file_set_attribute (file, attribute,
			       G_FILE_ATTRIBUTE_TYPE_UINT32, &value,
			       flags, cancellable, error);
}

/**
 * g_file_set_attribute_int32:
 * @file: input #GFile.
 * @attribute: a string containing the attribute's name.
 * @value: a #gint32 containing the attribute's new value.
 * @flags: a #GFileQueryInfoFlags.
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * @error: a #GError, or %NULL
 * 
 * Sets @attribute of type %G_FILE_ATTRIBUTE_TYPE_INT32 to @value. 
 * If @attribute is of a different type, this operation will fail.
 * 
 * If @cancellable is not %NULL, then the operation can be cancelled by
 * triggering the cancellable object from another thread. If the operation
 * was cancelled, the error %G_IO_ERROR_CANCELLED will be returned. 
 * 
 * Returns: %TRUE if the @attribute was successfully set to @value 
 * in the @file, %FALSE otherwise. 
 **/
gboolean
g_file_set_attribute_int32 (GFile                *file,
			    const char           *attribute,
			    gint32                value,
			    GFileQueryInfoFlags   flags,
			    GCancellable         *cancellable,
			    GError              **error)
{
  return g_file_set_attribute (file, attribute,
			       G_FILE_ATTRIBUTE_TYPE_INT32, &value,
			       flags, cancellable, error);
}

/**
 * g_file_set_attribute_uint64:
 * @file: input #GFile. 
 * @attribute: a string containing the attribute's name.
 * @value: a #guint64 containing the attribute's new value.
 * @flags: a #GFileQueryInfoFlags.
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * @error: a #GError, or %NULL
 * 
 * Sets @attribute of type %G_FILE_ATTRIBUTE_TYPE_UINT64 to @value. 
 * If @attribute is of a different type, this operation will fail.
 * 
 * If @cancellable is not %NULL, then the operation can be cancelled by
 * triggering the cancellable object from another thread. If the operation
 * was cancelled, the error %G_IO_ERROR_CANCELLED will be returned. 
 * 
 * Returns: %TRUE if the @attribute was successfully set to @value 
 * in the @file, %FALSE otherwise.
 **/
gboolean
g_file_set_attribute_uint64 (GFile                *file,
			     const char           *attribute,
			     guint64               value,
			     GFileQueryInfoFlags   flags,
			     GCancellable         *cancellable,
			     GError              **error)
 {
  return g_file_set_attribute (file, attribute,
			       G_FILE_ATTRIBUTE_TYPE_UINT64, &value,
			       flags, cancellable, error);
}

/**
 * g_file_set_attribute_int64:
 * @file: input #GFile.
 * @attribute: a string containing the attribute's name.
 * @value: a #guint64 containing the attribute's new value.
 * @flags: a #GFileQueryInfoFlags.
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * @error: a #GError, or %NULL
 * 
 * Sets @attribute of type %G_FILE_ATTRIBUTE_TYPE_INT64 to @value. 
 * If @attribute is of a different type, this operation will fail.
 * 
 * If @cancellable is not %NULL, then the operation can be cancelled by
 * triggering the cancellable object from another thread. If the operation
 * was cancelled, the error %G_IO_ERROR_CANCELLED will be returned. 
 * 
 * Returns: %TRUE if the @attribute was successfully set, %FALSE otherwise.
 **/
gboolean
g_file_set_attribute_int64 (GFile                *file,
			    const char           *attribute,
			    gint64                value,
			    GFileQueryInfoFlags   flags,
			    GCancellable         *cancellable,
			    GError              **error)
{
  return g_file_set_attribute (file, attribute,
			       G_FILE_ATTRIBUTE_TYPE_INT64, &value,
			       flags, cancellable, error);
}

/**
 * g_file_mount_mountable:
 * @file: input #GFile.
 * @flags: flags affecting the operation
 * @mount_operation: a #GMountOperation, or %NULL to avoid user interaction.
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * @callback: a #GAsyncReadyCallback to call when the request is satisfied, or %NULL.
 * @user_data: the data to pass to callback function
 * 
 * Mounts a file of type G_FILE_TYPE_MOUNTABLE.
 * Using @mount_operation, you can request callbacks when, for instance, 
 * passwords are needed during authentication.
 *
 * If @cancellable is not %NULL, then the operation can be cancelled by
 * triggering the cancellable object from another thread. If the operation
 * was cancelled, the error %G_IO_ERROR_CANCELLED will be returned.
 *
 * When the operation is finished, @callback will be called. You can then call
 * g_file_mount_mountable_finish() to get the result of the operation.
 **/
void
g_file_mount_mountable (GFile               *file,
			GMountMountFlags     flags,
			GMountOperation     *mount_operation,
			GCancellable        *cancellable,
			GAsyncReadyCallback  callback,
			gpointer             user_data)
{
  GFileIface *iface;

  g_return_if_fail (G_IS_FILE (file));

  iface = G_FILE_GET_IFACE (file);

  if (iface->mount_mountable == NULL) 
    {
      g_simple_async_report_error_in_idle (G_OBJECT (file),
					   callback,
					   user_data,
					   G_IO_ERROR,
					   G_IO_ERROR_NOT_SUPPORTED,
					   _("Operation not supported"));
      return;
    }
  
  (* iface->mount_mountable) (file,
			      flags,
			      mount_operation,
			      cancellable,
			      callback,
			      user_data);
}

/**
 * g_file_mount_mountable_finish:
 * @file: input #GFile.
 * @result: a #GAsyncResult.
 * @error: a #GError, or %NULL
 *
 * Finishes a mount operation. See g_file_mount_mountable() for details.
 * 
 * Finish an asynchronous mount operation that was started 
 * with g_file_mount_mountable().
 *
 * Returns: a #GFile or %NULL on error.
 *     Free the returned object with g_object_unref().
 **/
GFile *
g_file_mount_mountable_finish (GFile         *file,
			       GAsyncResult  *result,
			       GError       **error)
{
  GFileIface *iface;

  g_return_val_if_fail (G_IS_FILE (file), NULL);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), NULL);

  if (G_IS_SIMPLE_ASYNC_RESULT (result))
    {
      GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (result);
      if (g_simple_async_result_propagate_error (simple, error))
	return NULL;
    }
  
  iface = G_FILE_GET_IFACE (file);
  return (* iface->mount_mountable_finish) (file, result, error);
}

/**
 * g_file_unmount_mountable:
 * @file: input #GFile.
 * @flags: flags affecting the operation
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * @callback: a #GAsyncReadyCallback to call when the request is satisfied, or %NULL.
 * @user_data: the data to pass to callback function
 *
 * Unmounts a file of type G_FILE_TYPE_MOUNTABLE.
 *
 * If @cancellable is not %NULL, then the operation can be cancelled by
 * triggering the cancellable object from another thread. If the operation
 * was cancelled, the error %G_IO_ERROR_CANCELLED will be returned.
 *
 * When the operation is finished, @callback will be called. You can then call
 * g_file_unmount_mountable_finish() to get the result of the operation.
 *
 * Deprecated: 2.22: Use g_file_unmount_mountable_with_operation() instead.
 **/
void
g_file_unmount_mountable (GFile               *file,
			  GMountUnmountFlags   flags,
			  GCancellable        *cancellable,
			  GAsyncReadyCallback  callback,
			  gpointer             user_data)
{
  GFileIface *iface;
  
  g_return_if_fail (G_IS_FILE (file));

  iface = G_FILE_GET_IFACE (file);
  
  if (iface->unmount_mountable == NULL)
    {
      g_simple_async_report_error_in_idle (G_OBJECT (file),
					   callback,
					   user_data,
					   G_IO_ERROR,
					   G_IO_ERROR_NOT_SUPPORTED,
					   _("Operation not supported"));
      return;
    }
  
  (* iface->unmount_mountable) (file,
				flags,
				cancellable,
				callback,
				user_data);
}

/**
 * g_file_unmount_mountable_finish:
 * @file: input #GFile.
 * @result: a #GAsyncResult.
 * @error: a #GError, or %NULL
 *
 * Finishes an unmount operation, see g_file_unmount_mountable() for details.
 * 
 * Finish an asynchronous unmount operation that was started 
 * with g_file_unmount_mountable().
 *
 * Returns: %TRUE if the operation finished successfully. %FALSE
 * otherwise.
 *
 * Deprecated: 2.22: Use g_file_unmount_mountable_with_operation_finish() instead.
 **/
gboolean
g_file_unmount_mountable_finish (GFile         *file,
				 GAsyncResult  *result,
				 GError       **error)
{
  GFileIface *iface;
  
  g_return_val_if_fail (G_IS_FILE (file), FALSE);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

  if (G_IS_SIMPLE_ASYNC_RESULT (result))
    {
      GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (result);
      if (g_simple_async_result_propagate_error (simple, error))
	return FALSE;
    }
  
  iface = G_FILE_GET_IFACE (file);
  return (* iface->unmount_mountable_finish) (file, result, error);
}

/**
 * g_file_unmount_mountable_with_operation:
 * @file: input #GFile.
 * @flags: flags affecting the operation
 * @mount_operation: a #GMountOperation, or %NULL to avoid user interaction.
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * @callback: a #GAsyncReadyCallback to call when the request is satisfied, or %NULL.
 * @user_data: the data to pass to callback function
 *
 * Unmounts a file of type G_FILE_TYPE_MOUNTABLE.
 *
 * If @cancellable is not %NULL, then the operation can be cancelled by
 * triggering the cancellable object from another thread. If the operation
 * was cancelled, the error %G_IO_ERROR_CANCELLED will be returned.
 *
 * When the operation is finished, @callback will be called. You can then call
 * g_file_unmount_mountable_finish() to get the result of the operation.
 *
 * Since: 2.22
 **/
void
g_file_unmount_mountable_with_operation (GFile               *file,
                                         GMountUnmountFlags   flags,
                                         GMountOperation     *mount_operation,
                                         GCancellable        *cancellable,
                                         GAsyncReadyCallback  callback,
                                         gpointer             user_data)
{
  GFileIface *iface;

  g_return_if_fail (G_IS_FILE (file));

  iface = G_FILE_GET_IFACE (file);

  if (iface->unmount_mountable == NULL && iface->unmount_mountable_with_operation == NULL)
    {
      g_simple_async_report_error_in_idle (G_OBJECT (file),
					   callback,
					   user_data,
					   G_IO_ERROR,
					   G_IO_ERROR_NOT_SUPPORTED,
					   _("Operation not supported"));
      return;
    }

  if (iface->unmount_mountable_with_operation != NULL)
    (* iface->unmount_mountable_with_operation) (file,
                                                 flags,
                                                 mount_operation,
                                                 cancellable,
                                                 callback,
                                                 user_data);
  else
    (* iface->unmount_mountable) (file,
                                  flags,
                                  cancellable,
                                  callback,
                                  user_data);
}

/**
 * g_file_unmount_mountable_with_operation_finish:
 * @file: input #GFile.
 * @result: a #GAsyncResult.
 * @error: a #GError, or %NULL
 *
 * Finishes an unmount operation, see g_file_unmount_mountable_with_operation() for details.
 *
 * Finish an asynchronous unmount operation that was started
 * with g_file_unmount_mountable_with_operation().
 *
 * Returns: %TRUE if the operation finished successfully. %FALSE
 * otherwise.
 *
 * Since: 2.22
 **/
gboolean
g_file_unmount_mountable_with_operation_finish (GFile         *file,
                                                GAsyncResult  *result,
                                                GError       **error)
{
  GFileIface *iface;

  g_return_val_if_fail (G_IS_FILE (file), FALSE);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

  if (G_IS_SIMPLE_ASYNC_RESULT (result))
    {
      GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (result);
      if (g_simple_async_result_propagate_error (simple, error))
	return FALSE;
    }

  iface = G_FILE_GET_IFACE (file);
  if (iface->unmount_mountable_with_operation_finish != NULL)
    return (* iface->unmount_mountable_with_operation_finish) (file, result, error);
  else
    return (* iface->unmount_mountable_finish) (file, result, error);
}

/**
 * g_file_eject_mountable:
 * @file: input #GFile.
 * @flags: flags affecting the operation
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * @callback: a #GAsyncReadyCallback to call when the request is satisfied, or %NULL.
 * @user_data: the data to pass to callback function
 * 
 * Starts an asynchronous eject on a mountable.  
 * When this operation has completed, @callback will be called with
 * @user_user data, and the operation can be finalized with 
 * g_file_eject_mountable_finish().
 * 
 * If @cancellable is not %NULL, then the operation can be cancelled by
 * triggering the cancellable object from another thread. If the operation
 * was cancelled, the error %G_IO_ERROR_CANCELLED will be returned. 
 *
 * Deprecated: 2.22: Use g_file_eject_mountable_with_operation() instead.
 **/
void
g_file_eject_mountable (GFile               *file,
			GMountUnmountFlags   flags,
			GCancellable        *cancellable,
			GAsyncReadyCallback  callback,
			gpointer             user_data)
{
  GFileIface *iface;

  g_return_if_fail (G_IS_FILE (file));

  iface = G_FILE_GET_IFACE (file);
  
  if (iface->eject_mountable == NULL) 
    {
      g_simple_async_report_error_in_idle (G_OBJECT (file),
					   callback,
					   user_data,
					   G_IO_ERROR,
					   G_IO_ERROR_NOT_SUPPORTED,
					   _("Operation not supported"));
      return;
    }
  
  (* iface->eject_mountable) (file,
			      flags,
			      cancellable,
			      callback,
			      user_data);
}

/**
 * g_file_eject_mountable_finish:
 * @file: input #GFile.
 * @result: a #GAsyncResult.
 * @error: a #GError, or %NULL
 * 
 * Finishes an asynchronous eject operation started by 
 * g_file_eject_mountable().
 * 
 * Returns: %TRUE if the @file was ejected successfully. %FALSE 
 * otherwise.
 *
 * Deprecated: 2.22: Use g_file_eject_mountable_with_operation_finish() instead.
 **/
gboolean
g_file_eject_mountable_finish (GFile         *file,
			       GAsyncResult  *result,
			       GError       **error)
{
  GFileIface *iface;
  
  g_return_val_if_fail (G_IS_FILE (file), FALSE);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

  if (G_IS_SIMPLE_ASYNC_RESULT (result))
    {
      GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (result);
      if (g_simple_async_result_propagate_error (simple, error))
	return FALSE;
    }
  
  iface = G_FILE_GET_IFACE (file);
  return (* iface->eject_mountable_finish) (file, result, error);
}

/**
 * g_file_eject_mountable_with_operation:
 * @file: input #GFile.
 * @flags: flags affecting the operation
 * @mount_operation: a #GMountOperation, or %NULL to avoid user interaction.
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * @callback: a #GAsyncReadyCallback to call when the request is satisfied, or %NULL.
 * @user_data: the data to pass to callback function
 *
 * Starts an asynchronous eject on a mountable.
 * When this operation has completed, @callback will be called with
 * @user_user data, and the operation can be finalized with
 * g_file_eject_mountable_with_operation_finish().
 *
 * If @cancellable is not %NULL, then the operation can be cancelled by
 * triggering the cancellable object from another thread. If the operation
 * was cancelled, the error %G_IO_ERROR_CANCELLED will be returned.
 *
 * Since: 2.22
 **/
void
g_file_eject_mountable_with_operation (GFile               *file,
                                       GMountUnmountFlags   flags,
                                       GMountOperation     *mount_operation,
                                       GCancellable        *cancellable,
                                       GAsyncReadyCallback  callback,
                                       gpointer             user_data)
{
  GFileIface *iface;

  g_return_if_fail (G_IS_FILE (file));

  iface = G_FILE_GET_IFACE (file);

  if (iface->eject_mountable == NULL && iface->eject_mountable_with_operation == NULL)
    {
      g_simple_async_report_error_in_idle (G_OBJECT (file),
					   callback,
					   user_data,
					   G_IO_ERROR,
					   G_IO_ERROR_NOT_SUPPORTED,
					   _("Operation not supported"));
      return;
    }

  if (iface->eject_mountable_with_operation != NULL)
    (* iface->eject_mountable_with_operation) (file,
                                               flags,
                                               mount_operation,
                                               cancellable,
                                               callback,
                                               user_data);
  else
    (* iface->eject_mountable) (file,
                                flags,
                                cancellable,
                                callback,
                                user_data);
}

/**
 * g_file_eject_mountable_with_operation_finish:
 * @file: input #GFile.
 * @result: a #GAsyncResult.
 * @error: a #GError, or %NULL
 *
 * Finishes an asynchronous eject operation started by
 * g_file_eject_mountable_with_operation().
 *
 * Returns: %TRUE if the @file was ejected successfully. %FALSE
 * otherwise.
 *
 * Since: 2.22
 **/
gboolean
g_file_eject_mountable_with_operation_finish (GFile         *file,
                                              GAsyncResult  *result,
                                              GError       **error)
{
  GFileIface *iface;

  g_return_val_if_fail (G_IS_FILE (file), FALSE);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

  if (G_IS_SIMPLE_ASYNC_RESULT (result))
    {
      GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (result);
      if (g_simple_async_result_propagate_error (simple, error))
	return FALSE;
    }

  iface = G_FILE_GET_IFACE (file);
  if (iface->eject_mountable_with_operation_finish != NULL)
    return (* iface->eject_mountable_with_operation_finish) (file, result, error);
  else
    return (* iface->eject_mountable_finish) (file, result, error);
}

/**
 * g_file_monitor_directory:
 * @file: input #GFile.
 * @flags: a set of #GFileMonitorFlags.
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * @error: a #GError, or %NULL.
 * 
 * Obtains a directory monitor for the given file.
 * This may fail if directory monitoring is not supported.
 *
 * If @cancellable is not %NULL, then the operation can be cancelled by
 * triggering the cancellable object from another thread. If the operation
 * was cancelled, the error %G_IO_ERROR_CANCELLED will be returned. 
 * 
 * Returns: a #GFileMonitor for the given @file, or %NULL on error.
 *     Free the returned object with g_object_unref().
 **/
GFileMonitor*
g_file_monitor_directory (GFile             *file,
			  GFileMonitorFlags  flags,
			  GCancellable      *cancellable,
			  GError           **error)
{
  GFileIface *iface;

  g_return_val_if_fail (G_IS_FILE (file), NULL);

  if (g_cancellable_set_error_if_cancelled (cancellable, error))
    return NULL;

  iface = G_FILE_GET_IFACE (file);

  if (iface->monitor_dir == NULL)
    {
      g_set_error_literal (error, G_IO_ERROR,
                           G_IO_ERROR_NOT_SUPPORTED,
                           _("Operation not supported"));
      return NULL;
    }

  return (* iface->monitor_dir) (file, flags, cancellable, error);
}

/**
 * g_file_monitor_file:
 * @file: input #GFile.
 * @flags: a set of #GFileMonitorFlags.
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * @error: a #GError, or %NULL.
 * 
 * Obtains a file monitor for the given file. If no file notification
 * mechanism exists, then regular polling of the file is used.
 *
 * If @cancellable is not %NULL, then the operation can be cancelled by
 * triggering the cancellable object from another thread. If the operation
 * was cancelled, the error %G_IO_ERROR_CANCELLED will be returned. 
 * 
 * Returns: a #GFileMonitor for the given @file, or %NULL on error.
 *     Free the returned object with g_object_unref().
 **/
GFileMonitor*
g_file_monitor_file (GFile             *file,
		     GFileMonitorFlags  flags,
		     GCancellable      *cancellable,
		     GError           **error)
{
  GFileIface *iface;
  GFileMonitor *monitor;
  
  g_return_val_if_fail (G_IS_FILE (file), NULL);

  if (g_cancellable_set_error_if_cancelled (cancellable, error))
    return NULL;

  iface = G_FILE_GET_IFACE (file);

  monitor = NULL;
  
  if (iface->monitor_file)
    monitor = (* iface->monitor_file) (file, flags, cancellable, NULL);

/* Fallback to polling */
  if (monitor == NULL)
    monitor = _g_poll_file_monitor_new (file);

  return monitor;
}

/**
 * g_file_monitor:
 * @file: input #GFile
 * @flags: a set of #GFileMonitorFlags
 * @cancellable: optional #GCancellable object, %NULL to ignore
 * @error: a #GError, or %NULL
 * 
 * Obtains a file or directory monitor for the given file, depending
 * on the type of the file.
 *
 * If @cancellable is not %NULL, then the operation can be cancelled by
 * triggering the cancellable object from another thread. If the operation
 * was cancelled, the error %G_IO_ERROR_CANCELLED will be returned. 
 * 
 * Returns: a #GFileMonitor for the given @file, or %NULL on error.
 *     Free the returned object with g_object_unref().
 *
 * Since: 2.18
 */
GFileMonitor*
g_file_monitor (GFile             *file,
	        GFileMonitorFlags  flags,
		GCancellable      *cancellable,
		GError           **error)
{
  if (g_file_query_file_type (file, 0, cancellable) == G_FILE_TYPE_DIRECTORY)
    return g_file_monitor_directory (file, flags, cancellable, error);
  else
    return g_file_monitor_file (file, flags, cancellable, error);
}

/********************************************
 *   Default implementation of async ops    *
 ********************************************/

typedef struct {
  char *attributes;
  GFileQueryInfoFlags flags;
  GFileInfo *info;
} QueryInfoAsyncData;

static void
query_info_data_free (QueryInfoAsyncData *data)
{
  if (data->info)
    g_object_unref (data->info);
  g_free (data->attributes);
  g_free (data);
}

static void
query_info_async_thread (GSimpleAsyncResult *res,
			 GObject            *object,
			 GCancellable       *cancellable)
{
  GError *error = NULL;
  QueryInfoAsyncData *data;
  GFileInfo *info;
  
  data = g_simple_async_result_get_op_res_gpointer (res);
  
  info = g_file_query_info (G_FILE (object), data->attributes, data->flags, cancellable, &error);

  if (info == NULL)
    {
      g_simple_async_result_set_from_error (res, error);
      g_error_free (error);
    }
  else
    data->info = info;
}

static void
g_file_real_query_info_async (GFile               *file,
			      const char          *attributes,
			      GFileQueryInfoFlags  flags,
			      int                  io_priority,
			      GCancellable        *cancellable,
			      GAsyncReadyCallback  callback,
			      gpointer             user_data)
{
  GSimpleAsyncResult *res;
  QueryInfoAsyncData *data;

  data = g_new0 (QueryInfoAsyncData, 1);
  data->attributes = g_strdup (attributes);
  data->flags = flags;
  
  res = g_simple_async_result_new (G_OBJECT (file), callback, user_data, g_file_real_query_info_async);
  g_simple_async_result_set_op_res_gpointer (res, data, (GDestroyNotify)query_info_data_free);
  
  g_simple_async_result_run_in_thread (res, query_info_async_thread, io_priority, cancellable);
  g_object_unref (res);
}

static GFileInfo *
g_file_real_query_info_finish (GFile         *file,
			       GAsyncResult  *res,
			       GError       **error)
{
  GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (res);
  QueryInfoAsyncData *data;

  g_warn_if_fail (g_simple_async_result_get_source_tag (simple) == g_file_real_query_info_async);

  data = g_simple_async_result_get_op_res_gpointer (simple);
  if (data->info)
    return g_object_ref (data->info);
  
  return NULL;
}

typedef struct {
  char *attributes;
  GFileInfo *info;
} QueryFilesystemInfoAsyncData;

static void
query_filesystem_info_data_free (QueryFilesystemInfoAsyncData *data)
{
  if (data->info)
    g_object_unref (data->info);
  g_free (data->attributes);
  g_free (data);
}

static void
query_filesystem_info_async_thread (GSimpleAsyncResult *res,
                                    GObject            *object,
                                    GCancellable       *cancellable)
{
  GError *error = NULL;
  QueryFilesystemInfoAsyncData *data;
  GFileInfo *info;
  
  data = g_simple_async_result_get_op_res_gpointer (res);
  
  info = g_file_query_filesystem_info (G_FILE (object), data->attributes, cancellable, &error);

  if (info == NULL)
    {
      g_simple_async_result_set_from_error (res, error);
      g_error_free (error);
    }
  else
    data->info = info;
}

static void
g_file_real_query_filesystem_info_async (GFile               *file,
                                         const char          *attributes,
                                         int                  io_priority,
                                         GCancellable        *cancellable,
                                         GAsyncReadyCallback  callback,
                                         gpointer             user_data)
{
  GSimpleAsyncResult *res;
  QueryFilesystemInfoAsyncData *data;

  data = g_new0 (QueryFilesystemInfoAsyncData, 1);
  data->attributes = g_strdup (attributes);
  
  res = g_simple_async_result_new (G_OBJECT (file), callback, user_data, g_file_real_query_filesystem_info_async);
  g_simple_async_result_set_op_res_gpointer (res, data, (GDestroyNotify)query_filesystem_info_data_free);
  
  g_simple_async_result_run_in_thread (res, query_filesystem_info_async_thread, io_priority, cancellable);
  g_object_unref (res);
}

static GFileInfo *
g_file_real_query_filesystem_info_finish (GFile         *file,
                                          GAsyncResult  *res,
                                          GError       **error)
{
  GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (res);
  QueryFilesystemInfoAsyncData *data;

  g_warn_if_fail (g_simple_async_result_get_source_tag (simple) == g_file_real_query_filesystem_info_async);

  data = g_simple_async_result_get_op_res_gpointer (simple);
  if (data->info)
    return g_object_ref (data->info);
  
  return NULL;
}

typedef struct {
  char *attributes;
  GFileQueryInfoFlags flags;
  GFileEnumerator *enumerator;
} EnumerateChildrenAsyncData;

static void
enumerate_children_data_free (EnumerateChildrenAsyncData *data)
{
  if (data->enumerator)
    g_object_unref (data->enumerator);
  g_free (data->attributes);
  g_free (data);
}

static void
enumerate_children_async_thread (GSimpleAsyncResult *res,
				 GObject            *object,
				 GCancellable       *cancellable)
{
  GError *error = NULL;
  EnumerateChildrenAsyncData *data;
  GFileEnumerator *enumerator;
  
  data = g_simple_async_result_get_op_res_gpointer (res);
  
  enumerator = g_file_enumerate_children (G_FILE (object), data->attributes, data->flags, cancellable, &error);

  if (enumerator == NULL)
    {
      g_simple_async_result_set_from_error (res, error);
      g_error_free (error);
    }
  else
    data->enumerator = enumerator;
}

static void
g_file_real_enumerate_children_async (GFile               *file,
				      const char          *attributes,
				      GFileQueryInfoFlags  flags,
				      int                  io_priority,
				      GCancellable        *cancellable,
				      GAsyncReadyCallback  callback,
				      gpointer             user_data)
{
  GSimpleAsyncResult *res;
  EnumerateChildrenAsyncData *data;

  data = g_new0 (EnumerateChildrenAsyncData, 1);
  data->attributes = g_strdup (attributes);
  data->flags = flags;
  
  res = g_simple_async_result_new (G_OBJECT (file), callback, user_data, g_file_real_enumerate_children_async);
  g_simple_async_result_set_op_res_gpointer (res, data, (GDestroyNotify)enumerate_children_data_free);
  
  g_simple_async_result_run_in_thread (res, enumerate_children_async_thread, io_priority, cancellable);
  g_object_unref (res);
}

static GFileEnumerator *
g_file_real_enumerate_children_finish (GFile         *file,
				       GAsyncResult  *res,
				       GError       **error)
{
  GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (res);
  EnumerateChildrenAsyncData *data;

  g_warn_if_fail (g_simple_async_result_get_source_tag (simple) == g_file_real_enumerate_children_async);

  data = g_simple_async_result_get_op_res_gpointer (simple);
  if (data->enumerator)
    return g_object_ref (data->enumerator);
  
  return NULL;
}

static void
open_read_async_thread (GSimpleAsyncResult *res,
			GObject            *object,
			GCancellable       *cancellable)
{
  GFileIface *iface;
  GFileInputStream *stream;
  GError *error = NULL;

  iface = G_FILE_GET_IFACE (object);

  if (iface->read_fn == NULL)
    {
      g_set_error_literal (&error, G_IO_ERROR,
                           G_IO_ERROR_NOT_SUPPORTED,
                           _("Operation not supported"));

      g_simple_async_result_set_from_error (res, error);
      g_error_free (error);

      return;
    }
  
  stream = iface->read_fn (G_FILE (object), cancellable, &error);

  if (stream == NULL)
    {
      g_simple_async_result_set_from_error (res, error);
      g_error_free (error);
    }
  else
    g_simple_async_result_set_op_res_gpointer (res, stream, g_object_unref);
}

static void
g_file_real_read_async (GFile               *file,
			int                  io_priority,
			GCancellable        *cancellable,
			GAsyncReadyCallback  callback,
			gpointer             user_data)
{
  GSimpleAsyncResult *res;
  
  res = g_simple_async_result_new (G_OBJECT (file), callback, user_data, g_file_real_read_async);
  
  g_simple_async_result_run_in_thread (res, open_read_async_thread, io_priority, cancellable);
  g_object_unref (res);
}

static GFileInputStream *
g_file_real_read_finish (GFile         *file,
			 GAsyncResult  *res,
			 GError       **error)
{
  GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (res);
  gpointer op;

  g_warn_if_fail (g_simple_async_result_get_source_tag (simple) == g_file_real_read_async);

  op = g_simple_async_result_get_op_res_gpointer (simple);
  if (op)
    return g_object_ref (op);
  
  return NULL;
}

static void
append_to_async_thread (GSimpleAsyncResult *res,
			GObject            *object,
			GCancellable       *cancellable)
{
  GFileIface *iface;
  GFileCreateFlags *data;
  GFileOutputStream *stream;
  GError *error = NULL;

  iface = G_FILE_GET_IFACE (object);

  data = g_simple_async_result_get_op_res_gpointer (res);

  stream = iface->append_to (G_FILE (object), *data, cancellable, &error);

  if (stream == NULL)
    {
      g_simple_async_result_set_from_error (res, error);
      g_error_free (error);
    }
  else
    g_simple_async_result_set_op_res_gpointer (res, stream, g_object_unref);
}

static void
g_file_real_append_to_async (GFile               *file,
			     GFileCreateFlags     flags,
			     int                  io_priority,
			     GCancellable        *cancellable,
			     GAsyncReadyCallback  callback,
			     gpointer             user_data)
{
  GFileCreateFlags *data;
  GSimpleAsyncResult *res;

  data = g_new0 (GFileCreateFlags, 1);
  *data = flags;

  res = g_simple_async_result_new (G_OBJECT (file), callback, user_data, g_file_real_append_to_async);
  g_simple_async_result_set_op_res_gpointer (res, data, (GDestroyNotify)g_free);

  g_simple_async_result_run_in_thread (res, append_to_async_thread, io_priority, cancellable);
  g_object_unref (res);
}

static GFileOutputStream *
g_file_real_append_to_finish (GFile         *file,
			      GAsyncResult  *res,
			      GError       **error)
{
  GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (res);
  gpointer op;

  g_warn_if_fail (g_simple_async_result_get_source_tag (simple) == g_file_real_append_to_async);

  op = g_simple_async_result_get_op_res_gpointer (simple);
  if (op)
    return g_object_ref (op);
  
  return NULL;
}

static void
create_async_thread (GSimpleAsyncResult *res,
		     GObject            *object,
		     GCancellable       *cancellable)
{
  GFileIface *iface;
  GFileCreateFlags *data;
  GFileOutputStream *stream;
  GError *error = NULL;

  iface = G_FILE_GET_IFACE (object);

  data = g_simple_async_result_get_op_res_gpointer (res);

  stream = iface->create (G_FILE (object), *data, cancellable, &error);

  if (stream == NULL)
    {
      g_simple_async_result_set_from_error (res, error);
      g_error_free (error);
    }
  else
    g_simple_async_result_set_op_res_gpointer (res, stream, g_object_unref);
}

static void
g_file_real_create_async (GFile               *file,
			  GFileCreateFlags     flags,
			  int                  io_priority,
			  GCancellable        *cancellable,
			  GAsyncReadyCallback  callback,
			  gpointer             user_data)
{
  GFileCreateFlags *data;
  GSimpleAsyncResult *res;

  data = g_new0 (GFileCreateFlags, 1);
  *data = flags;

  res = g_simple_async_result_new (G_OBJECT (file), callback, user_data, g_file_real_create_async);
  g_simple_async_result_set_op_res_gpointer (res, data, (GDestroyNotify)g_free);

  g_simple_async_result_run_in_thread (res, create_async_thread, io_priority, cancellable);
  g_object_unref (res);
}

static GFileOutputStream *
g_file_real_create_finish (GFile         *file,
			   GAsyncResult  *res,
			   GError       **error)
{
  GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (res);
  gpointer op;

  g_warn_if_fail (g_simple_async_result_get_source_tag (simple) == g_file_real_create_async);

  op = g_simple_async_result_get_op_res_gpointer (simple);
  if (op)
    return g_object_ref (op);
  
  return NULL;
}

typedef struct {
  GFileOutputStream *stream;
  char *etag;
  gboolean make_backup;
  GFileCreateFlags flags;
} ReplaceAsyncData;

static void
replace_async_data_free (ReplaceAsyncData *data)
{
  if (data->stream)
    g_object_unref (data->stream);
  g_free (data->etag);
  g_free (data);
}

static void
replace_async_thread (GSimpleAsyncResult *res,
		      GObject            *object,
		      GCancellable       *cancellable)
{
  GFileIface *iface;
  GFileOutputStream *stream;
  GError *error = NULL;
  ReplaceAsyncData *data;

  iface = G_FILE_GET_IFACE (object);
  
  data = g_simple_async_result_get_op_res_gpointer (res);

  stream = iface->replace (G_FILE (object),
			   data->etag,
			   data->make_backup,
			   data->flags,
			   cancellable,
			   &error);

  if (stream == NULL)
    {
      g_simple_async_result_set_from_error (res, error);
      g_error_free (error);
    }
  else
    data->stream = stream;
}

static void
g_file_real_replace_async (GFile               *file,
			   const char          *etag,
			   gboolean             make_backup,
			   GFileCreateFlags     flags,
			   int                  io_priority,
			   GCancellable        *cancellable,
			   GAsyncReadyCallback  callback,
			   gpointer             user_data)
{
  GSimpleAsyncResult *res;
  ReplaceAsyncData *data;

  data = g_new0 (ReplaceAsyncData, 1);
  data->etag = g_strdup (etag);
  data->make_backup = make_backup;
  data->flags = flags;

  res = g_simple_async_result_new (G_OBJECT (file), callback, user_data, g_file_real_replace_async);
  g_simple_async_result_set_op_res_gpointer (res, data, (GDestroyNotify)replace_async_data_free);

  g_simple_async_result_run_in_thread (res, replace_async_thread, io_priority, cancellable);
  g_object_unref (res);
}

static GFileOutputStream *
g_file_real_replace_finish (GFile         *file,
			    GAsyncResult  *res,
			    GError       **error)
{
  GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (res);
  ReplaceAsyncData *data;

  g_warn_if_fail (g_simple_async_result_get_source_tag (simple) == g_file_real_replace_async);

  data = g_simple_async_result_get_op_res_gpointer (simple);
  if (data->stream)
    return g_object_ref (data->stream);
  
  return NULL;
}

static void
open_readwrite_async_thread (GSimpleAsyncResult *res,
			     GObject            *object,
			     GCancellable       *cancellable)
{
  GFileIface *iface;
  GFileIOStream *stream;
  GError *error = NULL;

  iface = G_FILE_GET_IFACE (object);

  if (iface->open_readwrite == NULL)
    {
      g_set_error_literal (&error, G_IO_ERROR,
                           G_IO_ERROR_NOT_SUPPORTED,
                           _("Operation not supported"));

      g_simple_async_result_set_from_error (res, error);
      g_error_free (error);

      return;
    }

  stream = iface->open_readwrite (G_FILE (object), cancellable, &error);

  if (stream == NULL)
    {
      g_simple_async_result_set_from_error (res, error);
      g_error_free (error);
    }
  else
    g_simple_async_result_set_op_res_gpointer (res, stream, g_object_unref);
}

static void
g_file_real_open_readwrite_async (GFile               *file,
				  int                  io_priority,
				  GCancellable        *cancellable,
				  GAsyncReadyCallback  callback,
				  gpointer             user_data)
{
  GSimpleAsyncResult *res;

  res = g_simple_async_result_new (G_OBJECT (file), callback, user_data, g_file_real_open_readwrite_async);

  g_simple_async_result_run_in_thread (res, open_readwrite_async_thread, io_priority, cancellable);
  g_object_unref (res);
}

static GFileIOStream *
g_file_real_open_readwrite_finish (GFile         *file,
				   GAsyncResult  *res,
				   GError       **error)
{
  GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (res);
  gpointer op;

  g_warn_if_fail (g_simple_async_result_get_source_tag (simple) == g_file_real_open_readwrite_async);

  op = g_simple_async_result_get_op_res_gpointer (simple);
  if (op)
    return g_object_ref (op);

  return NULL;
}

static void
create_readwrite_async_thread (GSimpleAsyncResult *res,
			       GObject            *object,
			       GCancellable       *cancellable)
{
  GFileIface *iface;
  GFileCreateFlags *data;
  GFileIOStream *stream;
  GError *error = NULL;

  iface = G_FILE_GET_IFACE (object);

  data = g_simple_async_result_get_op_res_gpointer (res);

  if (iface->create_readwrite == NULL)
    {
      g_set_error_literal (&error, G_IO_ERROR,
                           G_IO_ERROR_NOT_SUPPORTED,
                           _("Operation not supported"));

      g_simple_async_result_set_from_error (res, error);
      g_error_free (error);

      return;
    }

  stream = iface->create_readwrite (G_FILE (object), *data, cancellable, &error);

  if (stream == NULL)
    {
      g_simple_async_result_set_from_error (res, error);
      g_error_free (error);
    }
  else
    g_simple_async_result_set_op_res_gpointer (res, stream, g_object_unref);
}

static void
g_file_real_create_readwrite_async (GFile               *file,
				    GFileCreateFlags     flags,
				    int                  io_priority,
				    GCancellable        *cancellable,
				    GAsyncReadyCallback  callback,
				    gpointer             user_data)
{
  GFileCreateFlags *data;
  GSimpleAsyncResult *res;

  data = g_new0 (GFileCreateFlags, 1);
  *data = flags;

  res = g_simple_async_result_new (G_OBJECT (file), callback, user_data, g_file_real_create_readwrite_async);
  g_simple_async_result_set_op_res_gpointer (res, data, (GDestroyNotify)g_free);

  g_simple_async_result_run_in_thread (res, create_readwrite_async_thread, io_priority, cancellable);
  g_object_unref (res);
}

static GFileIOStream *
g_file_real_create_readwrite_finish (GFile         *file,
                                     GAsyncResult  *res,
                                     GError       **error)
{
  GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (res);
  gpointer op;

  g_warn_if_fail (g_simple_async_result_get_source_tag (simple) == g_file_real_create_readwrite_async);

  op = g_simple_async_result_get_op_res_gpointer (simple);
  if (op)
    return g_object_ref (op);

  return NULL;
}

typedef struct {
  GFileIOStream *stream;
  char *etag;
  gboolean make_backup;
  GFileCreateFlags flags;
} ReplaceRWAsyncData;

static void
replace_rw_async_data_free (ReplaceRWAsyncData *data)
{
  if (data->stream)
    g_object_unref (data->stream);
  g_free (data->etag);
  g_free (data);
}

static void
replace_readwrite_async_thread (GSimpleAsyncResult *res,
				GObject            *object,
				GCancellable       *cancellable)
{
  GFileIface *iface;
  GFileIOStream *stream;
  GError *error = NULL;
  ReplaceRWAsyncData *data;

  iface = G_FILE_GET_IFACE (object);

  data = g_simple_async_result_get_op_res_gpointer (res);

  stream = iface->replace_readwrite (G_FILE (object),
				     data->etag,
				     data->make_backup,
				     data->flags,
				     cancellable,
				     &error);

  if (stream == NULL)
    {
      g_simple_async_result_set_from_error (res, error);
      g_error_free (error);
    }
  else
    data->stream = stream;
}

static void
g_file_real_replace_readwrite_async (GFile               *file,
				     const char          *etag,
				     gboolean             make_backup,
				     GFileCreateFlags     flags,
				     int                  io_priority,
				     GCancellable        *cancellable,
				     GAsyncReadyCallback  callback,
				     gpointer             user_data)
{
  GSimpleAsyncResult *res;
  ReplaceRWAsyncData *data;

  data = g_new0 (ReplaceRWAsyncData, 1);
  data->etag = g_strdup (etag);
  data->make_backup = make_backup;
  data->flags = flags;

  res = g_simple_async_result_new (G_OBJECT (file), callback, user_data, g_file_real_replace_readwrite_async);
  g_simple_async_result_set_op_res_gpointer (res, data, (GDestroyNotify)replace_rw_async_data_free);

  g_simple_async_result_run_in_thread (res, replace_readwrite_async_thread, io_priority, cancellable);
  g_object_unref (res);
}

static GFileIOStream *
g_file_real_replace_readwrite_finish (GFile         *file,
                                      GAsyncResult  *res,
                                      GError       **error)
{
  GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (res);
  ReplaceRWAsyncData *data;

  g_warn_if_fail (g_simple_async_result_get_source_tag (simple) == g_file_real_replace_readwrite_async);

  data = g_simple_async_result_get_op_res_gpointer (simple);
  if (data->stream)
    return g_object_ref (data->stream);

  return NULL;
}

typedef struct {
  char *name;
  GFile *file;
} SetDisplayNameAsyncData;

static void
set_display_name_data_free (SetDisplayNameAsyncData *data)
{
  g_free (data->name);
  if (data->file)
    g_object_unref (data->file);
  g_free (data);
}

static void
set_display_name_async_thread (GSimpleAsyncResult *res,
			       GObject            *object,
			       GCancellable       *cancellable)
{
  GError *error = NULL;
  SetDisplayNameAsyncData *data;
  GFile *file;
  
  data = g_simple_async_result_get_op_res_gpointer (res);
  
  file = g_file_set_display_name (G_FILE (object), data->name, cancellable, &error);

  if (file == NULL)
    {
      g_simple_async_result_set_from_error (res, error);
      g_error_free (error);
    }
  else
    data->file = file;
}

static void
g_file_real_set_display_name_async (GFile               *file,	
				    const char          *display_name,
				    int                  io_priority,
				    GCancellable        *cancellable,
				    GAsyncReadyCallback  callback,
				    gpointer             user_data)
{
  GSimpleAsyncResult *res;
  SetDisplayNameAsyncData *data;

  data = g_new0 (SetDisplayNameAsyncData, 1);
  data->name = g_strdup (display_name);
  
  res = g_simple_async_result_new (G_OBJECT (file), callback, user_data, g_file_real_set_display_name_async);
  g_simple_async_result_set_op_res_gpointer (res, data, (GDestroyNotify)set_display_name_data_free);
  
  g_simple_async_result_run_in_thread (res, set_display_name_async_thread, io_priority, cancellable);
  g_object_unref (res);
}

static GFile *
g_file_real_set_display_name_finish (GFile         *file,
				     GAsyncResult  *res,
				     GError       **error)
{
  GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (res);
  SetDisplayNameAsyncData *data;

  g_warn_if_fail (g_simple_async_result_get_source_tag (simple) == g_file_real_set_display_name_async);

  data = g_simple_async_result_get_op_res_gpointer (simple);
  if (data->file)
    return g_object_ref (data->file);
  
  return NULL;
}

typedef struct {
  GFileQueryInfoFlags flags;
  GFileInfo *info;
  gboolean res;
  GError *error;
} SetInfoAsyncData;

static void
set_info_data_free (SetInfoAsyncData *data)
{
  if (data->info)
    g_object_unref (data->info);
  if (data->error)
    g_error_free (data->error);
  g_free (data);
}

static void
set_info_async_thread (GSimpleAsyncResult *res,
		       GObject            *object,
		       GCancellable       *cancellable)
{
  SetInfoAsyncData *data;
  
  data = g_simple_async_result_get_op_res_gpointer (res);
  
  data->error = NULL;
  data->res = g_file_set_attributes_from_info (G_FILE (object),
					       data->info,
					       data->flags,
					       cancellable,
					       &data->error);
}

static void
g_file_real_set_attributes_async (GFile               *file,
				  GFileInfo           *info,
				  GFileQueryInfoFlags  flags,
				  int                  io_priority,
				  GCancellable        *cancellable,
				  GAsyncReadyCallback  callback,
				  gpointer             user_data)
{
  GSimpleAsyncResult *res;
  SetInfoAsyncData *data;

  data = g_new0 (SetInfoAsyncData, 1);
  data->info = g_file_info_dup (info);
  data->flags = flags;
  
  res = g_simple_async_result_new (G_OBJECT (file), callback, user_data, g_file_real_set_attributes_async);
  g_simple_async_result_set_op_res_gpointer (res, data, (GDestroyNotify)set_info_data_free);
  
  g_simple_async_result_run_in_thread (res, set_info_async_thread, io_priority, cancellable);
  g_object_unref (res);
}

static gboolean
g_file_real_set_attributes_finish (GFile         *file,
				   GAsyncResult  *res,
				   GFileInfo    **info,
				   GError       **error)
{
  GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (res);
  SetInfoAsyncData *data;
  
  g_warn_if_fail (g_simple_async_result_get_source_tag (simple) == g_file_real_set_attributes_async);

  data = g_simple_async_result_get_op_res_gpointer (simple);

  if (info) 
    *info = g_object_ref (data->info);

  if (error != NULL && data->error) 
    *error = g_error_copy (data->error);
  
  return data->res;
}

static void
find_enclosing_mount_async_thread (GSimpleAsyncResult *res,
				    GObject            *object,
				    GCancellable       *cancellable)
{
  GError *error = NULL;
  GMount *mount;
  
  mount = g_file_find_enclosing_mount (G_FILE (object), cancellable, &error);

  if (mount == NULL)
    {
      g_simple_async_result_set_from_error (res, error);
      g_error_free (error);
    }
  else
    g_simple_async_result_set_op_res_gpointer (res, mount, (GDestroyNotify)g_object_unref);
}

static void
g_file_real_find_enclosing_mount_async (GFile               *file,
					int                  io_priority,
					GCancellable        *cancellable,
					GAsyncReadyCallback  callback,
					gpointer             user_data)
{
  GSimpleAsyncResult *res;
  
  res = g_simple_async_result_new (G_OBJECT (file), callback, user_data, g_file_real_find_enclosing_mount_async);
  
  g_simple_async_result_run_in_thread (res, find_enclosing_mount_async_thread, io_priority, cancellable);
  g_object_unref (res);
}

static GMount *
g_file_real_find_enclosing_mount_finish (GFile         *file,
					  GAsyncResult  *res,
					  GError       **error)
{
  GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (res);
  GMount *mount;

  g_warn_if_fail (g_simple_async_result_get_source_tag (simple) == g_file_real_find_enclosing_mount_async);

  mount = g_simple_async_result_get_op_res_gpointer (simple);
  return g_object_ref (mount);
}


typedef struct {
  GFile *source;
  GFile *destination;
  GFileCopyFlags flags;
  GFileProgressCallback progress_cb;
  gpointer progress_cb_data;
  GIOSchedulerJob *job;
} CopyAsyncData;

static void
copy_async_data_free (CopyAsyncData *data)
{
  g_object_unref (data->source);
  g_object_unref (data->destination);
  g_free (data);
}

typedef struct {
  CopyAsyncData *data;
  goffset current_num_bytes;
  goffset total_num_bytes;
} ProgressData;

static gboolean
copy_async_progress_in_main (gpointer user_data)
{
  ProgressData *progress = user_data;
  CopyAsyncData *data = progress->data;

  data->progress_cb (progress->current_num_bytes,
		     progress->total_num_bytes,
		     data->progress_cb_data);

  return FALSE;
}

static gboolean
mainloop_barrier (gpointer user_data)
{
  /* Does nothing, but ensures all queued idles before
     this are run */
  return FALSE;
}


static void
copy_async_progress_callback (goffset  current_num_bytes,
			      goffset  total_num_bytes,
			      gpointer user_data)
{
  CopyAsyncData *data = user_data;
  ProgressData *progress;

  progress = g_new (ProgressData, 1);
  progress->data = data;
  progress->current_num_bytes = current_num_bytes;
  progress->total_num_bytes = total_num_bytes;
  
  g_io_scheduler_job_send_to_mainloop_async (data->job,
					     copy_async_progress_in_main,
					     progress,
					     g_free);
}

static gboolean
copy_async_thread (GIOSchedulerJob *job,
		   GCancellable    *cancellable,
 		   gpointer         user_data)
{
  GSimpleAsyncResult *res;
  CopyAsyncData *data;
  gboolean result;
  GError *error;

  res = user_data;
  data = g_simple_async_result_get_op_res_gpointer (res);

  error = NULL;
  data->job = job;
  result = g_file_copy (data->source,
			data->destination,
			data->flags,
			cancellable,
			(data->progress_cb != NULL) ? copy_async_progress_callback : NULL,
			data,
			&error);

  /* Ensure all progress callbacks are done running in main thread */
  if (data->progress_cb != NULL)
    g_io_scheduler_job_send_to_mainloop (job,
					 mainloop_barrier,
					 NULL, NULL);
  
  if (!result)
    {
      g_simple_async_result_set_from_error (res, error);
      g_error_free (error);
    }

  g_simple_async_result_complete_in_idle (res);

  return FALSE;
}

static void
g_file_real_copy_async (GFile                  *source,
			GFile                  *destination,
			GFileCopyFlags          flags,
			int                     io_priority,
			GCancellable           *cancellable,
			GFileProgressCallback   progress_callback,
			gpointer                progress_callback_data,
			GAsyncReadyCallback     callback,
			gpointer                user_data)
{
  GSimpleAsyncResult *res;
  CopyAsyncData *data;

  data = g_new0 (CopyAsyncData, 1);
  data->source = g_object_ref (source);
  data->destination = g_object_ref (destination);
  data->flags = flags;
  data->progress_cb = progress_callback;
  data->progress_cb_data = progress_callback_data;

  res = g_simple_async_result_new (G_OBJECT (source), callback, user_data, g_file_real_copy_async);
  g_simple_async_result_set_op_res_gpointer (res, data, (GDestroyNotify)copy_async_data_free);

  g_io_scheduler_push_job (copy_async_thread, res, g_object_unref, io_priority, cancellable);
}

static gboolean
g_file_real_copy_finish (GFile        *file,
			 GAsyncResult *res,
			 GError      **error)
{
  /* Error handled in g_file_copy_finish() */
  return TRUE;
}


/********************************************
 *   Default VFS operations                 *
 ********************************************/

/**
 * g_file_new_for_path:
 * @path: a string containing a relative or absolute path.
 * 
 * Constructs a #GFile for a given path. This operation never
 * fails, but the returned object might not support any I/O
 * operation if @path is malformed.
 * 
 * Returns: a new #GFile for the given @path. 
 **/
GFile *
g_file_new_for_path (const char *path)
{
  g_return_val_if_fail (path != NULL, NULL);

  return g_vfs_get_file_for_path (g_vfs_get_default (), path);
}
 
/**
 * g_file_new_for_uri:
 * @uri: a string containing a URI.
 * 
 * Constructs a #GFile for a given URI. This operation never 
 * fails, but the returned object might not support any I/O 
 * operation if @uri is malformed or if the uri type is 
 * not supported.
 * 
 * Returns: a #GFile for the given @uri.
 **/ 
GFile *
g_file_new_for_uri (const char *uri)
{
  g_return_val_if_fail (uri != NULL, NULL);

  return g_vfs_get_file_for_uri (g_vfs_get_default (), uri);
}
  
/**
 * g_file_parse_name:
 * @parse_name: a file name or path to be parsed.
 * 
 * Constructs a #GFile with the given @parse_name (i.e. something given by g_file_get_parse_name()).
 * This operation never fails, but the returned object might not support any I/O
 * operation if the @parse_name cannot be parsed.
 * 
 * Returns: a new #GFile.
 **/
GFile *
g_file_parse_name (const char *parse_name)
{
  g_return_val_if_fail (parse_name != NULL, NULL);

  return g_vfs_parse_name (g_vfs_get_default (), parse_name);
}

static gboolean
is_valid_scheme_character (char c)
{
  return g_ascii_isalnum (c) || c == '+' || c == '-' || c == '.';
}

/* Following RFC 2396, valid schemes are built like:
 *       scheme        = alpha *( alpha | digit | "+" | "-" | "." )
 */
static gboolean
has_valid_scheme (const char *uri)
{
  const char *p;
  
  p = uri;
  
  if (!g_ascii_isalpha (*p))
    return FALSE;

  do {
    p++;
  } while (is_valid_scheme_character (*p));

  return *p == ':';
}

/**
 * g_file_new_for_commandline_arg:
 * @arg: a command line string.
 * 
 * Creates a #GFile with the given argument from the command line. The value of
 * @arg can be either a URI, an absolute path or a relative path resolved
 * relative to the current working directory.
 * This operation never fails, but the returned object might not support any
 * I/O operation if @arg points to a malformed path.
 *
 * Returns: a new #GFile. 
 **/
GFile *
g_file_new_for_commandline_arg (const char *arg)
{
  GFile *file;
  char *filename;
  char *current_dir;
  
  g_return_val_if_fail (arg != NULL, NULL);
  
  if (g_path_is_absolute (arg))
    return g_file_new_for_path (arg);

  if (has_valid_scheme (arg))
    return g_file_new_for_uri (arg);
    
  current_dir = g_get_current_dir ();
  filename = g_build_filename (current_dir, arg, NULL);
  g_free (current_dir);
  
  file = g_file_new_for_path (filename);
  g_free (filename);
  
  return file;
}

/**
 * g_file_mount_enclosing_volume:
 * @location: input #GFile.
 * @flags: flags affecting the operation
 * @mount_operation: a #GMountOperation or %NULL to avoid user interaction.
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * @callback: a #GAsyncReadyCallback to call when the request is satisfied, or %NULL.
 * @user_data: the data to pass to callback function
 * 
 * Starts a @mount_operation, mounting the volume that contains the file @location. 
 * 
 * When this operation has completed, @callback will be called with
 * @user_user data, and the operation can be finalized with 
 * g_file_mount_enclosing_volume_finish().
 * 
 * If @cancellable is not %NULL, then the operation can be cancelled by
 * triggering the cancellable object from another thread. If the operation
 * was cancelled, the error %G_IO_ERROR_CANCELLED will be returned. 
 **/
void
g_file_mount_enclosing_volume (GFile               *location,
			       GMountMountFlags     flags,
			       GMountOperation     *mount_operation,
			       GCancellable        *cancellable,
			       GAsyncReadyCallback  callback,
			       gpointer             user_data)
{
  GFileIface *iface;

  g_return_if_fail (G_IS_FILE (location));

  iface = G_FILE_GET_IFACE (location);

  if (iface->mount_enclosing_volume == NULL)
    {
      g_simple_async_report_error_in_idle (G_OBJECT (location),
					   callback, user_data,
					   G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
					   _("volume doesn't implement mount"));
      
      return;
    }
  
  (* iface->mount_enclosing_volume) (location, flags, mount_operation, cancellable, callback, user_data);

}

/**
 * g_file_mount_enclosing_volume_finish:
 * @location: input #GFile.
 * @result: a #GAsyncResult.
 * @error: a #GError, or %NULL
 * 
 * Finishes a mount operation started by g_file_mount_enclosing_volume().
 * 
 * Returns: %TRUE if successful. If an error
 * has occurred, this function will return %FALSE and set @error
 * appropriately if present.
 **/
gboolean
g_file_mount_enclosing_volume_finish (GFile         *location,
				      GAsyncResult  *result,
				      GError       **error)
{
  GFileIface *iface;

  g_return_val_if_fail (G_IS_FILE (location), FALSE);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

  if (G_IS_SIMPLE_ASYNC_RESULT (result))
    {
      GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (result);
      if (g_simple_async_result_propagate_error (simple, error))
	return FALSE;
    }
  
  iface = G_FILE_GET_IFACE (location);

  return (* iface->mount_enclosing_volume_finish) (location, result, error);
}

/********************************************
 *   Utility functions                      *
 ********************************************/

/**
 * g_file_query_default_handler:
 * @file: a #GFile to open.
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * @error: a #GError, or %NULL
 *
 * Returns the #GAppInfo that is registered as the default
 * application to handle the file specified by @file.
 *
 * If @cancellable is not %NULL, then the operation can be cancelled by
 * triggering the cancellable object from another thread. If the operation
 * was cancelled, the error %G_IO_ERROR_CANCELLED will be returned. 
 *
 * Returns: a #GAppInfo if the handle was found, %NULL if there were errors.
 * When you are done with it, release it with g_object_unref()
 **/
GAppInfo *
g_file_query_default_handler (GFile                  *file,
			      GCancellable           *cancellable,
			      GError                **error)
{
  char *uri_scheme;
  const char *content_type;
  GAppInfo *appinfo;
  GFileInfo *info;
  char *path;
  
  uri_scheme = g_file_get_uri_scheme (file);
  if (uri_scheme && uri_scheme[0] != '\0')
    {
      appinfo = g_app_info_get_default_for_uri_scheme (uri_scheme);
      g_free (uri_scheme);

      if (appinfo != NULL)
        return appinfo;
    }

  info = g_file_query_info (file,
			    G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
			    0,
			    cancellable,
			    error);
  if (info == NULL)
    return NULL;

  appinfo = NULL;

  content_type = g_file_info_get_content_type (info);
  if (content_type)
    {
      /* Don't use is_native(), as we want to support fuse paths if availible */
      path = g_file_get_path (file);
      appinfo = g_app_info_get_default_for_type (content_type,
						 path == NULL);
      g_free (path);
    }
  
  g_object_unref (info);

  if (appinfo != NULL)
    return appinfo;

  g_set_error_literal (error, G_IO_ERROR,
                       G_IO_ERROR_NOT_SUPPORTED,
                       _("No application is registered as handling this file"));
  return NULL;
  
}


#define GET_CONTENT_BLOCK_SIZE 8192

/**
 * g_file_load_contents:
 * @file: input #GFile.
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * @contents: a location to place the contents of the file.
 * @length: a location to place the length of the contents of the file,
 *    or %NULL if the length is not needed
 * @etag_out: a location to place the current entity tag for the file,
 *    or %NULL if the entity tag is not needed
 * @error: a #GError, or %NULL
 *
 * Loads the content of the file into memory. The data is always 
 * zero-terminated, but this is not included in the resultant @length.
 * The returned @content should be freed with g_free() when no longer
 * needed.
 * 
 * If @cancellable is not %NULL, then the operation can be cancelled by
 * triggering the cancellable object from another thread. If the operation
 * was cancelled, the error %G_IO_ERROR_CANCELLED will be returned. 
 * 
 * Returns: %TRUE if the @file's contents were successfully loaded.
 * %FALSE if there were errors.
 **/
gboolean
g_file_load_contents (GFile         *file,
		      GCancellable  *cancellable,
		      char         **contents,
		      gsize         *length,
		      char         **etag_out,
		      GError       **error)
{
  GFileInputStream *in;
  GByteArray *content;
  gsize pos;
  gssize res;
  GFileInfo *info;

  g_return_val_if_fail (G_IS_FILE (file), FALSE);
  g_return_val_if_fail (contents != NULL, FALSE);

  in = g_file_read (file, cancellable, error);
  if (in == NULL)
    return FALSE;

  content = g_byte_array_new ();
  pos = 0;
  
  g_byte_array_set_size (content, pos + GET_CONTENT_BLOCK_SIZE + 1);
  while ((res = g_input_stream_read (G_INPUT_STREAM (in),
				     content->data + pos,
				     GET_CONTENT_BLOCK_SIZE,
				     cancellable, error)) > 0)
    {
      pos += res;
      g_byte_array_set_size (content, pos + GET_CONTENT_BLOCK_SIZE + 1);
    }

  if (etag_out)
    {
      *etag_out = NULL;
      
      info = g_file_input_stream_query_info (in,
					     G_FILE_ATTRIBUTE_ETAG_VALUE,
					     cancellable,
					     NULL);
      if (info)
	{
	  *etag_out = g_strdup (g_file_info_get_etag (info));
	  g_object_unref (info);
	}
    } 

  /* Ignore errors on close */
  g_input_stream_close (G_INPUT_STREAM (in), cancellable, NULL);
  g_object_unref (in);

  if (res < 0)
    {
      /* error is set already */
      g_byte_array_free (content, TRUE);
      return FALSE;
    }

  if (length)
    *length = pos;

  /* Zero terminate (we got an extra byte allocated for this */
  content->data[pos] = 0;
  
  *contents = (char *)g_byte_array_free (content, FALSE);
  
  return TRUE;
}

typedef struct {
  GFile *file;
  GError *error;
  GCancellable *cancellable;
  GFileReadMoreCallback read_more_callback;
  GAsyncReadyCallback callback;
  gpointer user_data;
  GByteArray *content;
  gsize pos;
  char *etag;
} LoadContentsData;


static void
load_contents_data_free (LoadContentsData *data)
{
  if (data->error)
    g_error_free (data->error);
  if (data->cancellable)
    g_object_unref (data->cancellable);
  if (data->content)
    g_byte_array_free (data->content, TRUE);
  g_free (data->etag);
  g_object_unref (data->file);
  g_free (data);
}

static void
load_contents_close_callback (GObject      *obj,
			      GAsyncResult *close_res,
			      gpointer      user_data)
{
  GInputStream *stream = G_INPUT_STREAM (obj);
  LoadContentsData *data = user_data;
  GSimpleAsyncResult *res;

  /* Ignore errors here, we're only reading anyway */
  g_input_stream_close_finish (stream, close_res, NULL);
  g_object_unref (stream);

  res = g_simple_async_result_new (G_OBJECT (data->file),
				   data->callback,
				   data->user_data,
				   g_file_load_contents_async);
  g_simple_async_result_set_op_res_gpointer (res, data, (GDestroyNotify)load_contents_data_free);
  g_simple_async_result_complete (res);
  g_object_unref (res);
}

static void
load_contents_fstat_callback (GObject      *obj,
			      GAsyncResult *stat_res,
			      gpointer      user_data)
{
  GInputStream *stream = G_INPUT_STREAM (obj);
  LoadContentsData *data = user_data;
  GFileInfo *info;

  info = g_file_input_stream_query_info_finish (G_FILE_INPUT_STREAM (stream),
						   stat_res, NULL);
  if (info)
    {
      data->etag = g_strdup (g_file_info_get_etag (info));
      g_object_unref (info);
    }

  g_input_stream_close_async (stream, 0,
			      data->cancellable,
			      load_contents_close_callback, data);
}

static void
load_contents_read_callback (GObject      *obj,
			     GAsyncResult *read_res,
			     gpointer      user_data)
{
  GInputStream *stream = G_INPUT_STREAM (obj);
  LoadContentsData *data = user_data;
  GError *error = NULL;
  gssize read_size;

  read_size = g_input_stream_read_finish (stream, read_res, &error);

  if (read_size < 0) 
    {
      /* Error or EOF, close the file */
      data->error = error;
      g_input_stream_close_async (stream, 0,
				  data->cancellable,
				  load_contents_close_callback, data);
    }
  else if (read_size == 0)
    {
      g_file_input_stream_query_info_async (G_FILE_INPUT_STREAM (stream),
					    G_FILE_ATTRIBUTE_ETAG_VALUE,
					    0,
					    data->cancellable,
					    load_contents_fstat_callback,
					    data);
    }
  else if (read_size > 0)
    {
      data->pos += read_size;
      
      g_byte_array_set_size (data->content,
			     data->pos + GET_CONTENT_BLOCK_SIZE);


      if (data->read_more_callback &&
	  !data->read_more_callback ((char *)data->content->data, data->pos, data->user_data))
	g_file_input_stream_query_info_async (G_FILE_INPUT_STREAM (stream),
					      G_FILE_ATTRIBUTE_ETAG_VALUE,
					      0,
					      data->cancellable,
					      load_contents_fstat_callback,
					      data);
      else 
	g_input_stream_read_async (stream,
				   data->content->data + data->pos,
				   GET_CONTENT_BLOCK_SIZE,
				   0,
				   data->cancellable,
				   load_contents_read_callback,
				   data);
    }
}

static void
load_contents_open_callback (GObject      *obj,
			     GAsyncResult *open_res,
			     gpointer      user_data)
{
  GFile *file = G_FILE (obj);
  GFileInputStream *stream;
  LoadContentsData *data = user_data;
  GError *error = NULL;
  GSimpleAsyncResult *res;

  stream = g_file_read_finish (file, open_res, &error);

  if (stream)
    {
      g_byte_array_set_size (data->content,
			     data->pos + GET_CONTENT_BLOCK_SIZE);
      g_input_stream_read_async (G_INPUT_STREAM (stream),
				 data->content->data + data->pos,
				 GET_CONTENT_BLOCK_SIZE,
				 0,
				 data->cancellable,
				 load_contents_read_callback,
				 data);
      
    }
  else
    {
      res = g_simple_async_result_new_from_error (G_OBJECT (data->file),
						  data->callback,
						  data->user_data,
						  error);
      g_simple_async_result_complete (res);
      g_error_free (error);
      load_contents_data_free (data);
      g_object_unref (res);
    }
}

/**
 * g_file_load_partial_contents_async:
 * @file: input #GFile.
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * @read_more_callback: a #GFileReadMoreCallback to receive partial data and to specify whether further data should be read.
 * @callback: a #GAsyncReadyCallback to call when the request is satisfied
 * @user_data: the data to pass to the callback functions.
 *
 * Reads the partial contents of a file. A #GFileReadMoreCallback should be 
 * used to stop reading from the file when appropriate, else this function
 * will behave exactly as g_file_load_contents_async(). This operation 
 * can be finished by g_file_load_partial_contents_finish().
 *
 * Users of this function should be aware that @user_data is passed to 
 * both the @read_more_callback and the @callback.
 *
 * If @cancellable is not %NULL, then the operation can be cancelled by
 * triggering the cancellable object from another thread. If the operation
 * was cancelled, the error %G_IO_ERROR_CANCELLED will be returned. 
 **/
void
g_file_load_partial_contents_async (GFile                 *file,
				    GCancellable          *cancellable,
				    GFileReadMoreCallback  read_more_callback,
				    GAsyncReadyCallback    callback,
				    gpointer               user_data)
{
  LoadContentsData *data;

  g_return_if_fail (G_IS_FILE (file));

  data = g_new0 (LoadContentsData, 1);

  if (cancellable)
    data->cancellable = g_object_ref (cancellable);
  data->read_more_callback = read_more_callback;
  data->callback = callback;
  data->user_data = user_data;
  data->content = g_byte_array_new ();
  data->file = g_object_ref (file);

  g_file_read_async (file,
		     0,
		     cancellable,
		     load_contents_open_callback,
		     data);
}

/**
 * g_file_load_partial_contents_finish:
 * @file: input #GFile.
 * @res: a #GAsyncResult. 
 * @contents: a location to place the contents of the file.
 * @length: a location to place the length of the contents of the file,
 *     or %NULL if the length is not needed
 * @etag_out: a location to place the current entity tag for the file,
 *     or %NULL if the entity tag is not needed
 * @error: a #GError, or %NULL
 * 
 * Finishes an asynchronous partial load operation that was started
 * with g_file_load_partial_contents_async(). The data is always 
 * zero-terminated, but this is not included in the resultant @length.
 * The returned @content should be freed with g_free() when no longer
 * needed.
 *
 * Returns: %TRUE if the load was successful. If %FALSE and @error is 
 * present, it will be set appropriately. 
 **/
gboolean
g_file_load_partial_contents_finish (GFile         *file,
				     GAsyncResult  *res,
				     char         **contents,
				     gsize         *length,
				     char         **etag_out,
				     GError       **error)
{
  GSimpleAsyncResult *simple;
  LoadContentsData *data;

  g_return_val_if_fail (G_IS_FILE (file), FALSE);
  g_return_val_if_fail (G_IS_SIMPLE_ASYNC_RESULT (res), FALSE);
  g_return_val_if_fail (contents != NULL, FALSE);

  simple = G_SIMPLE_ASYNC_RESULT (res);

  if (g_simple_async_result_propagate_error (simple, error))
    return FALSE;
  
  g_warn_if_fail (g_simple_async_result_get_source_tag (simple) == g_file_load_contents_async);
  
  data = g_simple_async_result_get_op_res_gpointer (simple);

  if (data->error)
    {
      g_propagate_error (error, data->error);
      data->error = NULL;
      *contents = NULL;
      if (length)
	*length = 0;
      return FALSE;
    }

  if (length)
    *length = data->pos;

  if (etag_out)
    {
      *etag_out = data->etag;
      data->etag = NULL;
    }

  /* Zero terminate */
  g_byte_array_set_size (data->content, data->pos + 1);
  data->content->data[data->pos] = 0;
  
  *contents = (char *)g_byte_array_free (data->content, FALSE);
  data->content = NULL;

  return TRUE;
}

/**
 * g_file_load_contents_async:
 * @file: input #GFile.
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * @callback: a #GAsyncReadyCallback to call when the request is satisfied
 * @user_data: the data to pass to callback function
 * 
 * Starts an asynchronous load of the @file's contents.
 *
 * For more details, see g_file_load_contents() which is
 * the synchronous version of this call.
 *
 * When the load operation has completed, @callback will be called 
 * with @user data. To finish the operation, call 
 * g_file_load_contents_finish() with the #GAsyncResult returned by 
 * the @callback.
 * 
 * If @cancellable is not %NULL, then the operation can be cancelled by
 * triggering the cancellable object from another thread. If the operation
 * was cancelled, the error %G_IO_ERROR_CANCELLED will be returned. 
 **/
void
g_file_load_contents_async (GFile               *file,
			   GCancellable        *cancellable,
			   GAsyncReadyCallback  callback,
			   gpointer             user_data)
{
  g_file_load_partial_contents_async (file,
				      cancellable,
				      NULL,
				      callback, user_data);
}

/**
 * g_file_load_contents_finish:
 * @file: input #GFile.
 * @res: a #GAsyncResult. 
 * @contents: a location to place the contents of the file.
 * @length: a location to place the length of the contents of the file,
 *     or %NULL if the length is not needed
 * @etag_out: a location to place the current entity tag for the file,
 *     or %NULL if the entity tag is not needed
 * @error: a #GError, or %NULL
 * 
 * Finishes an asynchronous load of the @file's contents. 
 * The contents are placed in @contents, and @length is set to the 
 * size of the @contents string. The @content should be freed with
 * g_free() when no longer needed. If @etag_out is present, it will be 
 * set to the new entity tag for the @file.
 * 
 * Returns: %TRUE if the load was successful. If %FALSE and @error is 
 * present, it will be set appropriately. 
 **/
gboolean
g_file_load_contents_finish (GFile         *file,
			     GAsyncResult  *res,
			     char         **contents,
			     gsize         *length,
			     char         **etag_out,
			     GError       **error)
{
  return g_file_load_partial_contents_finish (file,
					      res,
					      contents,
					      length,
					      etag_out,
					      error);
}
  
/**
 * g_file_replace_contents:
 * @file: input #GFile.
 * @contents: a string containing the new contents for @file.
 * @length: the length of @contents in bytes.
 * @etag: the old <link linkend="gfile-etag">entity tag</link> 
 *     for the document, or %NULL
 * @make_backup: %TRUE if a backup should be created.
 * @flags: a set of #GFileCreateFlags.
 * @new_etag: a location to a new <link linkend="gfile-etag">entity tag</link>
 *      for the document. This should be freed with g_free() when no longer 
 *      needed, or %NULL
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * @error: a #GError, or %NULL
 *
 * Replaces the contents of @file with @contents of @length bytes.
 
 * If @etag is specified (not %NULL) any existing file must have that etag, or
 * the error %G_IO_ERROR_WRONG_ETAG will be returned.
 *
 * If @make_backup is %TRUE, this function will attempt to make a backup of @file.
 * 
 * If @cancellable is not %NULL, then the operation can be cancelled by
 * triggering the cancellable object from another thread. If the operation
 * was cancelled, the error %G_IO_ERROR_CANCELLED will be returned. 
 *
 * The returned @new_etag can be used to verify that the file hasn't changed the
 * next time it is saved over.
 * 
 * Returns: %TRUE if successful. If an error
 * has occurred, this function will return %FALSE and set @error
 * appropriately if present.
 **/
gboolean
g_file_replace_contents (GFile             *file,
			 const char        *contents,
			 gsize              length,
			 const char        *etag,
			 gboolean           make_backup,
			 GFileCreateFlags   flags,
			 char             **new_etag,
			 GCancellable      *cancellable,
			 GError           **error)
{
  GFileOutputStream *out;
  gsize pos, remainder;
  gssize res;
  gboolean ret;

  g_return_val_if_fail (G_IS_FILE (file), FALSE);
  g_return_val_if_fail (contents != NULL, FALSE);

  out = g_file_replace (file, etag, make_backup, flags, cancellable, error);
  if (out == NULL)
    return FALSE;

  pos = 0;
  remainder = length;
  while (remainder > 0 &&
	 (res = g_output_stream_write (G_OUTPUT_STREAM (out),
				       contents + pos,
				       MIN (remainder, GET_CONTENT_BLOCK_SIZE),
				       cancellable,
				       error)) > 0)
    {
      pos += res;
      remainder -= res;
    }
  
  if (remainder > 0 && res < 0)
    {
      /* Ignore errors on close */
      g_output_stream_close (G_OUTPUT_STREAM (out), cancellable, NULL);
      g_object_unref (out);

      /* error is set already */
      return FALSE;
    }
  
  ret = g_output_stream_close (G_OUTPUT_STREAM (out), cancellable, error);

  if (new_etag)
    *new_etag = g_file_output_stream_get_etag (out);

  g_object_unref (out);

  return ret;
}

typedef struct {
  GFile *file;
  GError *error;
  GCancellable *cancellable;
  GAsyncReadyCallback callback;
  gpointer user_data;
  const char *content;
  gsize length;
  gsize pos;
  char *etag;
} ReplaceContentsData;

static void
replace_contents_data_free (ReplaceContentsData *data)
{
  if (data->error)
    g_error_free (data->error);
  if (data->cancellable)
    g_object_unref (data->cancellable);
  g_object_unref (data->file);
  g_free (data->etag);
  g_free (data);
}

static void
replace_contents_close_callback (GObject      *obj,
				 GAsyncResult *close_res,
				 gpointer      user_data)
{
  GOutputStream *stream = G_OUTPUT_STREAM (obj);
  ReplaceContentsData *data = user_data;
  GSimpleAsyncResult *res;

  /* Ignore errors here, we're only reading anyway */
  g_output_stream_close_finish (stream, close_res, NULL);
  g_object_unref (stream);

  data->etag = g_file_output_stream_get_etag (G_FILE_OUTPUT_STREAM (stream));
  
  res = g_simple_async_result_new (G_OBJECT (data->file),
				   data->callback,
				   data->user_data,
				   g_file_replace_contents_async);
  g_simple_async_result_set_op_res_gpointer (res, data, (GDestroyNotify)replace_contents_data_free);
  g_simple_async_result_complete (res);
  g_object_unref (res);
}

static void
replace_contents_write_callback (GObject      *obj,
				 GAsyncResult *read_res,
				 gpointer      user_data)
{
  GOutputStream *stream = G_OUTPUT_STREAM (obj);
  ReplaceContentsData *data = user_data;
  GError *error = NULL;
  gssize write_size;
  
  write_size = g_output_stream_write_finish (stream, read_res, &error);

  if (write_size <= 0) 
    {
      /* Error or EOF, close the file */
      if (write_size < 0)
	data->error = error;
      g_output_stream_close_async (stream, 0,
				   data->cancellable,
				   replace_contents_close_callback, data);
    }
  else if (write_size > 0)
    {
      data->pos += write_size;

      if (data->pos >= data->length)
	g_output_stream_close_async (stream, 0,
				     data->cancellable,
				     replace_contents_close_callback, data);
      else
	g_output_stream_write_async (stream,
				     data->content + data->pos,
				     data->length - data->pos,
				     0,
				     data->cancellable,
				     replace_contents_write_callback,
				     data);
    }
}

static void
replace_contents_open_callback (GObject      *obj,
				GAsyncResult *open_res,
				gpointer      user_data)
{
  GFile *file = G_FILE (obj);
  GFileOutputStream *stream;
  ReplaceContentsData *data = user_data;
  GError *error = NULL;
  GSimpleAsyncResult *res;

  stream = g_file_replace_finish (file, open_res, &error);

  if (stream)
    {
      g_output_stream_write_async (G_OUTPUT_STREAM (stream),
				   data->content + data->pos,
				   data->length - data->pos,
				   0,
				   data->cancellable,
				   replace_contents_write_callback,
				   data);
      
    }
  else
    {
      res = g_simple_async_result_new_from_error (G_OBJECT (data->file),
						  data->callback,
						  data->user_data,
						  error);
      g_simple_async_result_complete (res);
      g_error_free (error);
      replace_contents_data_free (data);
      g_object_unref (res);
    }
}

/**
 * g_file_replace_contents_async:
 * @file: input #GFile.
 * @contents: string of contents to replace the file with.
 * @length: the length of @contents in bytes.
 * @etag: a new <link linkend="gfile-etag">entity tag</link> for the @file, or %NULL
 * @make_backup: %TRUE if a backup should be created.
 * @flags: a set of #GFileCreateFlags.
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * @callback: a #GAsyncReadyCallback to call when the request is satisfied
 * @user_data: the data to pass to callback function
 * 
 * Starts an asynchronous replacement of @file with the given 
 * @contents of @length bytes. @etag will replace the document's 
 * current entity tag.
 * 
 * When this operation has completed, @callback will be called with
 * @user_user data, and the operation can be finalized with 
 * g_file_replace_contents_finish().
 * 
 * If @cancellable is not %NULL, then the operation can be cancelled by
 * triggering the cancellable object from another thread. If the operation
 * was cancelled, the error %G_IO_ERROR_CANCELLED will be returned. 
 * 
 * If @make_backup is %TRUE, this function will attempt to 
 * make a backup of @file.
 **/
void
g_file_replace_contents_async  (GFile               *file,
				const char          *contents,
				gsize                length,
				const char          *etag,
				gboolean             make_backup,
				GFileCreateFlags     flags,
				GCancellable        *cancellable,
				GAsyncReadyCallback  callback,
				gpointer             user_data)
{
  ReplaceContentsData *data;

  g_return_if_fail (G_IS_FILE (file));
  g_return_if_fail (contents != NULL);

  data = g_new0 (ReplaceContentsData, 1);

  if (cancellable)
    data->cancellable = g_object_ref (cancellable);
  data->callback = callback;
  data->user_data = user_data;
  data->content = contents;
  data->length = length;
  data->pos = 0;
  data->file = g_object_ref (file);

  g_file_replace_async (file,
			etag,
			make_backup,
			flags,
			0,
			cancellable,
			replace_contents_open_callback,
			data);
}
  
/**
 * g_file_replace_contents_finish:
 * @file: input #GFile.
 * @res: a #GAsyncResult. 
 * @new_etag: a location of a new <link linkend="gfile-etag">entity tag</link> 
 *     for the document. This should be freed with g_free() when it is no 
 *     longer needed, or %NULL
 * @error: a #GError, or %NULL 
 * 
 * Finishes an asynchronous replace of the given @file. See
 * g_file_replace_contents_async(). Sets @new_etag to the new entity 
 * tag for the document, if present.
 * 
 * Returns: %TRUE on success, %FALSE on failure.
 **/
gboolean
g_file_replace_contents_finish (GFile         *file,
				GAsyncResult  *res,
				char         **new_etag,
				GError       **error)
{
  GSimpleAsyncResult *simple;
  ReplaceContentsData *data;

  g_return_val_if_fail (G_IS_FILE (file), FALSE);
  g_return_val_if_fail (G_IS_SIMPLE_ASYNC_RESULT (res), FALSE);

  simple = G_SIMPLE_ASYNC_RESULT (res);

  if (g_simple_async_result_propagate_error (simple, error))
    return FALSE;
  
  g_warn_if_fail (g_simple_async_result_get_source_tag (simple) == g_file_replace_contents_async);
  
  data = g_simple_async_result_get_op_res_gpointer (simple);

  if (data->error)
    {
      g_propagate_error (error, data->error);
      data->error = NULL;
      return FALSE;
    }


  if (new_etag)
    {
      *new_etag = data->etag;
      data->etag = NULL; /* Take ownership */
    }
  
  return TRUE;
}

/**
 * g_file_start_mountable:
 * @file: input #GFile.
 * @flags: flags affecting the operation
 * @start_operation: a #GMountOperation, or %NULL to avoid user interaction.
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * @callback: a #GAsyncReadyCallback to call when the request is satisfied, or %NULL.
 * @user_data: the data to pass to callback function
 *
 * Starts a file of type G_FILE_TYPE_MOUNTABLE.
 * Using @start_operation, you can request callbacks when, for instance,
 * passwords are needed during authentication.
 *
 * If @cancellable is not %NULL, then the operation can be cancelled by
 * triggering the cancellable object from another thread. If the operation
 * was cancelled, the error %G_IO_ERROR_CANCELLED will be returned.
 *
 * When the operation is finished, @callback will be called. You can then call
 * g_file_mount_mountable_finish() to get the result of the operation.
 *
 * Since: 2.22
 */
void
g_file_start_mountable (GFile                      *file,
                        GDriveStartFlags            flags,
                        GMountOperation            *start_operation,
                        GCancellable               *cancellable,
                        GAsyncReadyCallback         callback,
                        gpointer                    user_data)
{
  GFileIface *iface;

  g_return_if_fail (G_IS_FILE (file));

  iface = G_FILE_GET_IFACE (file);

  if (iface->start_mountable == NULL)
    {
      g_simple_async_report_error_in_idle (G_OBJECT (file),
					   callback,
					   user_data,
					   G_IO_ERROR,
					   G_IO_ERROR_NOT_SUPPORTED,
					   _("Operation not supported"));
      return;
    }

  (* iface->start_mountable) (file,
			      flags,
			      start_operation,
			      cancellable,
			      callback,
			      user_data);
}

/**
 * g_file_start_mountable_finish:
 * @file: input #GFile.
 * @result: a #GAsyncResult.
 * @error: a #GError, or %NULL
 *
 * Finishes a start operation. See g_file_start_mountable() for details.
 *
 * Finish an asynchronous start operation that was started
 * with g_file_start_mountable().
 *
 * Returns: %TRUE if the operation finished successfully. %FALSE
 * otherwise.
 *
 * Since: 2.22
 */
gboolean
g_file_start_mountable_finish (GFile                      *file,
                               GAsyncResult               *result,
                               GError                    **error)
{
  GFileIface *iface;

  g_return_val_if_fail (G_IS_FILE (file), FALSE);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

  if (G_IS_SIMPLE_ASYNC_RESULT (result))
    {
      GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (result);
      if (g_simple_async_result_propagate_error (simple, error))
	return FALSE;
    }

  iface = G_FILE_GET_IFACE (file);
  return (* iface->start_mountable_finish) (file, result, error);
}

/**
 * g_file_stop_mountable:
 * @file: input #GFile.
 * @flags: flags affecting the operation
 * @mount_operation: a #GMountOperation, or %NULL to avoid user interaction.
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * @callback: a #GAsyncReadyCallback to call when the request is satisfied, or %NULL.
 * @user_data: the data to pass to callback function
 *
 * Stops a file of type G_FILE_TYPE_MOUNTABLE.
 *
 * If @cancellable is not %NULL, then the operation can be cancelled by
 * triggering the cancellable object from another thread. If the operation
 * was cancelled, the error %G_IO_ERROR_CANCELLED will be returned.
 *
 * When the operation is finished, @callback will be called. You can then call
 * g_file_stop_mountable_finish() to get the result of the operation.
 *
 * Since: 2.22
 */
void
g_file_stop_mountable (GFile                      *file,
                       GMountUnmountFlags          flags,
                       GMountOperation            *mount_operation,
                       GCancellable               *cancellable,
                       GAsyncReadyCallback         callback,
                       gpointer                    user_data)
{
  GFileIface *iface;

  g_return_if_fail (G_IS_FILE (file));

  iface = G_FILE_GET_IFACE (file);

  if (iface->stop_mountable == NULL)
    {
      g_simple_async_report_error_in_idle (G_OBJECT (file),
					   callback,
					   user_data,
					   G_IO_ERROR,
					   G_IO_ERROR_NOT_SUPPORTED,
					   _("Operation not supported"));
      return;
    }

  (* iface->stop_mountable) (file,
                             flags,
                             mount_operation,
                             cancellable,
                             callback,
                             user_data);
}

/**
 * g_file_stop_mountable_finish:
 * @file: input #GFile.
 * @result: a #GAsyncResult.
 * @error: a #GError, or %NULL
 *
 * Finishes an stop operation, see g_file_stop_mountable() for details.
 *
 * Finish an asynchronous stop operation that was started
 * with g_file_stop_mountable().
 *
 * Returns: %TRUE if the operation finished successfully. %FALSE
 * otherwise.
 *
 * Since: 2.22
 */
gboolean
g_file_stop_mountable_finish (GFile                      *file,
                              GAsyncResult               *result,
                              GError                    **error)
{
  GFileIface *iface;

  g_return_val_if_fail (G_IS_FILE (file), FALSE);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

  if (G_IS_SIMPLE_ASYNC_RESULT (result))
    {
      GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (result);
      if (g_simple_async_result_propagate_error (simple, error))
	return FALSE;
    }

  iface = G_FILE_GET_IFACE (file);
  return (* iface->stop_mountable_finish) (file, result, error);
}

/**
 * g_file_poll_mountable:
 * @file: input #GFile.
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * @callback: a #GAsyncReadyCallback to call when the request is satisfied, or %NULL.
 * @user_data: the data to pass to callback function
 *
 * Polls a file of type G_FILE_TYPE_MOUNTABLE.
 *
 * If @cancellable is not %NULL, then the operation can be cancelled by
 * triggering the cancellable object from another thread. If the operation
 * was cancelled, the error %G_IO_ERROR_CANCELLED will be returned.
 *
 * When the operation is finished, @callback will be called. You can then call
 * g_file_mount_mountable_finish() to get the result of the operation.
 *
 * Since: 2.22
 */
void
g_file_poll_mountable (GFile                      *file,
                       GCancellable               *cancellable,
                       GAsyncReadyCallback         callback,
                       gpointer                    user_data)
{
  GFileIface *iface;

  g_return_if_fail (G_IS_FILE (file));

  iface = G_FILE_GET_IFACE (file);

  if (iface->poll_mountable == NULL)
    {
      g_simple_async_report_error_in_idle (G_OBJECT (file),
					   callback,
					   user_data,
					   G_IO_ERROR,
					   G_IO_ERROR_NOT_SUPPORTED,
					   _("Operation not supported"));
      return;
    }

  (* iface->poll_mountable) (file,
                             cancellable,
                             callback,
                             user_data);
}

/**
 * g_file_poll_mountable_finish:
 * @file: input #GFile.
 * @result: a #GAsyncResult.
 * @error: a #GError, or %NULL
 *
 * Finishes a poll operation. See g_file_poll_mountable() for details.
 *
 * Finish an asynchronous poll operation that was polled
 * with g_file_poll_mountable().
 *
 * Returns: %TRUE if the operation finished successfully. %FALSE
 * otherwise.
 *
 * Since: 2.22
 */
gboolean
g_file_poll_mountable_finish (GFile                      *file,
                              GAsyncResult               *result,
                              GError                    **error)
{
  GFileIface *iface;

  g_return_val_if_fail (G_IS_FILE (file), FALSE);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

  if (G_IS_SIMPLE_ASYNC_RESULT (result))
    {
      GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (result);
      if (g_simple_async_result_propagate_error (simple, error))
	return FALSE;
    }

  iface = G_FILE_GET_IFACE (file);
  return (* iface->poll_mountable_finish) (file, result, error);
}

/**
 * g_file_supports_thread_contexts:
 * @file: a #GFile.
 *
 * Checks if @file supports <link
 * linkend="g-main-context-push-thread-default-context">thread-default
 * contexts</link>. If this returns %FALSE, you cannot perform
 * asynchronous operations on @file in a thread that has a
 * thread-default context.
 *
 * Returns: Whether or not @file supports thread-default contexts.
 *
 * Since: 2.22
 */
gboolean
g_file_supports_thread_contexts (GFile *file)
{
 GFileIface *iface;

 g_return_val_if_fail (G_IS_FILE (file), FALSE);

 iface = G_FILE_GET_IFACE (file);
 return iface->supports_thread_contexts;
}

#define __G_FILE_C__
#include "gioaliasdef.c"
