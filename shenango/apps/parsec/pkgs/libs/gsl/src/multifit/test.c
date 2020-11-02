/* These tests are based on the NIST Statistical Reference Datasets
   See http://www.nist.gov/itl/div898/strd/index.html for more
   information. */

#include <config.h>
#include <stdlib.h>
#include <gsl/gsl_math.h>
#include <gsl/gsl_test.h>
#include <gsl/gsl_multifit.h>
#include <gsl/gsl_multifit_nlin.h>
#include <gsl/gsl_blas.h>

#include <gsl/gsl_ieee_utils.h>

#include "test_longley.c"
#include "test_filip.c"
#include "test_pontius.c"
#include "test_brown.c"
#include "test_enso.c"
#include "test_kirby2.c"
#include "test_hahn1.c"
#include "test_nelson.c"
#include "test_fn.c"
#include "test_estimator.c"

void
test_lmder (gsl_multifit_function_fdf * f, double x0[], 
            double * X, double F[], double * cov);

void
test_fdf (const char * name, gsl_multifit_function_fdf * f, 
          double x0[], double x[], double sumsq,
          double sigma[]);

int
main (void)
{
  gsl_ieee_env_setup();

  test_longley();
  test_filip();
  test_pontius();
  test_estimator();

  {
    gsl_multifit_function_fdf f = make_fdf (&brown_f, &brown_df, &brown_fdf,
                                            brown_N, brown_P, 0);
    
    test_lmder(&f, brown_x0, &brown_X[0][0], brown_F, &brown_cov[0][0]);
  }

  {
    gsl_multifit_function_fdf f = make_fdf (&enso_f, &enso_df, &enso_fdf,
                                            enso_N, enso_P, 0);

    test_fdf("nist-ENSO", &f, enso_x0, enso_x, enso_sumsq, enso_sigma);
  }

  {
    gsl_multifit_function_fdf f = make_fdf (&kirby2_f, &kirby2_df, &kirby2_fdf,
                                            kirby2_N, kirby2_P, 0);

    test_fdf("nist-kirby2", &f, kirby2_x0, kirby2_x, kirby2_sumsq, kirby2_sigma);
  }

  {
    gsl_multifit_function_fdf f = make_fdf (&hahn1_f, &hahn1_df, &hahn1_fdf,
                                            hahn1_N, hahn1_P, 0);

    test_fdf("nist-hahn1", &f, hahn1_x0, hahn1_x, hahn1_sumsq, hahn1_sigma);
  }

#ifdef JUNK
  {
    gsl_multifit_function_fdf f = make_fdf (&nelson_f, &nelson_df, &nelson_fdf,
                                            nelson_N, nelson_P, 0);

    test_fdf("nist-nelson", &f, nelson_x0, nelson_x, nelson_sumsq, nelson_sigma);
  }
#endif

  /* now summarize the results */

  exit (gsl_test_summary ());
}


void
test_lmder (gsl_multifit_function_fdf * f, double x0[], 
            double * X, double F[], double * cov)
{
  const gsl_multifit_fdfsolver_type *T;
  gsl_multifit_fdfsolver *s;

  const size_t n = f->n;
  const size_t p = f->p;

  int status;
  size_t iter = 0, i;
  
  gsl_vector_view x = gsl_vector_view_array (x0, p);

  T = gsl_multifit_fdfsolver_lmsder;
  s = gsl_multifit_fdfsolver_alloc (T, n, p);
  gsl_multifit_fdfsolver_set (s, f, &x.vector);

  do
    {
      status = gsl_multifit_fdfsolver_iterate (s);

      for (i = 0 ; i < p; i++)
        {
          gsl_test_rel (gsl_vector_get (s->x, i), X[p*iter+i], 1e-5, 
                        "lmsder, iter=%u, x%u", iter, i);
        }

      gsl_test_rel (gsl_blas_dnrm2 (s->f), F[iter], 1e-5, 
                    "lmsder, iter=%u, f", iter);

      iter++;
    }
  while (iter < 20);
  
  {
    size_t i, j;
    gsl_matrix * covar = gsl_matrix_alloc (4, 4);
    gsl_multifit_covar (s->J, 0.0, covar);

    for (i = 0; i < 4; i++) 
      {
        for (j = 0; j < 4; j++)
          {
            gsl_test_rel (gsl_matrix_get(covar,i,j), cov[i*p + j], 1e-7, 
                          "gsl_multifit_covar cov(%d,%d)", i, j) ;
          }
      }

    gsl_matrix_free (covar);
  }

  gsl_multifit_fdfsolver_free (s);

}

void
test_fdf (const char * name, gsl_multifit_function_fdf * f, 
          double x0[], double x_final[], 
          double f_sumsq, double sigma[])
{
  const gsl_multifit_fdfsolver_type *T;
  gsl_multifit_fdfsolver *s;

  const size_t n = f->n;
  const size_t p = f->p;

  int status;
  size_t iter = 0;

  gsl_vector_view x = gsl_vector_view_array (x0, p);

  T = gsl_multifit_fdfsolver_lmsder;
  s = gsl_multifit_fdfsolver_alloc (T, n, p);
  gsl_multifit_fdfsolver_set (s, f, &x.vector);

  do
    {
      status = gsl_multifit_fdfsolver_iterate (s);

#ifdef DEBUG
       printf("iter = %d  status = %d  |f| = %.18e x = \n", 
         iter, status, gsl_blas_dnrm2 (s->f));
         
         gsl_vector_fprintf(stdout, s->x, "%.8e");
#endif       
      status = gsl_multifit_test_delta (s->dx, s->x, 0.0, 1e-7);

      iter++;
    }
  while (status == GSL_CONTINUE && iter < 1000);
  
  {
    size_t i;
    gsl_matrix * covar = gsl_matrix_alloc (p, p);
    gsl_multifit_covar (s->J, 0.0, covar);

    for (i = 0 ; i < p; i++)
      {
        gsl_test_rel (gsl_vector_get (s->x, i), x_final[i], 1e-5, 
                      "%s, lmsder, x%u", name, i);
      }


    {
      double s2 = pow(gsl_blas_dnrm2 (s->f), 2.0);

      gsl_test_rel (s2, f_sumsq, 1e-5, "%s, lmsder, |f|^2", name);

      for (i = 0; i < p; i++) 
        {
          double ei = sqrt(s2/(n-p))*sqrt(gsl_matrix_get(covar,i,i));
          gsl_test_rel (ei, sigma[i], 1e-4, 
                        "%s, sigma(%d)", name, i) ;
        }
    }

    gsl_matrix_free (covar);
  }

  gsl_multifit_fdfsolver_free (s);
}
