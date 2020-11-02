/* Unit tests for gstrfuncs
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This work is provided "as is"; redistribution and modification
 * in whole or in part, in any medium, physical or electronic is
 * permitted without restriction.
 *
 * This work is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * In no event shall the authors or contributors be liable for any
 * direct, indirect, incidental, special, exemplary, or consequential
 * damages (including, but not limited to, procurement of substitute
 * goods or services; loss of use, data, or profits; or business
 * interruption) however caused and on any theory of liability, whether
 * in contract, strict liability, or tort (including negligence or
 * otherwise) arising in any way out of the use of this software, even
 * if advised of the possibility of such damage.
 */

#include <ctype.h>
#include <errno.h>
#include <locale.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "glib.h"

#define GLIB_TEST_STRING "el dorado "

#define FOR_ALL_CTYPE(macro)	\
	macro(isalnum)		\
	macro(isalpha)		\
	macro(iscntrl)		\
	macro(isdigit)		\
	macro(isgraph)		\
	macro(islower)		\
	macro(isprint)		\
	macro(ispunct)		\
	macro(isspace)		\
	macro(isupper)		\
	macro(isxdigit)

#define DEFINE_CALL_CTYPE(function)		\
	static int				\
	call_##function (int c)			\
	{					\
		return function (c);		\
	}

#define DEFINE_CALL_G_ASCII_CTYPE(function)	\
	static gboolean				\
	call_g_ascii_##function (gchar c)	\
	{					\
		return g_ascii_##function (c);	\
	}

FOR_ALL_CTYPE (DEFINE_CALL_CTYPE)
FOR_ALL_CTYPE (DEFINE_CALL_G_ASCII_CTYPE)

static void
test_is_function (const char *name,
		  gboolean (* ascii_function) (gchar),
		  int (* c_library_function) (int),
		  gboolean (* unicode_function) (gunichar))
{
  int c;

  for (c = 0; c <= 0x7F; c++)
    {
      gboolean ascii_result = ascii_function ((gchar)c);
      gboolean c_library_result = c_library_function (c) != 0;
      gboolean unicode_result = unicode_function ((gunichar) c);
      if (ascii_result != c_library_result && c != '\v')
        {
	  g_error ("g_ascii_%s returned %d and %s returned %d for 0x%X",
		   name, ascii_result, name, c_library_result, c);
	}
      if (ascii_result != unicode_result)
	{
	  g_error ("g_ascii_%s returned %d and g_unichar_%s returned %d for 0x%X",
		   name, ascii_result, name, unicode_result, c);
	}
    }
  for (c = 0x80; c <= 0xFF; c++)
    {
      gboolean ascii_result = ascii_function ((gchar)c);
      if (ascii_result)
	{
	  g_error ("g_ascii_%s returned TRUE for 0x%X", name, c);
	}
    }
}

static void
test_to_function (const char *name,
		  gchar (* ascii_function) (gchar),
		  int (* c_library_function) (int),
		  gunichar (* unicode_function) (gunichar))
{
  int c;

  for (c = 0; c <= 0x7F; c++)
    {
      int ascii_result = (guchar) ascii_function ((gchar) c);
      int c_library_result = c_library_function (c);
      int unicode_result = unicode_function ((gunichar) c);
      if (ascii_result != c_library_result)
	{
	  g_error ("g_ascii_%s returned 0x%X and %s returned 0x%X for 0x%X",
		   name, ascii_result, name, c_library_result, c);
	}
      if (ascii_result != unicode_result)
	{
	  g_error ("g_ascii_%s returned 0x%X and g_unichar_%s returned 0x%X for 0x%X",
		   name, ascii_result, name, unicode_result, c);
	}
    }
  for (c = 0x80; c <= 0xFF; c++)
    {
      int ascii_result = (guchar) ascii_function ((gchar) c);
      if (ascii_result != c)
	{
	  g_error ("g_ascii_%s returned 0x%X for 0x%X",
		   name, ascii_result, c);
	}
    }
}

static void
test_digit_function (const char *name,
		     int (* ascii_function) (gchar),
		     int (* unicode_function) (gunichar))
{
  int c;

  for (c = 0; c <= 0x7F; c++)
    {
      int ascii_result = ascii_function ((gchar) c);
      int unicode_result = unicode_function ((gunichar) c);
      if (ascii_result != unicode_result)
	{
	  g_error ("g_ascii_%s_value returned %d and g_unichar_%s_value returned %d for 0x%X",
		   name, ascii_result, name, unicode_result, c);
	}
    }
  for (c = 0x80; c <= 0xFF; c++)
    {
      int ascii_result = ascii_function ((gchar) c);
      if (ascii_result != -1)
	{
	  g_error ("g_ascii_%s_value returned %d for 0x%X",
		   name, ascii_result, c);
	}
    }
}

static void
test_is_to_digit (void)
{
  #define TEST_IS(name) test_is_function (#name, call_g_ascii_##name, call_##name, g_unichar_##name);

  FOR_ALL_CTYPE(TEST_IS)

  #undef TEST_IS

  #define TEST_TO(name) test_to_function (#name, g_ascii_##name, name, g_unichar_##name)

  TEST_TO (tolower);
  TEST_TO (toupper);

  #undef TEST_TO

  #define TEST_DIGIT(name) test_digit_function (#name, g_ascii_##name##_value, g_unichar_##name##_value)

  TEST_DIGIT (digit);
  TEST_DIGIT (xdigit);

  #undef TEST_DIGIT
}

static void
test_strdup (void)
{
  gchar *str;

  str = g_strdup (NULL);
  g_assert (str == NULL);

  str = g_strdup (GLIB_TEST_STRING);
  g_assert (str != NULL);
  g_assert_cmpstr (str, ==, GLIB_TEST_STRING);
  g_free (str);
}

