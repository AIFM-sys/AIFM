/* matrix/test.c
 * 
 * Copyright (C) 1996, 1997, 1998, 1999, 2000 Gerard Jungman, Brian Gough
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <config.h>

#if (!GSL_RANGE_CHECK) && HAVE_INLINE
#undef GSL_RANGE_CHECK
#define GSL_RANGE_CHECK 1
#endif

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <gsl/gsl_math.h>
#include <gsl/gsl_matrix.h>
#include <gsl/gsl_test.h>
#include <gsl/gsl_ieee_utils.h>

#define M 53
#define N 107

int status = 0;

#ifndef DESC
#define DESC ""
#endif

#define BASE_GSL_COMPLEX_LONG
#include "templates_on.h"
#include "test_complex_source.c"
#include "templates_off.h"
#undef  BASE_GSL_COMPLEX_LONG

#define BASE_GSL_COMPLEX
#include "templates_on.h"
#include "test_complex_source.c"
#include "templates_off.h"
#undef  BASE_GSL_COMPLEX

#define BASE_GSL_COMPLEX_FLOAT
#include "templates_on.h"
#include "test_complex_source.c"
#include "templates_off.h"
#undef  BASE_GSL_COMPLEX_FLOAT

#define BASE_LONG_DOUBLE
#include "templates_on.h"
#include "test_source.c"
#include "templates_off.h"
#undef  BASE_LONG_DOUBLE

#define BASE_DOUBLE
#include "templates_on.h"
#include "test_source.c"
#include "templates_off.h"
#undef  BASE_DOUBLE

#define BASE_FLOAT
#include "templates_on.h"
#include "test_source.c"
#include "templates_off.h"
#undef  BASE_FLOAT

#define BASE_ULONG
#include "templates_on.h"
#include "test_source.c"
#include "templates_off.h"
#undef  BASE_ULONG

#define BASE_LONG
#include "templates_on.h"
#include "test_source.c"
#include "templates_off.h"
#undef  BASE_LONG

#define BASE_UINT
#include "templates_on.h"
#include "test_source.c"
#include "templates_off.h"
#undef  BASE_UINT

#define BASE_INT
#include "templates_on.h"
#include "test_source.c"
#include "templates_off.h"
#undef  BASE_INT

#define BASE_USHORT
#include "templates_on.h"
#include "test_source.c"
#include "templates_off.h"
#undef  BASE_USHORT

#define BASE_SHORT
#include "templates_on.h"
#include "test_source.c"
#include "templates_off.h"
#undef  BASE_SHORT

#define BASE_UCHAR
#include "templates_on.h"
#include "test_source.c"
#include "templates_off.h"
#undef  BASE_UCHAR

#define BASE_CHAR
#include "templates_on.h"
#include "test_source.c"
#include "templates_off.h"
#undef  BASE_CHAR

void my_error_handler (const char *reason, const char *file,
                       int line, int err);

int
main (void)
{
  gsl_ieee_env_setup ();

  test_func ();
  test_float_func ();
  test_long_double_func ();
  test_ulong_func ();
  test_long_func ();
  test_uint_func ();
  test_int_func ();
  test_ushort_func ();
  test_short_func ();
  test_uchar_func ();
  test_char_func ();
  test_complex_func ();
  test_complex_float_func ();
  test_complex_long_double_func ();

  test_text ();
  test_float_text ();
#if HAVE_PRINTF_LONGDOUBLE
  test_long_double_text ();
#endif
  test_ulong_text ();
  test_long_text ();
  test_uint_text ();
  test_int_text ();
  test_ushort_text ();
  test_short_text ();
  test_uchar_text ();
  test_char_text ();
  test_complex_text ();
  test_complex_float_text ();
#if HAVE_PRINTF_LONGDOUBLE
  test_complex_long_double_text ();
#endif

  test_binary ();
  test_float_binary ();
  test_long_double_binary ();
  test_ulong_binary ();
  test_long_binary ();
  test_uint_binary ();
  test_int_binary ();
  test_ushort_binary ();
  test_short_binary ();
  test_uchar_binary ();
  test_char_binary ();
  test_complex_binary ();
  test_complex_float_binary ();
  test_complex_long_double_binary ();

#if GSL_RANGE_CHECK
  gsl_set_error_handler (&my_error_handler);

  test_trap ();
  test_float_trap ();
  test_long_double_trap ();
  test_ulong_trap ();
  test_long_trap ();
  test_uint_trap ();
  test_int_trap ();
  test_ushort_trap ();
  test_short_trap ();
  test_uchar_trap ();
  test_char_trap ();
  test_complex_trap ();
  test_complex_float_trap ();
  test_complex_long_double_trap ();
#endif

  test_complex_arith ();
  test_complex_float_arith ();
  test_complex_long_double_arith ();

  exit (gsl_test_summary ());
}

void
my_error_handler (const char *reason, const char *file, int line, int err)
{
  if (0)
    printf ("(caught [%s:%d: %s (%d)])\n", file, line, reason, err);
  status = 1;
}
