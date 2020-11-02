/* cheb/test.c
 * 
 * Copyright (C) 1996, 1997, 1998, 1999, 2000 Gerard Jungman
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
#include <gsl/gsl_errno.h>
#include <gsl/gsl_test.h>
#include <gsl/gsl_ieee_utils.h>
#include <gsl/gsl_chebyshev.h>

double f_T0 (double x, void * p) {
  p = 0;
  return 1.0;
}

double f_T1 (double x, void * p) {
  p = 0;
  return x;
}

double f_T2 (double x, void * p) {
  p = 0;
  return 2*x*x - 1;
}

double f_sin (double x, void * p) {
  p = 0;
  return sin(x);
}

int 
main(void)
{
  double tol = 100.0 * GSL_DBL_EPSILON;
  double ftol = 20.0;
  double x; 
  size_t i;

  gsl_cheb_series * cs = gsl_cheb_alloc(40);
  gsl_cheb_series * csd = gsl_cheb_alloc(40);
  gsl_cheb_series * csi = gsl_cheb_alloc(40);

  gsl_function F_sin, F_T0, F_T1, F_T2;

  F_sin.function = f_sin;
  F_sin.params = 0;

  F_T0.function = f_T0;
  F_T0.params = 0;

  F_T1.function = f_T1;
  F_T1.params = 0;

  F_T2.function = f_T2;
  F_T2.params = 0;

  gsl_ieee_env_setup();

  gsl_cheb_init(cs, &F_T0, -1.0, 1.0);

  for (i = 0; i<cs->order; i++)
    {
      double c_exp = (i == 0) ? 2.0 : 0.0;
      gsl_test_abs (cs->c[i], c_exp, tol, "c[%d] for T_0(x)", i);
    }

  gsl_cheb_init(cs, &F_T1, -1.0, 1.0);

  for (i = 0; i<cs->order; i++)
    {
      double c_exp = (i == 1) ? 1.0 : 0.0;
      gsl_test_abs (cs->c[i], c_exp, tol, "c[%d] for T_1(x)", i);
    }

  gsl_cheb_init(cs, &F_T2, -1.0, 1.0);

  for (i = 0; i<cs->order; i++)
    {
      double c_exp = (i == 2) ? 1.0 : 0.0;
      gsl_test_abs (cs->c[i], c_exp, tol, "c[%d] for T_2(x)", i);
    }

  gsl_cheb_init(cs, &F_sin, -M_PI, M_PI);

  gsl_test_abs (cs->c[0], 0.0, tol, "c[0] for F_sin(x)");
  gsl_test_abs (cs->c[1], 5.69230686359506e-01, tol, "c[1] for F_sin(x)");
  gsl_test_abs (cs->c[2], 0.0, tol, "c[2] for F_sin(x)");
  gsl_test_abs (cs->c[3], -6.66916672405979e-01, tol, "c[3] for F_sin(x)");
  gsl_test_abs (cs->c[4], 0.0, tol, "c[4] for F_sin(x)");
  gsl_test_abs (cs->c[5], 1.04282368734237e-01, tol, "c[5] for F_sin(x)");

  for(x=-M_PI; x<M_PI; x += M_PI/100.0) {
    double r = gsl_cheb_eval(cs, x);
    gsl_test_abs(r, sin(x), tol, "gsl_cheb_eval, sin(%.3g)", x);
  }
  
  for(x=-M_PI; x<M_PI; x += M_PI/100.0) {
    double r, e;
    gsl_cheb_eval_err(cs, x, &r, &e);
    gsl_test_abs(r, sin(x), tol, "gsl_cheb_eval_err, sin(%.3g)", x);
    gsl_test_factor(fabs(r-sin(x)) + GSL_DBL_EPSILON, e, ftol, 
                    "gsl_cheb_eval_err, error sin(%.3g)", x);
  }

  for(x=-M_PI; x<M_PI; x += M_PI/100.0) {
    double r = gsl_cheb_eval_n(cs, 25, x);
    gsl_test_abs(r, sin(x), tol, "gsl_cheb_eval_n, sin(%.3g)", x);
  }

  for(x=-M_PI; x<M_PI; x += M_PI/100.0) {
    double r, e;
    gsl_cheb_eval_n_err(cs, 25, x, &r, &e);
    gsl_test_abs(r, sin(x), 100.0 * tol, "gsl_cheb_eval_n_err, deriv sin(%.3g)", x);
    gsl_test_factor(fabs(r-sin(x)) + GSL_DBL_EPSILON, e, ftol, 
                    "gsl_cheb_eval_n_err, error sin(%.3g)", x);
  }

  /* Test derivative */

  gsl_cheb_calc_deriv(csd, cs);

  for(x=-M_PI; x<M_PI; x += M_PI/100.0) {
    double r = gsl_cheb_eval(csd, x);
    gsl_test_abs(r, cos(x), 1600 * tol, "gsl_cheb_eval, deriv sin(%.3g)", x);
  }
  