static void
test_strndup (void)
{
  gchar *str;

  str = g_strndup (NULL, 3);
  g_assert (str == NULL);

  str = g_strndup ("aaaa", 5);
  g_assert (str != NULL);
  g_assert_cmpstr (str, ==, "aaaa");
  g_free (str);

  str = g_strndup ("aaaa", 2);
  g_assert (str != NULL);
  g_assert_cmpstr (str, ==, "aa");
  g_free (str);
}

static void
test_strdup_printf (void)
{
  gchar *str;

  str = g_strdup_printf ("%05d %-5s", 21, "test");
  g_assert (str != NULL);
  g_assert_cmpstr (str, ==, "00021 test ");
  g_free (str);
}

static void
test_strdupv (void)
{
  gchar *vec[] = { "Foo", "Bar", NULL };
  gchar **copy;

  copy = g_strdupv (NULL);
  g_assert (copy == NULL);  

  copy = g_strdupv (vec);
  g_assert (copy != NULL);
  g_assert_cmpstr (copy[0], ==, "Foo");
  g_assert_cmpstr (copy[1], ==, "Bar");
  g_assert (copy[2] == NULL);
  g_strfreev (copy);
}

static void
test_strnfill (void)
{
  gchar *str;

  str = g_strnfill (0, 'a');
  g_assert (str != NULL);
  g_assert (*str == '\0');
  g_free (str);

  str = g_strnfill (5, 'a');
  g_assert (str != NULL);
  g_assert_cmpstr (str, ==, "aaaaa");
  g_free (str);
}

static void
test_strconcat (void)
{
  gchar *str;

  str = g_strconcat (GLIB_TEST_STRING, NULL);
  g_assert (str != NULL);
  g_assert_cmpstr (str, ==, GLIB_TEST_STRING);
  g_free (str);

  str = g_strconcat (GLIB_TEST_STRING,
		     GLIB_TEST_STRING, 
		     GLIB_TEST_STRING,
		     NULL);
  g_assert (str != NULL);
  g_assert_cmpstr (str, ==, GLIB_TEST_STRING GLIB_TEST_STRING GLIB_TEST_STRING);
  g_free (str);
}

static void
test_strjoin (void)
{
  gchar *str;

  str = g_strjoin (NULL, NULL);
  g_assert (str != NULL);
  g_assert (*str == '\0');
  g_free (str);

  str = g_strjoin (":", NULL);
  g_assert (str != NULL);
  g_assert (*str == '\0');
  g_free (str);

  str = g_strjoin (NULL, GLIB_TEST_STRING, NULL);
  g_assert (str != NULL);
  g_assert_cmpstr (str, ==, GLIB_TEST_STRING);
  g_free (str);

  str = g_strjoin (NULL,
		   GLIB_TEST_STRING,
		   GLIB_TEST_STRING, 
		   GLIB_TEST_STRING,
		   NULL);
  g_assert (str != NULL);
  g_assert_cmpstr (str, ==, GLIB_TEST_STRING GLIB_TEST_STRING GLIB_TEST_STRING);
  g_free (str);

  str = g_strjoin (":",
		   GLIB_TEST_STRING,
		   GLIB_TEST_STRING, 
		   GLIB_TEST_STRING,
		   NULL);
  g_assert (str != NULL);
  g_assert_cmpstr (str, ==, GLIB_TEST_STRING ":" GLIB_TEST_STRING ":" GLIB_TEST_STRING);
  g_free (str);
}

static void
test_strcanon (void)
{
  gchar *str;

  if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDERR))
    {
      str = g_strcanon (NULL, "ab", 'y');
    }
  g_test_trap_assert_failed ();

  if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDERR))
    {
      str = g_strdup ("abxabxab");
      str = g_strcanon (str, NULL, 'y');
      g_free (str);
    }
  g_test_trap_assert_failed ();

  str = g_strdup ("abxabxab");
  str = g_strcanon (str, "ab", 'y');
  g_assert (str != NULL);
  g_assert_cmpstr (str, ==, "abyabyab");
  g_free (str);
}

static void
test_strcompress_strescape (void)
{
  gchar *str;
  gchar *tmp;

  /* test compress */
  if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDERR))
    {
      str = g_strcompress (NULL);
    }
  g_test_trap_assert_failed ();

  /* trailing slashes are not allowed */
  if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDERR))
    {
      str = g_strcompress ("abc\\");
    }
  g_test_trap_assert_failed ();

  str = g_strcompress ("abc\\\\\\\"\\b\\f\\n\\r\\t\\003\\177\\234\\313\\12345z");
  g_assert (str != NULL);
  g_assert_cmpstr (str, ==, "abc\\\"\b\f\n\r\t\003\177\234\313\12345z");
  g_free (str);

  /* test escape */
  if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDERR))
    {
      str = g_strescape (NULL, NULL);
    }
  g_test_trap_assert_failed ();

  str = g_strescape ("abc\\\"\b\f\n\r\t\003\177\234\313", NULL);
  g_assert (str != NULL);
  g_assert_cmpstr (str, ==, "abc\\\\\\\"\\b\\f\\n\\r\\t\\003\\177\\234\\313");
  g_free (str);

  str = g_strescape ("abc\\\"\b\f\n\r\t\003\177\234\313",
		     "\b\f\001\002\003\004");
  g_assert (str != NULL);
  g_assert_cmpstr (str, ==, "abc\\\\\\\"\b\f\\n\\r\\t\003\\177\\234\\313");
  g_free (str);

  /* round trip */
  tmp = g_strescape ("abc\\\"\b\f\n\r\t\003\177\234\313", NULL);
  str = g_strcompress (tmp);
  g_assert (str != NULL); 
  g_assert_cmpstr (str, ==, "abc\\\"\b\f\n\r\t\003\177\234\313");
  g_free (str);
  g_free (tmp);
}

