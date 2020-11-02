/* poly/test.c
 * 
 * Copyright (C) 1996, 1997, 1998, 1999, 2000 Brian Gough
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
#include <stdlib.h>
#include <gsl/gsl_math.h>
#include <gsl/gsl_test.h>
#include <gsl/gsl_ieee_utils.h>
#include <gsl/gsl_poly.h>

int
main (void)
{
  const double eps = 100.0 * GSL_DBL_EPSILON;

  gsl_ieee_env_setup ();

  {
    double x, y;
    double c[3] = { 1.0, 0.5, 0.3 };
    x = 0.5;
    y = gsl_poly_eval (c, 3, x);
    gsl_test_rel (y, 1 + 0.5 * x + 0.3 * x * x, eps,
                  "gsl_poly_eval({1, 0.5, 0.3}, 0.5)");
  }

  {
    double x, y;
    double d[11] = { 1, -1, 1, -1, 1, -1, 1, -1, 1, -1, 1 };
    x = 1.0;
    y = gsl_poly_eval (d, 11, x);
    gsl_test_rel (y, 1.0, eps,
                  "gsl_poly_eval({1,-1, 1, -1, 1, -1, 1, -1, 1, -1, 1}, 1.0)");

  }

  /* Quadratic */

  {
    double x0, x1;

    int n = gsl_poly_solve_quadratic (4.0, -20.0, 26.0, &x0, &x1);

    gsl_test (n != 0, "gsl_poly_solve_quadratic, no roots, (2x - 5)^2 = -1");
  }

  {
    double x0, x1;

    int n = gsl_poly_solve_quadratic (4.0, -20.0, 25.0, &x0, &x1);

    gsl_test (n != 2, "gsl_poly_solve_quadratic, one root, (2x - 5)^2 = 0");
    gsl_test_rel (x0, 2.5, 1e-9, "x0, (2x - 5)^2 = 0");
    gsl_test_rel (x1, 2.5, 1e-9, "x1, (2x - 5)^2 = 0");
    gsl_test (x0 != x1, "x0 == x1, (2x - 5)^2 = 0");
  }

  {
    double x0, x1;

    int n = gsl_poly_solve_quadratic (4.0, -20.0, 21.0, &x0, &x1);

    gsl_test (n != 2, "gsl_poly_solve_quadratic, two roots, (2x - 5)^2 = 4");
    gsl_test_rel (x0, 1.5, 1e-9, "x0, (2x - 5)^2 = 4");
    gsl_test_rel (x1, 3.5, 1e-9, "x1, (2x - 5)^2 = 4");
  }

  {
    double x0, x1;

    int n = gsl_poly_solve_quadratic (4.0, 7.0, 0.0, &x0, &x1);

    gsl_test (n != 2, "gsl_poly_solve_quadratic, two roots, x(4x + 7) = 0");
    gsl_test_rel (x0, -1.75, 1e-9, "x0, x(4x + 7) = 0");
    gsl_test_rel (x1, 0.0, 1e-9, "x1, x(4x + 7) = 0");
  }

  {
    double x0, x1;

    int n = gsl_poly_solve_quadratic (5.0, 0.0, -20.0, &x0, &x1);

    gsl_test (n != 2,
              "gsl_poly_solve_quadratic, two roots b = 0, 5 x^2 = 20");
    gsl_test_rel (x0, -2.0, 1e-9, "x0, 5 x^2 = 20");
    gsl_test_rel (x1, 2.0, 1e-9, "x1, 5 x^2 = 20");
  }


  {
    double x0, x1;

    int n = gsl_poly_solve_quadratic (0.0, 3.0, -21.0, &x0, &x1);

    gsl_test (n != 1,
              "gsl_poly_solve_quadratic, one root (linear) 3 x - 21 = 0");
    gsl_test_rel (x0, 7.0, 1e-9, "x0, 3x - 21 = 0");
  }


  {
    double x0, x1;
    int n = gsl_poly_solve_quadratic (0.0, 0.0, 1.0, &x0, &x1);

    gsl_test (n != 0,
              "gsl_poly_solve_quadratic, no roots 1 = 0");
  }


  /* Cubic */

  {
    double x0, x1, x2;

    int n = gsl_poly_solve_cubic (0.0, 0.0, -27.0, &x0, &x1, &x2);

    gsl_test (n != 1, "gsl_poly_solve_cubic, one root, x^3 = 27");
    gsl_test_rel (x0, 3.0, 1e-9, "x0, x^3 = 27");
  }

  {
    double x0, x1, x2;

    int n = gsl_poly_solve_cubic (-51.0, 867.0, -4913.0, &x0, &x1, &x2);

    gsl_test (n != 3, "gsl_poly_solve_cubic, three roots, (x-17)^3=0");
    gsl_test_rel (x0, 17.0, 1e-9, "x0, (x-17)^3=0");
    gsl_test_rel (x1, 17.0, 1e-9, "x1, (x-17)^3=0");
    gsl_test_rel (x2, 17.0, 1e-9, "x2, (x-17)^3=0");
  }

  {
    double x0, x1, x2;

    int n = gsl_poly_solve_cubic (-57.0, 1071.0, -6647.0, &x0, &x1, &x2);

    gsl_test (n != 3,
              "gsl_poly_solve_cubic, three roots, (x-17)(x-17)(x-23)=0");
    gsl_test_rel (x0, 17.0, 1e-9, "x0, (x-17)(x-17)(x-23)=0");
    gsl_test_rel (x1, 17.0, 1e-9, "x1, (x-17)(x-17)(x-23)=0");
    gsl_test_rel (x2, 23.0, 1e-9, "x2, (x-17)(x-17)(x-23)=0");
  }

  {
    double x0, x1, x2;

    int n = gsl_poly_solve_cubic (-11.0, -493.0, +6647.0, &x0, &x1, &x2);

    gsl_test (n != 3,
              "gsl_poly_solve_cubic, three roots, (x+23)(x-17)(x-17)=0");
    gsl_test_rel (x0, -23.0, 1e-9, "x0, (x+23)(x-17)(x-17)=0");
    gsl_test_rel (x1, 17.0, 1e-9, "x1, (x+23)(x-17)(x-17)=0");
    gsl_test_rel (x2, 17.0, 1e-9, "x2, (x+23)(x-17)(x-17)=0");
  }

  {
    double x0, x1, x2;

    int n = gsl_poly_solve_cubic (-143.0, 5087.0, -50065.0, &x0, &x1, &x2);

    gsl_test (n != 3,
              "gsl_poly_solve_cubic, three roots, (x-17)(x-31)(x-95)=0");
    gsl_test_rel (x0, 17.0, 1e-9, "x0, (x-17)(x-31)(x-95)=0");
    gsl_test_rel (x1, 31.0, 1e-9, "x1, (x-17)(x-31)(x-95)=0");
    gsl_test_rel (x2, 95.0, 1e-9, "x2, (x-17)(x-31)(x-95)=0");
  }

  {
    double x0, x1, x2;

    int n = gsl_poly_solve_cubic (-109.0, 803.0, 50065.0, &x0, &x1, &x2);

    gsl_test (n != 3,
              "gsl_poly_solve_cubic, three roots, (x+17)(x-31)(x-95)=0");
    gsl_test_rel (x0, -17.0, 1e-9, "x0, (x+17)(x-31)(x-95)=0");
    gsl_test_rel (x1, 31.0, 1e-9, "x1, (x+17)(x-31)(x-95)=0");
    gsl_test_rel (x2, 95.0, 1e-9, "x2, (x+17)(x-31)(x-95)=0");
  }

  /* Quadratic with complex roots */

  {
    gsl_complex z0, z1;

    int n = gsl_poly_complex_solve_quadratic (4.0, -20.0, 26.0, &z0, &z1);

    gsl_test (n != 2,
              "gsl_poly_complex_solve_quadratic, 2 roots (2x - 5)^2 = -1");
    gsl_test_rel (GSL_REAL (z0), 2.5, 1e-9, "z0.real, (2x - 5)^2 = -1");
    gsl_test_rel (GSL_IMAG (z0), -0.5, 1e-9, "z0.imag, (2x - 5)^2 = -1");

    gsl_test_rel (GSL_REAL (z1), 2.5, 1e-9, "z1.real, (2x - 5)^2 = -1");
    gsl_test_rel (GSL_IMAG (z1), 0.5, 1e-9, "z1.imag, (2x - 5)^2 = -1");
  }

  {
    gsl_complex z0, z1;

    int n = gsl_poly_complex_solve_quadratic (4.0, -20.0, 25.0, &z0, &z1);

    gsl_test (n != 2,
              "gsl_poly_complex_solve_quadratic, one root, (2x - 5)^2 = 0");
    gsl_test_rel (GSL_REAL (z0), 2.5, 1e-9, "z0.real, (2x - 5)^2 = 0");
    gsl_test_rel (GSL_IMAG (z0), 0.0, 1e-9, "z0.imag (2x - 5)^2 = 0");
    gsl_test_rel (GSL_REAL (z1), 2.5, 1e-9, "z1.real, (2x - 5)^2 = 0");
    gsl_test_rel (GSL_IMAG (z1), 0.0, 1e-9, "z1.imag (2x - 5)^2 = 0");
    gsl_test (GSL_REAL (z0) != GSL_REAL (z1),
              "z0.real == z1.real, (2x - 5)^2 = 0");
    gsl_test (GSL_IMAG (z0) != GSL_IMAG (z1),
              "z0.imag == z1.imag, (2x - 5)^2 = 0");
  }

  {
    gsl_complex z0, z1;

    int n = gsl_poly_complex_solve_quadratic (4.0, -20.0, 21.0, &z0, &z1);

    gsl_test (n != 2,
              "gsl_poly_complex_solve_quadratic, two roots, (2x - 5)^2 = 4");
    gsl_test_rel (GSL_REAL (z0), 1.5, 1e-9, "z0.real, (2x - 5)^2 = 4");
    gsl_test_rel (GSL_IMAG (z0), 0.0, 1e-9, "z0.imag, (2x - 5)^2 = 4");
    gsl_test_rel (GSL_REAL (z1), 3.5, 1e-9, "z1.real, (2x - 5)^2 = 4");
    gsl_test_rel (GSL_IMAG (z1), 0.0, 1e-9, "z1.imag, (2x - 5)^2 = 4");
  }

  {
    gsl_complex z0, z1;

    int n = gsl_poly_complex_solve_quadratic (4.0, 7.0, 0.0, &z0, &z1);

    gsl_test (n != 2,
              "gsl_poly_complex_solve_quadratic, two roots, x(4x + 7) = 0");
    gsl_test_rel (GSL_REAL (z0), -1.75, 1e-9, "z0.real, x(4x + 7) = 0");
    gsl_test_rel (GSL_IMAG (z0), 0.0, 1e-9, "z0.imag, x(4x + 7) = 0");
    gsl_test_rel (GSL_REAL (z1), 0.0, 1e-9, "z1.real, x(4x + 7) = 0");
    gsl_test_rel (GSL_IMAG (z1), 0.0, 1e-9, "z1.imag, x(4x + 7) = 0");
  }

  {
    gsl_complex z0, z1;

    int n = gsl_poly_complex_solve_quadratic (5.0, 0.0, -20.0, &z0, &z1);

    gsl_test (n != 2,
              "gsl_poly_complex_solve_quadratic, two roots b = 0, 5 x^2 = 20");
    gsl_test_rel (GSL_REAL (z0), -2.0, 1e-9, "z0.real, 5 x^2 = 20");
    gsl_test_rel (GSL_IMAG (z0), 0.0, 1e-9, "z0.imag, 5 x^2 = 20");
    gsl_test_rel (GSL_REAL (z1), 2.0, 1e-9, "z1.real, 5 x^2 = 20");
    gsl_test_rel (GSL_IMAG (z1), 0.0, 1e-9, "z1.imag, 5 x^2 = 20");
  }

  {
    gsl_complex z0, z1;

    int n = gsl_poly_complex_solve_quadratic (5.0, 0.0, 20.0, &z0, &z1);

    gsl_test (n != 2,
              "gsl_poly_complex_solve_quadratic, two roots b = 0, 5 x^2 = -20");
    gsl_test_rel (GSL_REAL (z0), 0.0, 1e-9, "z0.real, 5 x^2 = -20");
    gsl_test_rel (GSL_IMAG (z0), -2.0, 1e-9, "z0.imag, 5 x^2 = -20");
    gsl_test_rel (GSL_REAL (z1), 0.0, 1e-9, "z1.real, 5 x^2 = -20");
    gsl_test_rel (GSL_IMAG (z1), 2.0, 1e-9, "z1.imag, 5 x^2 = -20");
  }


  {
    gsl_complex z0, z1;

    int n = gsl_poly_complex_solve_quadratic (0.0, 3.0, -21.0, &z0, &z1);

    gsl_test (n != 1,
              "gsl_poly_complex_solve_quadratic, one root (linear) 3 x - 21 = 0");

    gsl_test_rel (GSL_REAL (z0), 7.0, 1e-9, "z0.real, 3x - 21 = 0");
    gsl_test_rel (GSL_IMAG (z0), 0.0, 1e-9, "z0.imag, 3x - 21 = 0");
  }


  {
    gsl_complex z0, z1;

    int n = gsl_poly_complex_solve_quadratic (0.0, 0.0, 1.0, &z0, &z1);
    gsl_test (n != 0,
              "gsl_poly_complex_solve_quadratic, no roots 1 = 0");
  }



  /* Cubic with complex roots */

  {
    gsl_complex z0, z1, z2;

    int n = gsl_poly_complex_solve_cubic (0.0, 0.0, -27.0, &z0, &z1, &z2);

    gsl_test (n != 3, "gsl_poly_complex_solve_cubic, three root, x^3 = 27");
    gsl_test_rel (GSL_REAL (z0), -1.5, 1e-9, "z0.real, x^3 = 27");
    gsl_test_rel (GSL_IMAG (z0), -1.5 * sqrt (3.0), 1e-9,
                  "z0.imag, x^3 = 27");
    gsl_test_rel (GSL_REAL (z1), -1.5, 1e-9, "z1.real, x^3 = 27");
    gsl_test_rel (GSL_IMAG (z1), 1.5 * sqrt (3.0), 1e-9, "z1.imag, x^3 = 27");
    gsl_test_rel (GSL_REAL (z2), 3.0, 1e-9, "z2.real, x^3 = 27");
    gsl_test_rel (GSL_IMAG (z2), 0.0, 1e-9, "z2.imag, x^3 = 27");
  }

  {
    gsl_complex z0, z1, z2;

    int n = gsl_poly_complex_solve_cubic (-1.0, 1.0, 39.0, &z0, &z1, &z2);

    gsl_test (n != 3,
              "gsl_poly_complex_solve_cubic, three root, (x+3)(x^2-4x+13) = 0");
    gsl_test_rel (GSL_REAL (z0), -3.0, 1e-9, "z0.real, (x+3)(x^2+1) = 0");
    gsl_test_rel (GSL_IMAG (z0), 0.0, 1e-9, "z0.imag, (x+3)(x^2+1) = 0");
    gsl_test_rel (GSL_REAL (z1), 2.0, 1e-9, "z1.real, (x+3)(x^2+1) = 0");
    gsl_test_rel (GSL_IMAG (z1), -3.0, 1e-9, "z1.imag, (x+3)(x^2+1) = 0");
    gsl_test_rel (GSL_REAL (z2), 2.0, 1e-9, "z2.real, (x+3)(x^2+1) = 0");
    gsl_test_rel (GSL_IMAG (z2), 3.0, 1e-9, "z2.imag, (x+3)(x^2+1) = 0");
  }

  {
    gsl_complex z0, z1, z2;

    int n =
      gsl_poly_complex_solve_cubic (-51.0, 867.0, -4913.0, &z0, &z1, &z2);

    gsl_test (n != 3,
              "gsl_poly_complex_solve_cubic, three roots, (x-17)^3=0");
    gsl_test_rel (GSL_REAL (z0), 17.0, 1e-9, "z0.real, (x-17)^3=0");
    gsl_test_rel (GSL_IMAG (z0), 0.0, 1e-9, "z0.imag, (x-17)^3=0");
    gsl_test_rel (GSL_REAL (z1), 17.0, 1e-9, "z1.real, (x-17)^3=0");
    gsl_test_rel (GSL_IMAG (z1), 0.0, 1e-9, "z1.imag, (x-17)^3=0");
    gsl_test_rel (GSL_REAL (z2), 17.0, 1e-9, "z2.real, (x-17)^3=0");
    gsl_test_rel (GSL_IMAG (z2), 0.0, 1e-9, "z2.imag, (x-17)^3=0");
  }

  {
    gsl_complex z0, z1, z2;

    int n =
      gsl_poly_complex_solve_cubic (-57.0, 1071.0, -6647.0, &z0, &z1, &z2);

    gsl_test (n != 3,
              "gsl_poly_complex_solve_cubic, three roots, (x-17)(x-17)(x-23)=0");
    gsl_test_rel (GSL_REAL (z0), 17.0, 1e-9, "z0.real, (x-17)(x-17)(x-23)=0");
    gsl_test_rel (GSL_IMAG (z0), 0.0, 1e-9, "z0.imag, (x-17)(x-17)(x-23)=0");
    gsl_test_rel (GSL_REAL (z1), 17.0, 1e-9, "z1.real, (x-17)(x-17)(x-23)=0");
    gsl_test_rel (GSL_IMAG (z1), 0.0, 1e-9, "z1.imag, (x-17)(x-17)(x-23)=0");
    gsl_test_rel (GSL_REAL (z2), 23.0, 1e-9, "z2.real, (x-17)(x-17)(x-23)=0");
    gsl_test_rel (GSL_IMAG (z2), 0.0, 1e-9, "z2.imag, (x-17)(x-17)(x-23)=0");
  }

  {
    gsl_complex z0, z1, z2;

    int n =
      gsl_poly_complex_solve_cubic (-11.0, -493.0, +6647.0, &z0, &z1, &z2);

    gsl_test (n != 3,
              "gsl_poly_complex_solve_cubic, three roots, (x+23)(x-17)(x-17)=0");
    gsl_test_rel (GSL_REAL (z0), -23.0, 1e-9,
                  "z0.real, (x+23)(x-17)(x-17)=0");
    gsl_test_rel (GSL_IMAG (z0), 0.0, 1e-9, "z0.imag, (x+23)(x-17)(x-17)=0");
    gsl_test_rel (GSL_REAL (z1), 17.0, 1e-9, "z1.real, (x+23)(x-17)(x-17)=0");
    gsl_test_rel (GSL_IMAG (z1), 0.0, 1e-9, "z1.imag, (x+23)(x-17)(x-17)=0");
    gsl_test_rel (GSL_REAL (z2), 17.0, 1e-9, "z2.real, (x+23)(x-17)(x-17)=0");
    gsl_test_rel (GSL_IMAG (z2), 0.0, 1e-9, "z2.imag, (x+23)(x-17)(x-17)=0");
  }


  {
    gsl_complex z0, z1, z2;

    int n =
      gsl_poly_complex_solve_cubic (-143.0, 5087.0, -50065.0, &z0, &z1, &z2);

    gsl_test (n != 3,
              "gsl_poly_complex_solve_cubic, three roots, (x-17)(x-31)(x-95)=0");
    gsl_test_rel (GSL_REAL (z0), 17.0, 1e-9, "z0.real, (x-17)(x-31)(x-95)=0");
    gsl_test_rel (GSL_IMAG (z0), 0.0, 1e-9, "z0.imag, (x-17)(x-31)(x-95)=0");
    gsl_test_rel (GSL_REAL (z1), 31.0, 1e-9, "z1.real, (x-17)(x-31)(x-95)=0");
    gsl_test_rel (GSL_IMAG (z1), 0.0, 1e-9, "z1.imag, (x-17)(x-31)(x-95)=0");
    gsl_test_rel (GSL_REAL (z2), 95.0, 1e-9, "z2.real, (x-17)(x-31)(x-95)=0");
    gsl_test_rel (GSL_IMAG (z2), 0.0, 1e-9, "z2.imag, (x-17)(x-31)(x-95)=0");
  }


  {
    /* Wilkinson polynomial: y = (x-1)(x-2)(x-3)(x-4)(x-5) */

    double a[6] = { -120, 274, -225, 85, -15, 1 };
    double z[6*2];

    gsl_poly_complex_workspace *w = gsl_poly_complex_workspace_alloc (6);

    int status = gsl_poly_complex_solve (a, 6, w, z);

    gsl_poly_complex_workspace_free (w);

    gsl_test (status,
              "gsl_poly_complex_solve, 5th-order Wilkinson polynomial");
    gsl_test_rel (z[0], 1.0, 1e-9, "z0.real, 5th-order polynomial");
    gsl_test_rel (z[1], 0.0, 1e-9, "z0.imag, 5th-order polynomial");
    gsl_test_rel (z[2], 2.0, 1e-9, "z1.real, 5th-order polynomial");
    gsl_test_rel (z[3], 0.0, 1e-9, "z1.imag, 5th-order polynomial");
    gsl_test_rel (z[4], 3.0, 1e-9, "z2.real, 5th-order polynomial");
    gsl_test_rel (z[5], 0.0, 1e-9, "z2.imag, 5th-order polynomial");
    gsl_test_rel (z[6], 4.0, 1e-9, "z3.real, 5th-order polynomial");
    gsl_test_rel (z[7], 0.0, 1e-9, "z3.imag, 5th-order polynomial");
    gsl_test_rel (z[8], 5.0, 1e-9, "z4.real, 5th-order polynomial");
    gsl_test_rel (z[9], 0.0, 1e-9, "z4.imag, 5th-order polynomial");
  }

  {
    /* : 8-th order polynomial y = x^8 + x^4 + 1 */

    double a[9] = { 1, 0, 0, 0, 1, 0, 0, 0, 1 };
    double z[8*2];

    double C = 0.5;
    double S = sqrt (3.0) / 2.0;

    gsl_poly_complex_workspace *w = gsl_poly_complex_workspace_alloc (9);

    int status = gsl_poly_complex_solve (a, 9, w, z);

    gsl_poly_complex_workspace_free (w);

    gsl_test (status, "gsl_poly_complex_solve, 8th-order polynomial");

    gsl_test_rel (z[0], -S, 1e-9, "z0.real, 8th-order polynomial");
    gsl_test_rel (z[1], C, 1e-9, "z0.imag, 8th-order polynomial");
    gsl_test_rel (z[2], -S, 1e-9, "z1.real, 8th-order polynomial");
    gsl_test_rel (z[3], -C, 1e-9, "z1.imag, 8th-order polynomial");
    gsl_test_rel (z[4], -C, 1e-9, "z2.real, 8th-order polynomial");
    gsl_test_rel (z[5], S, 1e-9, "z2.imag, 8th-order polynomial");
    gsl_test_rel (z[6], -C, 1e-9, "z3.real, 8th-order polynomial");
    gsl_test_rel (z[7], -S, 1e-9, "z3.imag, 8th-order polynomial");
    gsl_test_rel (z[8], C, 1e-9, "z4.real, 8th-order polynomial");
    gsl_test_rel (z[9], S, 1e-9, "z4.imag, 8th-order polynomial");
    gsl_test_rel (z[10], C, 1e-9, "z5.real, 8th-order polynomial");
    gsl_test_rel (z[11], -S, 1e-9, "z5.imag, 8th-order polynomial");
    gsl_test_rel (z[12], S, 1e-9, "z6.real, 8th-order polynomial");
    gsl_test_rel (z[13], C, 1e-9, "z6.imag, 8th-order polynomial");
    gsl_test_rel (z[14], S, 1e-9, "z7.real, 8th-order polynomial");
    gsl_test_rel (z[15], -C, 1e-9, "z7.imag, 8th-order polynomial");

  }

  {
    int i;

    double xa[7] = {0.16, 0.97, 1.94, 2.74, 3.58, 3.73, 4.70 };
    double ya[7] = {0.73, 1.11, 1.49, 1.84, 2.30, 2.41, 3.07 };

    double dd_expected[7] = {  7.30000000000000e-01,
                               4.69135802469136e-01,
                              -4.34737219941284e-02,
                               2.68681098870099e-02,
                              -3.22937056934996e-03,
                               6.12763259971375e-03,
                              -6.45402453527083e-03 };

    double dd[7], coeff[7], work[7];
    
    gsl_poly_dd_init (dd, xa, ya, 7);

    for (i = 0; i < 7; i++)
      {
        gsl_test_rel (dd[i], dd_expected[i], 1e-10, "divided difference dd[%d]", i);
      }

    for (i = 0; i < 7; i++)
      {
        double y = gsl_poly_dd_eval(dd, xa, 7, xa[i]);
        gsl_test_rel (y, ya[i], 1e-10, "divided difference y[%d]", i);
      }

    gsl_poly_dd_taylor (coeff, 1.5, dd, xa, 7, work);
    
    for (i = 0; i < 7; i++)
      {
        double y = gsl_poly_eval(coeff, 7, xa[i] - 1.5);
        gsl_test_rel (y, ya[i], 1e-10, "taylor expansion about 1.5 y[%d]", i);
      }
  }


  /* now summarize the results */

  exit (gsl_test_summary ());
}