#ifdef TEST_DERIVATIVE_ERR
  for(x=-M_PI; x<M_PI; x += M_PI/100.0) {
    double r, e;
    gsl_cheb_eval_err(csd, x, &r, &e);
    gsl_test_abs(r, cos(x), tol, "gsl_cheb_eval_err, deriv sin(%.3g)", x);
    gsl_test_factor(fabs(r-cos(x)) + GSL_DBL_EPSILON, e, ftol, 
                    "gsl_cheb_eval_err, deriv error sin(%.3g)", x);
  }
#endif

  for(x=-M_PI; x<M_PI; x += M_PI/100.0) {
    double r = gsl_cheb_eval_n(csd, 25, x);
    gsl_test_abs(r, cos(x), 1600 * tol, "gsl_cheb_eval_n, deriv sin(%.3g)", x);
  }

#ifdef TEST_DERIVATIVE_ERR
  for(x=-M_PI; x<M_PI; x += M_PI/100.0) {
    double r, e;
    gsl_cheb_eval_n_err(csd, 25, x, &r, &e);
    gsl_test_abs(r, cos(x), 100.0 * tol, "gsl_cheb_eval_n_err, deriv sin(%.3g)", x);
    gsl_test_factor(fabs(r-cos(x)) + GSL_DBL_EPSILON, e, ftol, 
                    "gsl_cheb_eval_n_err, deriv error sin(%.3g)", x);
  }
#endif

  /* Test integral */

  gsl_cheb_calc_integ(csi, cs);

  for(x=-M_PI; x<M_PI; x += M_PI/100.0) {
    double r = gsl_cheb_eval(csi, x);
    gsl_test_abs(r, -(1+cos(x)), tol, "gsl_cheb_eval, integ sin(%.3g)", x);
  }
  
#ifdef TEST_INTEGRAL_ERR
  for(x=-M_PI; x<M_PI; x += M_PI/100.0) {
    double r, e;
    gsl_cheb_eval_err(csi, x, &r, &e);
    gsl_test_abs(r, -(1+cos(x)), tol, "gsl_cheb_eval_err, integ sin(%.3g)", x);
    gsl_test_factor(fabs(r-(-1-cos(x))) + GSL_DBL_EPSILON, e, ftol, 
                    "gsl_cheb_eval_err, integ error sin(%.3g)", x);
  }
#endif

  for(x=-M_PI; x<M_PI; x += M_PI/100.0) {
    double r = gsl_cheb_eval_n(csi, 25, x);
    gsl_test_abs(r, -(1+cos(x)), tol, "gsl_cheb_eval_n, integ sin(%.3g)", x);
  }

#ifdef TEST_INTEGRAL_ERR
  for(x=-M_PI; x<M_PI; x += M_PI/100.0) {
    double r, e;
    gsl_cheb_eval_n_err(csi, 25, x, &r, &e);
    gsl_test_abs(r, -(1+cos(x)), 100.0 * tol, "gsl_cheb_eval_n_err, integ sin(%.3g)", x);
    gsl_test_factor(fabs(r-(-1-cos(x))) + GSL_DBL_EPSILON, e, ftol, 
                    "gsl_cheb_eval_n_err, integ error sin(%.3g)", x);
  }
#endif

  gsl_cheb_free(csi);
  gsl_cheb_free(csd);
  gsl_cheb_free(cs);

  exit (gsl_test_summary());
}