static void
test_ascii_strcasecmp (void)
{
  gboolean res;

  if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDERR))
    {
      res = g_ascii_strcasecmp ("foo", NULL);
    }
  g_test_trap_assert_failed ();

  if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDERR))
    {
      res = g_ascii_strcasecmp (NULL, "foo");
    }
  g_test_trap_assert_failed ();

  res = g_ascii_strcasecmp ("FroboZZ", "frobozz");
  g_assert_cmpint (res, ==, 0);

  res = g_ascii_strcasecmp ("frobozz", "frobozz");
  g_assert_cmpint (res, ==, 0);

  res = g_ascii_strcasecmp ("frobozz", "FROBOZZ");
  g_assert_cmpint (res, ==, 0);

  res = g_ascii_strcasecmp ("FROBOZZ", "froboz");
  g_assert_cmpint (res, !=, 0);

  res = g_ascii_strcasecmp ("", "");
  g_assert_cmpint (res, ==, 0);

  res = g_ascii_strcasecmp ("!#%&/()", "!#%&/()");
  g_assert_cmpint (res, ==, 0);

  res = g_ascii_strcasecmp ("a", "b");
  g_assert_cmpint (res, <, 0);

  res = g_ascii_strcasecmp ("a", "B");
  g_assert_cmpint (res, <, 0);

  res = g_ascii_strcasecmp ("A", "b");
  g_assert_cmpint (res, <, 0);

  res = g_ascii_strcasecmp ("A", "B");
  g_assert_cmpint (res, <, 0);

  res = g_ascii_strcasecmp ("b", "a");
  g_assert_cmpint (res, >, 0);

  res = g_ascii_strcasecmp ("b", "A");
  g_assert_cmpint (res, >, 0);

  res = g_ascii_strcasecmp ("B", "a");
  g_assert_cmpint (res, >, 0);

  res = g_ascii_strcasecmp ("B", "A");
  g_assert_cmpint (res, >, 0);
}

static void
do_test_strchug (const gchar *str, const gchar *expected)
{
  gchar *tmp;
  gboolean res;

  tmp = g_strdup (str);

  g_strchug (tmp);
  res = (strcmp (tmp, expected) == 0);
  g_free (tmp);

  g_assert_cmpint (res, ==, TRUE);
}

static void
test_strchug (void)
{
  if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDERR))
    {
      g_strchug (NULL);
    }
  g_test_trap_assert_failed ();

  do_test_strchug ("", "");
  do_test_strchug (" ", "");
  do_test_strchug ("\t\r\n ", "");
  do_test_strchug (" a", "a");
  do_test_strchug ("  a", "a");
  do_test_strchug ("a a", "a a");
  do_test_strchug (" a a", "a a");
}

static void
do_test_strchomp (const gchar *str, const gchar *expected)
{
  gchar *tmp;
  gboolean res;

  tmp = g_strdup (str);

  g_strchomp (tmp);
  res = (strcmp (tmp, expected) == 0);
  g_free (tmp);

  g_assert_cmpint (res, ==, TRUE);
}

static void
test_strchomp (void)
{
  if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDERR))
    {
      g_strchomp (NULL);
    }
  g_test_trap_assert_failed ();

  do_test_strchomp ("", "");
  do_test_strchomp (" ", "");
  do_test_strchomp (" \t\r\n", "");
  do_test_strchomp ("a ", "a");
  do_test_strchomp ("a  ", "a");
  do_test_strchomp ("a a", "a a");
  do_test_strchomp ("a a ", "a a");
}

static void
test_strreverse (void)
{
  gchar *str;
  gchar *p;

  if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDERR))
    {
      str = g_strreverse (NULL);
    }
  g_test_trap_assert_failed ();

  str = p = g_strdup ("abcde");
  str = g_strreverse (str);
  g_assert (str != NULL);
  g_assert (p == str);
  g_assert_cmpstr (str, ==, "edcba");
  g_free (str);
}

static void
test_strstr (void)
{
  gchar *haystack;
  gchar *res;

  haystack = g_strdup ("FooBarFooBarFoo");

  /* strstr_len */
  res = g_strstr_len (haystack, 6, "xxx");
  g_assert (res == NULL);

  res = g_strstr_len (haystack, 6, "FooBarFooBarFooBar");
  g_assert (res == NULL);

  res = g_strstr_len (haystack, 3, "Bar");
  g_assert (res == NULL);

  res = g_strstr_len (haystack, 6, "");
  g_assert (res == haystack);
  g_assert_cmpstr (res, ==, "FooBarFooBarFoo");

  res = g_strstr_len (haystack, 6, "Bar");
  g_assert (res == haystack + 3);
  g_assert_cmpstr (res, ==, "BarFooBarFoo");

  res = g_strstr_len (haystack, -1, "Bar");
  g_assert (res == haystack + 3);
  g_assert_cmpstr (res, ==, "BarFooBarFoo");

  /* strrstr */
  res = g_strrstr (haystack, "xxx");
  g_assert (res == NULL);

  res = g_strrstr (haystack, "FooBarFooBarFooBar");
  g_assert (res == NULL);

  res = g_strrstr (haystack, "");
  g_assert (res == haystack);
  g_assert_cmpstr (res, ==, "FooBarFooBarFoo");

  res = g_strrstr (haystack, "Bar");
  g_assert (res == haystack + 9);
  g_assert_cmpstr (res, ==, "BarFoo");

  /* strrstr_len */
  res = g_strrstr_len (haystack, 14, "xxx");
  g_assert (res == NULL);

  res = g_strrstr_len (haystack, 14, "FooBarFooBarFooBar");
  g_assert (res == NULL);

  res = g_strrstr_len (haystack, 3, "Bar");
  g_assert (res == NULL);

  res = g_strrstr_len (haystack, 14, "BarFoo");
  g_assert (res == haystack + 3);
  g_assert_cmpstr (res, ==, "BarFooBarFoo");

  res = g_strrstr_len (haystack, 15, "BarFoo");
  g_assert (res == haystack + 9);
  g_assert_cmpstr (res, ==, "BarFoo");

  res = g_strrstr_len (haystack, -1, "BarFoo");
  g_assert (res == haystack + 9);
  g_assert_cmpstr (res, ==, "BarFoo");

  /* test case for strings with \0 in the middle */
  *(haystack + 7) = '\0';
  res = g_strstr_len (haystack, 15, "BarFoo");
  g_assert (res == NULL);

  g_free (haystack);
}

static void
test_has_prefix (void)
{
  gboolean res;

  if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDERR))
    {
      res = g_str_has_prefix ("foo", NULL);
    }
  g_test_trap_assert_failed ();

  if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDERR))
    {
      res = g_str_has_prefix (NULL, "foo");
    }
  g_test_trap_assert_failed ();

  res = g_str_has_prefix ("foo", "bar");
  g_assert_cmpint (res, ==, FALSE);

  res = g_str_has_prefix ("foo", "foobar");
  g_assert_cmpint (res, ==, FALSE);

  res = g_str_has_prefix ("foobar", "bar");
  g_assert_cmpint (res, ==, FALSE);

  res = g_str_has_prefix ("foobar", "foo");
  g_assert_cmpint (res, ==, TRUE);

  res = g_str_has_prefix ("foo", "");
  g_assert_cmpint (res, ==, TRUE);

  res = g_str_has_prefix ("foo", "foo");
  g_assert_cmpint (res, ==, TRUE);

  res = g_str_has_prefix ("", "");
  g_assert_cmpint (res, ==, TRUE);
}

static void
test_has_suffix (void)
{
  gboolean res;

  if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDERR))
    {
      res = g_str_has_suffix ("foo", NULL);
    }
  g_test_trap_assert_failed ();

  if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDERR))
    {
      res = g_str_has_suffix (NULL, "foo");
    }
  g_test_trap_assert_failed ();

  res = g_str_has_suffix ("foo", "bar");
  g_assert_cmpint (res, ==, FALSE);

  res = g_str_has_suffix ("bar", "foobar");
  g_assert_cmpint (res, ==, FALSE);

  res = g_str_has_suffix ("foobar", "foo");
  g_assert_cmpint (res, ==, FALSE);

  res = g_str_has_suffix ("foobar", "bar");
  g_assert_cmpint (res, ==, TRUE);

  res = g_str_has_suffix ("foo", "");
  g_assert_cmpint (res, ==, TRUE);

  res = g_str_has_suffix ("foo", "foo");
  g_assert_cmpint (res, ==, TRUE);

  res = g_str_has_suffix ("", "");
  g_assert_cmpint (res, ==, TRUE);
}

static void
strv_check (gchar **strv, ...)
{
  gboolean ok = TRUE;
  gint i = 0;
  va_list list;

  va_start (list, strv);
  while (ok)
    {
      const gchar *str = va_arg (list, const char *);
      if (strv[i] == NULL)
	{
	  g_assert (str == NULL);
	  break;
	}
      if (str == NULL)
        {
	  ok = FALSE;
        }
      else
        {
          g_assert_cmpstr (strv[i], ==, str);
        }
      i++;
    }
  va_end (list);

  g_strfreev (strv);
}

static void
test_strsplit (void)
{
  strv_check (g_strsplit ("", ",", 0), NULL);
  strv_check (g_strsplit ("x", ",", 0), "x", NULL);
  strv_check (g_strsplit ("x,y", ",", 0), "x", "y", NULL);
  strv_check (g_strsplit ("x,y,", ",", 0), "x", "y", "", NULL);
  strv_check (g_strsplit (",x,y", ",", 0), "", "x", "y", NULL);
  strv_check (g_strsplit (",x,y,", ",", 0), "", "x", "y", "", NULL);
  strv_check (g_strsplit ("x,y,z", ",", 0), "x", "y", "z", NULL);
  strv_check (g_strsplit ("x,y,z,", ",", 0), "x", "y", "z", "", NULL);
  strv_check (g_strsplit (",x,y,z", ",", 0), "", "x", "y", "z", NULL);
  strv_check (g_strsplit (",x,y,z,", ",", 0), "", "x", "y", "z", "", NULL);
  strv_check (g_strsplit (",,x,,y,,z,,", ",", 0), "", "", "x", "", "y", "", "z", "", "", NULL);
  strv_check (g_strsplit (",,x,,y,,z,,", ",,", 0), "", "x", "y", "z", "", NULL);

  strv_check (g_strsplit ("", ",", 1), NULL);
  strv_check (g_strsplit ("x", ",", 1), "x", NULL);
  strv_check (g_strsplit ("x,y", ",", 1), "x,y", NULL);
  strv_check (g_strsplit ("x,y,", ",", 1), "x,y,", NULL);
  strv_check (g_strsplit (",x,y", ",", 1), ",x,y", NULL);
  strv_check (g_strsplit (",x,y,", ",", 1), ",x,y,", NULL);
  strv_check (g_strsplit ("x,y,z", ",", 1), "x,y,z", NULL);
  strv_check (g_strsplit ("x,y,z,", ",", 1), "x,y,z,", NULL);
  strv_check (g_strsplit (",x,y,z", ",", 1), ",x,y,z", NULL);
  strv_check (g_strsplit (",x,y,z,", ",", 1), ",x,y,z,", NULL);
  strv_check (g_strsplit (",,x,,y,,z,,", ",", 1), ",,x,,y,,z,,", NULL);
  strv_check (g_strsplit (",,x,,y,,z,,", ",,", 1), ",,x,,y,,z,,", NULL);

  strv_check (g_strsplit ("", ",", 2), NULL);
  strv_check (g_strsplit ("x", ",", 2), "x", NULL);
  strv_check (g_strsplit ("x,y", ",", 2), "x", "y", NULL);
  strv_check (g_strsplit ("x,y,", ",", 2), "x", "y,", NULL);
  strv_check (g_strsplit (",x,y", ",", 2), "", "x,y", NULL);
  strv_check (g_strsplit (",x,y,", ",", 2), "", "x,y,", NULL);
  strv_check (g_strsplit ("x,y,z", ",", 2), "x", "y,z", NULL);
  strv_check (g_strsplit ("x,y,z,", ",", 2), "x", "y,z,", NULL);
  strv_check (g_strsplit (",x,y,z", ",", 2), "", "x,y,z", NULL);
  strv_check (g_strsplit (",x,y,z,", ",", 2), "", "x,y,z,", NULL);
  strv_check (g_strsplit (",,x,,y,,z,,", ",", 2), "", ",x,,y,,z,,", NULL);
  strv_check (g_strsplit (",,x,,y,,z,,", ",,", 2), "", "x,,y,,z,,", NULL);
}

static void
test_strsplit_set (void)
{
  strv_check (g_strsplit_set ("", ",/", 0), NULL);
  strv_check (g_strsplit_set (":def/ghi:", ":/", -1), "", "def", "ghi", "", NULL);
  strv_check (g_strsplit_set ("abc:def/ghi", ":/", -1), "abc", "def", "ghi", NULL);
  strv_check (g_strsplit_set (",;,;,;,;", ",;", -1), "", "", "", "", "", "", "", "", "", NULL);
  strv_check (g_strsplit_set (",,abc.def", ".,", -1), "", "", "abc", "def", NULL);

  strv_check (g_strsplit_set (",x.y", ",.", 0), "", "x", "y", NULL);
  strv_check (g_strsplit_set (".x,y,", ",.", 0), "", "x", "y", "", NULL);
  strv_check (g_strsplit_set ("x,y.z", ",.", 0), "x", "y", "z", NULL);
  strv_check (g_strsplit_set ("x.y,z,", ",.", 0), "x", "y", "z", "", NULL);
  strv_check (g_strsplit_set (",x.y,z", ",.", 0), "", "x", "y", "z", NULL);
  strv_check (g_strsplit_set (",x,y,z,", ",.", 0), "", "x", "y", "z", "", NULL);
  strv_check (g_strsplit_set (",.x,,y,;z..", ".,;", 0), "", "", "x", "", "y", "", "z", "", "", NULL);
  strv_check (g_strsplit_set (",,x,,y,,z,,", ",,", 0), "", "", "x", "", "y", "", "z", "", "", NULL);

  strv_check (g_strsplit_set ("x,y.z", ",.", 1), "x,y.z", NULL);
  strv_check (g_strsplit_set ("x.y,z,", ",.", 1), "x.y,z,", NULL);
  strv_check (g_strsplit_set (",x,y,z", ",.", 1), ",x,y,z", NULL);
  strv_check (g_strsplit_set (",x,y.z,", ",.", 1), ",x,y.z,", NULL);
  strv_check (g_strsplit_set (",,x,.y,,z,,", ",.", 1), ",,x,.y,,z,,", NULL);
  strv_check (g_strsplit_set (",.x,,y,,z,,", ",,..", 1), ",.x,,y,,z,,", NULL);
   
  strv_check (g_strsplit_set ("", ",", 0), NULL);
  strv_check (g_strsplit_set ("x", ",", 0), "x", NULL);
  strv_check (g_strsplit_set ("x,y", ",", 0), "x", "y", NULL);
  strv_check (g_strsplit_set ("x,y,", ",", 0), "x", "y", "", NULL);
  strv_check (g_strsplit_set (",x,y", ",", 0), "", "x", "y", NULL);
  strv_check (g_strsplit_set (",x,y,", ",", 0), "", "x", "y", "", NULL);
  strv_check (g_strsplit_set ("x,y,z", ",", 0), "x", "y", "z", NULL);
  strv_check (g_strsplit_set ("x,y,z,", ",", 0), "x", "y", "z", "", NULL);
  strv_check (g_strsplit_set (",x,y,z", ",", 0), "", "x", "y", "z", NULL);
  strv_check (g_strsplit_set (",x,y,z,", ",", 0), "", "x", "y", "z", "", NULL);
  strv_check (g_strsplit_set (",,x,,y,,z,,", ",", 0), "", "", "x", "", "y", "", "z", "", "", NULL);

  strv_check (g_strsplit_set ("", ",", 1), NULL);
  strv_check (g_strsplit_set ("x", ",", 1), "x", NULL);
  strv_check (g_strsplit_set ("x,y", ",", 1), "x,y", NULL);
  strv_check (g_strsplit_set ("x,y,", ",", 1), "x,y,", NULL);
  strv_check (g_strsplit_set (",x,y", ",", 1), ",x,y", NULL);
  strv_check (g_strsplit_set (",x,y,", ",", 1), ",x,y,", NULL);
  strv_check (g_strsplit_set ("x,y,z", ",", 1), "x,y,z", NULL);
  strv_check (g_strsplit_set ("x,y,z,", ",", 1), "x,y,z,", NULL);
  strv_check (g_strsplit_set (",x,y,z", ",", 1), ",x,y,z", NULL);
  strv_check (g_strsplit_set (",x,y,z,", ",", 1), ",x,y,z,", NULL);
  strv_check (g_strsplit_set (",,x,,y,,z,,", ",", 1), ",,x,,y,,z,,", NULL);
  strv_check (g_strsplit_set (",,x,,y,,z,,", ",,", 1), ",,x,,y,,z,,", NULL);

  strv_check (g_strsplit_set ("", ",", 2), NULL);
  strv_check (g_strsplit_set ("x", ",", 2), "x", NULL);
  strv_check (g_strsplit_set ("x,y", ",", 2), "x", "y", NULL);
  strv_check (g_strsplit_set ("x,y,", ",", 2), "x", "y,", NULL);
  strv_check (g_strsplit_set (",x,y", ",", 2), "", "x,y", NULL);
  strv_check (g_strsplit_set (",x,y,", ",", 2), "", "x,y,", NULL);
  strv_check (g_strsplit_set ("x,y,z", ",", 2), "x", "y,z", NULL);
  strv_check (g_strsplit_set ("x,y,z,", ",", 2), "x", "y,z,", NULL);
  strv_check (g_strsplit_set (",x,y,z", ",", 2), "", "x,y,z", NULL);
  strv_check (g_strsplit_set (",x,y,z,", ",", 2), "", "x,y,z,", NULL);
  strv_check (g_strsplit_set (",,x,,y,,z,,", ",", 2), "", ",x,,y,,z,,", NULL);
  
  strv_check (g_strsplit_set (",,x,.y,..z,,", ",.", 3), "", "", "x,.y,..z,,", NULL);
}

static void
test_strv_length (void)
{
  guint l;

  if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDERR))
    {
      l = g_strv_length (NULL);
    }
  g_test_trap_assert_failed ();

  l = g_strv_length (g_strsplit ("1,2,3,4", ",", -1));
  g_assert_cmpuint (l, ==, 4);
}

static char *locales[] = {"sv_SE", "en_US", "fa_IR", "C", "ru_RU"};

void
check_strtod_string (gchar    *number,
		     double    res,
		     gboolean  check_end,
		     gint      correct_len)
{
  double d;
  gint l;
  gchar *dummy;

  /* we try a copy of number, with some free space for malloc before that. 
   * This is supposed to smash the some wrong pointer calculations. */

  dummy = g_malloc (100000);
  number = g_strdup (number);
  g_free (dummy);

  for (l = 0; l < G_N_ELEMENTS (locales); l++)
    {
      gboolean ok;
      gchar *end = "(unset)";

      setlocale (LC_NUMERIC, locales[l]);
      d = g_ascii_strtod (number, &end);
      ok = isnan (res) ? isnan (d) : (d == res);
      if (!ok)
	{
	  g_error ("g_ascii_strtod on \"%s\" for locale %s failed\n" \
                   "expected %f (nan %d) actual %f (nan %d)\n", 
		   number, locales[l],
		   res, isnan (res),
		   d, isnan (d));
	}

      ok = (end - number) == (check_end ? correct_len : strlen (number));
      if (!ok) {
	if (end == NULL)
	  g_error ("g_ascii_strtod on \"%s\" for locale %s endptr was NULL\n",
		   number, locales[l]);
	else if (end >= number && end <= number + strlen (number))
	  g_error ("g_ascii_strtod on \"%s\" for locale %s endptr was wrong, leftover: \"%s\"\n",
		   number, locales[l], end);
	else
	  g_error ("g_ascii_strtod on \"%s\" for locale %s endptr was REALLY wrong (number=%p, end=%p)\n",
		   number, locales[l], number, end);
      }
    }

  g_free (number);
}

static void
check_strtod_number (gdouble num, gchar *fmt, gchar *str)
{
  int l;
  gchar buf[G_ASCII_DTOSTR_BUF_SIZE];

  for (l = 0; l < G_N_ELEMENTS (locales); l++)
    {
      g_ascii_formatd (buf, G_ASCII_DTOSTR_BUF_SIZE, fmt, num);
      g_assert_cmpstr (buf, ==, str);
    }
}

static void
test_strtod (void)
{
  gdouble d, our_nan, our_inf;
  char buffer[G_ASCII_DTOSTR_BUF_SIZE];

#ifdef NAN
  our_nan = NAN;
#else
  /* Do this before any call to setlocale.  */
  our_nan = atof ("NaN");
#endif
  g_assert (isnan (our_nan));

#ifdef INFINITY
  our_inf = INFINITY;
#else
  our_inf = atof ("Infinity");
#endif
  g_assert (our_inf > 1 && our_inf == our_inf / 2);

  check_strtod_string ("123.123", 123.123, FALSE, 0);
  check_strtod_string ("123.123e2", 123.123e2, FALSE, 0);
  check_strtod_string ("123.123e-2", 123.123e-2, FALSE, 0);
  check_strtod_string ("-123.123", -123.123, FALSE, 0);
  check_strtod_string ("-123.123e2", -123.123e2, FALSE, 0);
  check_strtod_string ("-123.123e-2", -123.123e-2, FALSE, 0);
  check_strtod_string ("5.4", 5.4, TRUE, 3);
  check_strtod_string ("5.4,5.5", 5.4, TRUE, 3);
  check_strtod_string ("5,4", 5.0, TRUE, 1);
  /* the following are for #156421 */
  check_strtod_string ("1e1", 1e1, FALSE, 0); 
  check_strtod_string ("NAN", our_nan, FALSE, 0);
  check_strtod_string ("-nan", -our_nan, FALSE, 0);
  check_strtod_string ("INF", our_inf, FALSE, 0);
  check_strtod_string ("-infinity", -our_inf, FALSE, 0);
  check_strtod_string ("-.75,0", -0.75, TRUE, 4);
  
  d = 179769313486231570814527423731704356798070567525844996598917476803157260780028538760589558632766878171540458953514382464234321326889464182768467546703537516986049910576551282076245490090389328944075868508455133942304583236903222948165808559332123348274797826204144723168738177180919299881250404026184124858368.0;
  g_assert (d == g_ascii_strtod (g_ascii_dtostr (buffer, sizeof (buffer), d), NULL));

  d = -179769313486231570814527423731704356798070567525844996598917476803157260780028538760589558632766878171540458953514382464234321326889464182768467546703537516986049910576551282076245490090389328944075868508455133942304583236903222948165808559332123348274797826204144723168738177180919299881250404026184124858368.0;
  g_assert (d == g_ascii_strtod (g_ascii_dtostr (buffer, sizeof (buffer), d), NULL));
  
  d = pow (2.0, -1024.1);
  g_assert (d == g_ascii_strtod (g_ascii_dtostr (buffer, sizeof (buffer), d), NULL));
  
  d = -pow (2.0, -1024.1);
  g_assert (d == g_ascii_strtod (g_ascii_dtostr (buffer, sizeof (buffer), d), NULL));

  /* for #343899 */
  check_strtod_string (" 0.75", 0.75, FALSE, 0);
  check_strtod_string (" +0.75", 0.75, FALSE, 0);
  check_strtod_string (" -0.75", -0.75, FALSE, 0);
  check_strtod_string ("\f0.75", 0.75, FALSE, 0);
  check_strtod_string ("\n0.75", 0.75, FALSE, 0);
  check_strtod_string ("\r0.75", 0.75, FALSE, 0);
  check_strtod_string ("\t0.75", 0.75, FALSE, 0);

#if 0
  /* g_ascii_isspace() returns FALSE for vertical tab, see #59388 */
  check_strtod_string ("\v0.75", 0.75, FALSE, 0);
#endif

  /* for #343899 */
  check_strtod_number (0.75, "%0.2f", "0.75");
  check_strtod_number (0.75, "%5.2f", " 0.75");
  check_strtod_number (-0.75, "%0.2f", "-0.75");
  check_strtod_number (-0.75, "%5.2f", "-0.75");
  check_strtod_number (1e99, "%.0e", "1e+99");
}

static void
check_uint64 (const gchar *str,
	      const gchar *end,
	      gint         base,
	      guint64      result,
	      gint         error)
{
  guint64 actual;
  gchar *endptr = NULL;
  gint err;

  errno = 0;
  actual = g_ascii_strtoull (str, &endptr, base);
  err = errno;

  g_assert (actual == result);
  g_assert_cmpstr (end, ==, endptr);
  g_assert (err == error);
}

static void
check_int64 (const gchar *str,
	     const gchar *end,
	     gint         base,
	     gint64       result,
	     gint         error)
{
  gint64 actual;
  gchar *endptr = NULL;
  gint err;

  errno = 0;
  actual = g_ascii_strtoll (str, &endptr, base);
  err = errno;

  g_assert (actual == result);
  g_assert_cmpstr (end, ==, endptr);
  g_assert (err == error);
}

static void
test_strtoll (void)
{
  check_uint64 ("0", "", 10, 0, 0);
  check_uint64 ("+0", "", 10, 0, 0);
  check_uint64 ("-0", "", 10, 0, 0);
  check_uint64 ("18446744073709551615", "", 10, G_MAXUINT64, 0);
  check_uint64 ("18446744073709551616", "", 10, G_MAXUINT64, ERANGE);
  check_uint64 ("20xyz", "xyz", 10, 20, 0);
  check_uint64 ("-1", "", 10, G_MAXUINT64, 0);

  check_int64 ("0", "", 10, 0, 0);
  check_int64 ("9223372036854775807", "", 10, G_MAXINT64, 0);
  check_int64 ("9223372036854775808", "", 10, G_MAXINT64, ERANGE);
  check_int64 ("-9223372036854775808", "", 10, G_MININT64, 0);
  check_int64 ("-9223372036854775809", "", 10, G_MININT64, ERANGE);
  check_int64 ("32768", "", 10, 32768, 0);
  check_int64 ("-32768", "", 10, -32768, 0);
  check_int64 ("001", "", 10, 1, 0);
  check_int64 ("-001", "", 10, -1, 0);
}

static void
test_bounds (void)
{
  GMappedFile *file, *before, *after;
  char buffer[4097];
  char *tmp, *tmp2;
  char **array;
  char *string;

  /* if we allocate the file between two others and then free those
   * other two, then hopefully we end up with unmapped memory on either
   * side.
   */
  before = g_mapped_file_new ("4096-random-bytes", TRUE, NULL);

  /* quick workaround until #549783 can be fixed */
  if (before == NULL)
    return;

  file = g_mapped_file_new ("4096-random-bytes", TRUE, NULL);
  after = g_mapped_file_new ("4096-random-bytes", TRUE, NULL);
  g_mapped_file_free (before);
  g_mapped_file_free (after);

  g_assert (file != NULL);
  g_assert_cmpint (g_mapped_file_get_length (file), ==, 4096);
  string = g_mapped_file_get_contents (file);

  /* ensure they're all non-nul */
  g_assert (memchr (string, '\0', 4096) == NULL);

  /* test set 1: ensure that nothing goes past its maximum length, even in
   *             light of a missing nul terminator.
   *
   * we try to test all of the 'n' functions here.
   */
  tmp = g_strndup (string, 4096);
  g_assert_cmpint (strlen (tmp), ==, 4096);
  g_free (tmp);

  /* found no bugs in gnome, i hope :) */
  g_assert (g_strstr_len (string, 4096, "BUGS") == NULL);
  g_strstr_len (string, 4096, "B");
  g_strstr_len (string, 4096, ".");
  g_strstr_len (string, 4096, "");

  g_strrstr_len (string, 4096, "BUGS");
  g_strrstr_len (string, 4096, "B");
  g_strrstr_len (string, 4096, ".");
  g_strrstr_len (string, 4096, "");

  g_ascii_strdown (string, 4096);
  g_ascii_strdown (string, 4096);
  g_ascii_strup (string, 4096);
  g_ascii_strup (string, 4096);

  g_ascii_strncasecmp (string, string, 4096);

  tmp = g_markup_escape_text (string, 4096);
  g_free (tmp);

  /* test set 2: ensure that nothing reads even one byte past a '\0'.
   */
  g_assert_cmpint (string[4095], ==, '\n');
  string[4095] = '\0';

  tmp = g_strdup (string);
  g_assert_cmpint (strlen (tmp), ==, 4095);
  g_free (tmp);

  tmp = g_strndup (string, 10000);
  g_assert_cmpint (strlen (tmp), ==, 4095);
  g_free (tmp);

  g_stpcpy (buffer, string);
  g_assert_cmpint (strlen (buffer), ==, 4095);

  g_strstr_len (string, 10000, "BUGS");
  g_strstr_len (string, 10000, "B");
  g_strstr_len (string, 10000, ".");
  g_strstr_len (string, 10000, "");

  g_strrstr (string, "BUGS");
  g_strrstr (string, "B");
  g_strrstr (string, ".");
  g_strrstr (string, "");

  g_strrstr_len (string, 10000, "BUGS");
  g_strrstr_len (string, 10000, "B");
  g_strrstr_len (string, 10000, ".");
  g_strrstr_len (string, 10000, "");

  g_str_has_prefix (string, "this won't do very much...");
  g_str_has_suffix (string, "but maybe this will...");
  g_str_has_suffix (string, "HMMMM.");
  g_str_has_suffix (string, "MMMM.");
  g_str_has_suffix (string, "M.");

  g_strlcpy (buffer, string, sizeof buffer);
  g_assert_cmpint (strlen (buffer), ==, 4095);
  g_strlcpy (buffer, string, sizeof buffer);
  buffer[0] = '\0';
  g_strlcat (buffer, string, sizeof buffer);
  g_assert_cmpint (strlen (buffer), ==, 4095);

  tmp = g_strdup_printf ("<%s>", string);
  g_assert_cmpint (strlen (tmp), ==, 4095 + 2);
  g_free (tmp);

  g_ascii_strdown (string, -1);
  g_ascii_strdown (string, -1);
  g_ascii_strup (string, -1);
  g_ascii_strup (string, -1);

  g_ascii_strcasecmp (string, string);
  g_ascii_strncasecmp (string, string, 10000);

  g_strreverse (string);
  g_strreverse (string);
  g_strchug (string);
  g_strchomp (string);
  g_strstrip (string);
  g_assert_cmpint (strlen (string), ==, 4095);

  g_strdelimit (string, "M", 'N');
  g_strcanon (string, " N.", ':');
  g_assert_cmpint (strlen (string), ==, 4095);

  array = g_strsplit (string, ".", -1);
  tmp = g_strjoinv (".", array);
  g_strfreev (array);

  g_assert_cmpint (strlen (tmp), ==, 4095);
  g_assert (memcmp (tmp, string, 4095) == 0);
  g_free (tmp);

  tmp = g_strconcat (string, string, string, NULL);
  g_assert_cmpint (strlen (tmp), ==, 4095 * 3);
  g_free (tmp);

  tmp = g_strjoin ("!", string, string, NULL);
  g_assert_cmpint (strlen (tmp), ==, 4095 + 1 + 4095);
  g_free (tmp);

  tmp = g_markup_escape_text (string, -1);
  g_free (tmp);

  tmp = g_markup_printf_escaped ("%s", string);
  g_free (tmp);

  tmp = g_strescape (string, NULL);
  tmp2 = g_strcompress (tmp);
  g_assert_cmpstr (string, ==, tmp2);
  g_free (tmp2);
  g_free (tmp);

  g_mapped_file_free (file);
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/strfuncs/test-is-to-digit", test_is_to_digit);
  g_test_add_func ("/strfuncs/strdup", test_strdup);
  g_test_add_func ("/strfuncs/strndup", test_strndup);
  g_test_add_func ("/strfuncs/strdup-printf", test_strdup_printf);
  g_test_add_func ("/strfuncs/strdupv", test_strdupv);
  g_test_add_func ("/strfuncs/strnfill", test_strnfill);
  g_test_add_func ("/strfuncs/strconcat", test_strconcat);
  g_test_add_func ("/strfuncs/strjoin", test_strjoin);
  g_test_add_func ("/strfuncs/strcanon", test_strcanon);
  g_test_add_func ("/strfuncs/strcompress-strescape", test_strcompress_strescape);
  g_test_add_func ("/strfuncs/ascii-strcasecmp", test_ascii_strcasecmp);
  g_test_add_func ("/strfuncs/strchug", test_strchug);
  g_test_add_func ("/strfuncs/strchomp", test_strchomp);
  g_test_add_func ("/strfuncs/strreverse", test_strreverse);
  g_test_add_func ("/strfuncs/strstr", test_strstr);
  g_test_add_func ("/strfuncs/has-prefix", test_has_prefix);
  g_test_add_func ("/strfuncs/has-suffix", test_has_suffix);
  g_test_add_func ("/strfuncs/strsplit", test_strsplit);
  g_test_add_func ("/strfuncs/strsplit-set", test_strsplit_set);
  g_test_add_func ("/strfuncs/strv-length", test_strv_length);
  g_test_add_func ("/strfuncs/strtod", test_strtod);
  g_test_add_func ("/strfuncs/strtoull-strtoll", test_strtoll);
  g_test_add_func ("/strfuncs/bounds-check", test_bounds);

  return g_test_run();
}
