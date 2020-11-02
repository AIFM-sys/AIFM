/* matrix/test_complex_source.c
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

void FUNCTION (test, func) (void);
void FUNCTION (test, trap) (void);
void FUNCTION (test, text) (void);
void FUNCTION (test, binary) (void);
void FUNCTION (test, arith) (void);

#define TEST(expr,desc) gsl_test((expr), NAME(gsl_matrix) desc " M=%d, N=%d", M, N)

void
FUNCTION (test, func) (void)
{

  size_t i, j;
  int k = 0;

  TYPE (gsl_matrix) * m = FUNCTION (gsl_matrix, alloc) (M, N);

  gsl_test (m->data == 0, NAME (gsl_matrix) "_alloc returns valid pointer");
  gsl_test (m->size1 != M, NAME (gsl_matrix) "_alloc returns valid size1");
  gsl_test (m->size2 != N, NAME (gsl_matrix) "_alloc returns valid size2");
  gsl_test (m->tda != N, NAME (gsl_matrix) "_alloc returns valid tda");

  for (i = 0; i < M; i++)
    {
      for (j = 0; j < N; j++)
        {
          BASE z = ZERO;
          k++;
          GSL_REAL (z) = (ATOMIC) k;
          GSL_IMAG (z) = (ATOMIC) (k + 1000);
          FUNCTION (gsl_matrix, set) (m, i, j, z);
        }
    }

  status = 0;
  k = 0;
  for (i = 0; i < M; i++)
    {
      for (j = 0; j < N; j++)
        {
          k++;
          if (m->data[2 * (i * N + j)] != k ||
              m->data[2 * (i * N + j) + 1] != k + 1000)
            status = 1;
        }
    }

  gsl_test (status, NAME (gsl_matrix) "_set writes into array");

  status = 0;
  k = 0;
  for (i = 0; i < M; i++)
    {
      for (j = 0; j < N; j++)
        {
          BASE z = FUNCTION (gsl_matrix, get) (m, i, j);
          k++;
          if (GSL_REAL (z) != k || GSL_IMAG (z) != k + 1000)
            status = 1;
        }
    }
  gsl_test (status, NAME (gsl_matrix) "_get reads from array");

  FUNCTION (gsl_matrix, free) (m);      /* free whatever is in m */

  m = FUNCTION (gsl_matrix, calloc) (M, N);

  {
    int status = (FUNCTION(gsl_matrix,isnull)(m) != 1);
    TEST (status, "_isnull" DESC " on calloc matrix");
    
    status = (FUNCTION(gsl_matrix,ispos)(m) != 0);
    TEST (status, "_ispos" DESC " on calloc matrix");
    
    status = (FUNCTION(gsl_matrix,isneg)(m) != 0);
    TEST (status, "_isneg" DESC " on calloc matrix");
  }

  for (i = 0; i < M; i++)
    {
      for (j = 0; j < N; j++)
        {
          BASE z = ZERO;
          FUNCTION (gsl_matrix, set) (m, i, j, z);
        }
    }

  {
    status = (FUNCTION(gsl_matrix,isnull)(m) != 1);
    TEST (status, "_isnull" DESC " on null matrix") ;

    status = (FUNCTION(gsl_matrix,ispos)(m) != 0);
    TEST (status, "_ispos" DESC " on null matrix") ;

    status = (FUNCTION(gsl_matrix,isneg)(m) != 0);
    TEST (status, "_isneg" DESC " on null matrix") ;
  }


  k = 0;
  for (i = 0; i < M; i++)
    {
      for (j = 0; j < N; j++)
        {
          BASE z = ZERO;
          k++;
          GSL_REAL (z) = (ATOMIC) (k % 10);
          GSL_IMAG (z) = (ATOMIC) ((k + 5) % 10);
          FUNCTION (gsl_matrix, set) (m, i, j, z);
        }
    }

  {
    status = (FUNCTION(gsl_matrix,isnull)(m) != 0);
    TEST (status, "_isnull" DESC " on non-negative matrix") ;

    status = (FUNCTION(gsl_matrix,ispos)(m) != 0);
    TEST (status, "_ispos" DESC " on non-negative matrix") ;

    status = (FUNCTION(gsl_matrix,isneg)(m) != 0);
    TEST (status, "_isneg" DESC " on non-negative matrix") ;
  }

  k = 0;
  for (i = 0; i < M; i++)
    {
      for (j = 0; j < N; j++)
        {
          BASE z = ZERO;
          k++;
          GSL_REAL (z) = (ATOMIC) ((k % 10) - 5);
          GSL_IMAG (z) = (ATOMIC) (((k + 5) % 10) - 5);
          FUNCTION (gsl_matrix, set) (m, i, j, z);
        }
    }

  {
    status = (FUNCTION(gsl_matrix,isnull)(m) != 0);
    TEST (status, "_isnull" DESC " on mixed matrix") ;

    status = (FUNCTION(gsl_matrix,ispos)(m) != 0);
    TEST (status, "_ispos" DESC " on mixed matrix") ;

    status = (FUNCTION(gsl_matrix,isneg)(m) != 0);
    TEST (status, "_isneg" DESC " on mixed matrix") ;
  }

  k = 0;
  for (i = 0; i < M; i++)
    {
      for (j = 0; j < N; j++)
        {
          BASE z = ZERO;
          k++;
          GSL_REAL (z) = -(ATOMIC) (k % 10);
          GSL_IMAG (z) = -(ATOMIC) ((k + 5) % 10);
          FUNCTION (gsl_matrix, set) (m, i, j, z);
        }
    }

  {
    status = (FUNCTION(gsl_matrix,isnull)(m) != 0);
    TEST (status, "_isnull" DESC " on non-positive matrix") ;

    status = (FUNCTION(gsl_matrix,ispos)(m) != 0);
    TEST (status, "_ispos" DESC " on non-positive matrix") ;

    status = (FUNCTION(gsl_matrix,isneg)(m) != 0);
    TEST (status, "_isneg" DESC " on non-positive matrix") ;
  }

  k = 0;
  for (i = 0; i < M; i++)
    {
      for (j = 0; j < N; j++)
        {
          BASE z = ZERO;
          k++;
          GSL_REAL (z) = (ATOMIC) (k % 10 + 1);
          GSL_IMAG (z) = (ATOMIC) ((k + 5) % 10 + 1);
          FUNCTION (gsl_matrix, set) (m, i, j, z);
        }
    }

  {
    status = (FUNCTION(gsl_matrix,isnull)(m) != 0);
    TEST (status, "_isnull" DESC " on positive matrix") ;

    status = (FUNCTION(gsl_matrix,ispos)(m) != 1);
    TEST (status, "_ispos" DESC " on positive matrix") ;

    status = (FUNCTION(gsl_matrix,isneg)(m) != 0);
    TEST (status, "_isneg" DESC " on positive matrix") ;
  }

  k = 0;
  for (i = 0; i < M; i++)
    {
      for (j = 0; j < N; j++)
        {
          BASE z = ZERO;
          k++;
          GSL_REAL (z) = -(ATOMIC) (k % 10 + 1);
          GSL_IMAG (z) = -(ATOMIC) ((k + 5) % 10 + 1);
          FUNCTION (gsl_matrix, set) (m, i, j, z);
        }
    }

  {
    status = (FUNCTION(gsl_matrix,isnull)(m) != 0);
    TEST (status, "_isnull" DESC " on negative matrix") ;

    status = (FUNCTION(gsl_matrix,ispos)(m) != 0);
    TEST (status, "_ispos" DESC " on negative matrix") ;

    status = (FUNCTION(gsl_matrix,isneg)(m) != 1);
    TEST (status, "_isneg" DESC " on negative matrix") ;
  }

  FUNCTION (gsl_matrix, free) (m);      /* free whatever is in m */
}

#if !(USES_LONGDOUBLE && !HAVE_PRINTF_LONGDOUBLE)
void
FUNCTION (test, text) (void)
{
  TYPE (gsl_matrix) * m = FUNCTION (gsl_matrix, alloc) (M, N);

  size_t i, j;
  int k = 0;

  {
    FILE *f = fopen ("test.txt", "w");
    k = 0;
    for (i = 0; i < M; i++)
      {
        for (j = 0; j < N; j++)
          {
            BASE z;
            k++;
            GSL_REAL (z) = (ATOMIC) k;
            GSL_IMAG (z) = (ATOMIC) (k + 1000);
            FUNCTION (gsl_matrix, set) (m, i, j, z);
          }
      }

    FUNCTION (gsl_matrix, fprintf) (f, m, OUT_FORMAT);

    fclose (f);
  }

  {
    FILE *f = fopen ("test.txt", "r");
    TYPE (gsl_matrix) * mm = FUNCTION (gsl_matrix, alloc) (M, N);
    status = 0;

    FUNCTION (gsl_matrix, fscanf) (f, mm);
    k = 0;
    for (i = 0; i < M; i++)
      {
        for (j = 0; j < N; j++)
          {
            k++;
            if (mm->data[2 * (i * N + j)] != k
                || mm->data[2 * (i * N + j) + 1] != k + 1000)
              status = 1;
          }
      }

    gsl_test (status, NAME (gsl_matrix) "_fprintf and fscanf");

    fclose (f);
    FUNCTION (gsl_matrix, free) (mm);
  }

  FUNCTION (gsl_matrix, free) (m);
}
#endif

void
FUNCTION (test, binary) (void)
{
  TYPE (gsl_matrix) * m = FUNCTION (gsl_matrix, alloc) (M, N);

  size_t i, j;
  int k = 0;

  {
    FILE *f = fopen ("test.dat", "wb");
    k = 0;
    for (i = 0; i < M; i++)
      {
        for (j = 0; j < N; j++)
          {
            BASE z = ZERO;
            k++;
            GSL_REAL (z) = (ATOMIC) k;
            GSL_IMAG (z) = (ATOMIC) (k + 1000);
            FUNCTION (gsl_matrix, set) (m, i, j, z);
          }
      }

    FUNCTION (gsl_matrix, fwrite) (f, m);

    fclose (f);
  }

  {
    FILE *f = fopen ("test.dat", "rb");
    TYPE (gsl_matrix) * mm = FUNCTION (gsl_matrix, alloc) (M, N);
    status = 0;

    FUNCTION (gsl_matrix, fread) (f, mm);
    k = 0;
    for (i = 0; i < M; i++)
      {
        for (j = 0; j < N; j++)
          {
            k++;
            if (mm->data[2 * (i * N + j)] != k
                || mm->data[2 * (i * N + j) + 1] != k + 1000)
              status = 1;
          }
      }

    gsl_test (status, NAME (gsl_matrix) "_write and read");

    fclose (f);
    FUNCTION (gsl_matrix, free) (mm);
  }

  FUNCTION (gsl_matrix, free) (m);
}

void
FUNCTION (test, trap) (void)
{
  TYPE (gsl_matrix) * mc = FUNCTION (gsl_matrix, alloc) (M, N);
  size_t i = 0, j = 0;

  BASE z = { {(ATOMIC) 1.2, (ATOMIC) 3.4} };
  BASE z1;

  status = 0;
  FUNCTION (gsl_matrix, set) (mc, i - 1, j, z);
  gsl_test (!status,
            NAME (gsl_matrix) "_set traps 1st index below lower bound");

  status = 0;
  FUNCTION (gsl_matrix, set) (mc, i, j - 1, z);
  gsl_test (!status,
            NAME (gsl_matrix) "_set traps 2nd index below lower bound");

  status = 0;
  FUNCTION (gsl_matrix, set) (mc, M + 1, 0, z);
  gsl_test (!status,
            NAME (gsl_matrix) "_set traps 1st index above upper bound");

  status = 0;
  FUNCTION (gsl_matrix, set) (mc, 0, N + 1, z);
  gsl_test (!status,
            NAME (gsl_matrix) "_set traps 2nd index above upper bound");

  status = 0;
  FUNCTION (gsl_matrix, set) (mc, M, 0, z);
  gsl_test (!status, NAME (gsl_matrix) "_set traps 1st index at upper bound");

  status = 0;
  FUNCTION (gsl_matrix, set) (mc, 0, N, z);
  gsl_test (!status, NAME (gsl_matrix) "_set traps 2nd index at upper bound");

  status = 0;
  z1 = FUNCTION (gsl_matrix, get) (mc, i - 1, 0);
  gsl_test (!status,
            NAME (gsl_matrix) "_get traps 1st index below lower bound");
  gsl_test (GSL_REAL (z1) != 0,
            NAME (gsl_matrix) "_get, zero real for 1st index below l.b.");
  gsl_test (GSL_IMAG (z1) != 0,
            NAME (gsl_matrix) "_get, zero imag for 1st index below l.b.");

  status = 0;
  z1 = FUNCTION (gsl_matrix, get) (mc, 0, j - 1);
  gsl_test (!status,
            NAME (gsl_matrix) "_get traps 2nd index below lower bound");
  gsl_test (GSL_REAL (z1) != 0,
            NAME (gsl_matrix) "_get, zero real for 2nd index below l.b.");
  gsl_test (GSL_IMAG (z1) != 0,
            NAME (gsl_matrix) "_get, zero imag for 2nd index below l.b.");

  status = 0;
  z1 = FUNCTION (gsl_matrix, get) (mc, M + 1, 0);
  gsl_test (!status,
            NAME (gsl_matrix) "_get traps 1st index above upper bound");
  gsl_test (GSL_REAL (z1) != 0,
            NAME (gsl_matrix) "_get, zero real for 1st index above u.b.");
  gsl_test (GSL_IMAG (z1) != 0,
            NAME (gsl_matrix) "_get, zero imag for 1st index above u.b.");

  status = 0;
  z1 = FUNCTION (gsl_matrix, get) (mc, 0, N + 1);
  gsl_test (!status,
            NAME (gsl_matrix) "_get traps 2nd index above upper bound");
  gsl_test (GSL_REAL (z1) != 0,
            NAME (gsl_matrix) "_get, zero real for 2nd index above u.b.");
  gsl_test (GSL_IMAG (z1) != 0,
            NAME (gsl_matrix) "_get, zero imag for 2nd index above u.b.");

  status = 0;
  z1 = FUNCTION (gsl_matrix, get) (mc, M, 0);
  gsl_test (!status, NAME (gsl_matrix) "_get traps 1st index at upper bound");
  gsl_test (GSL_REAL (z1) != 0,
            NAME (gsl_matrix) "_get, zero real for 1st index at u.b.");
  gsl_test (GSL_IMAG (z1) != 0,
            NAME (gsl_matrix) "_get, zero imag for 1st index at u.b.");

  status = 0;
  z1 = FUNCTION (gsl_matrix, get) (mc, 0, N);
  gsl_test (!status, NAME (gsl_matrix) "_get traps 2nd index at upper bound");
  gsl_test (GSL_REAL (z1) != 0,
            NAME (gsl_matrix) "_get, zero real for 2nd index at u.b.");
  gsl_test (GSL_IMAG (z1) != 0,
            NAME (gsl_matrix) "_get, zero imag for 2nd index at u.b.");

  FUNCTION (gsl_matrix, free) (mc);
}


void
FUNCTION (test, arith) (void)
{

#define P 8
#define Q 12
/* Must use smaller dimensions to prevent approximation of floats in float_mul_elements test*/

  TYPE (gsl_matrix) * a = FUNCTION (gsl_matrix, alloc) (P, Q);
  TYPE (gsl_matrix) * b = FUNCTION (gsl_matrix, alloc) (P, Q);
  TYPE (gsl_matrix) * m = FUNCTION (gsl_matrix, alloc) (P, Q);
  size_t i, j;
  size_t k = 0;

  size_t status = 0;

  for (i = 0; i < P; i++)
    {
      for (j = 0; j < Q; j++)
        {
          BASE z, z1;
          GSL_REAL (z) = (ATOMIC) k;
          GSL_IMAG (z) = (ATOMIC) (k + 10);
          GSL_REAL (z1) = (ATOMIC) (k + 5);
          GSL_IMAG (z1) = (ATOMIC) (k + 20);

          FUNCTION (gsl_matrix, set) (a, i, j, z);
          FUNCTION (gsl_matrix, set) (b, i, j, z1);
          k++;
        }
    }

  {
    FUNCTION (gsl_matrix, memcpy) (m, a);

    FUNCTION (gsl_matrix, add) (m, b);

    k = 0;
    status = 0;

    for (i = 0; i < P; i++)
      {
        for (j = 0; j < Q; j++)
          {
            BASE z = FUNCTION (gsl_matrix, get) (m, i, j);
            if (GSL_REAL (z) != (ATOMIC) (2 * k + 5) ||
                GSL_IMAG (z) != (ATOMIC) (2 * k + 30))
              status = 1;
            k++;
          }
      }
    gsl_test (status, NAME (gsl_matrix) "_add matrix addition");
  }

  {
    FUNCTION (gsl_matrix, memcpy) (m, a);

    FUNCTION (gsl_matrix, sub) (m, b);

    k = 0;
    status = 0;

    for (i = 0; i < P; i++)
      {
        for (j = 0; j < Q; j++)
          {
            BASE z = FUNCTION (gsl_matrix, get) (m, i, j);
            if (GSL_REAL (z) != (ATOMIC) (-5)
                || GSL_IMAG (z) != (ATOMIC) (-10))
              status = 1;
            k++;
          }
      }
    gsl_test (status, NAME (gsl_matrix) "_sub matrix subtraction");
  }

  {
    FUNCTION (gsl_matrix, memcpy) (m, a);

    FUNCTION (gsl_matrix, mul_elements) (m, b);

    k = 0;
    status = 0;

    for (i = 0; i < P; i++)
      {
        for (j = 0; j < Q; j++)
          {
            ATOMIC real = -(ATOMIC) (25 * k + 200);
            ATOMIC imag = (ATOMIC) (2 * k * k + 35 * k + 50);
            BASE z = FUNCTION (gsl_matrix, get) (m, i, j);
            if (fabs (GSL_REAL (z) - real) > 100 * BASE_EPSILON ||
                fabs (GSL_IMAG (z) - imag) > 100 * BASE_EPSILON)
              {
                status = 1;
#ifdef DEBUG
                printf ("%d\t%d\n", i, j);
                printf (OUT_FORMAT "\n",
                        GSL_REAL (z) + (ATOMIC) (25 * (ATOMIC) k + 200));
                printf (OUT_FORMAT "\n",
                        GSL_IMAG (z) - (ATOMIC) (2 * k * k + 35 * k + 50));
                printf ("\n");
#endif
              }
            k++;
          }
      }
    gsl_test (status, NAME (gsl_matrix) "_mul_elements multiplication");
  }


  {
    FUNCTION (gsl_matrix, memcpy) (m, a);

    FUNCTION (gsl_matrix, div_elements) (m, b);

    k = 0;
    status = 0;

    for (i = 0; i < P; i++)
      {
        for (j = 0; j < Q; j++)
          {
            ATOMIC denom = (2 * k * k + 50 * k + 425);
            ATOMIC real = (ATOMIC) (2 * k * k + 35 * k + 200) / denom;
            ATOMIC imag = ((ATOMIC) (50) - (ATOMIC) (5 * k)) / denom;
            BASE z = FUNCTION (gsl_matrix, get) (m, i, j);
            if (fabs (GSL_REAL (z) - real) > 100 * BASE_EPSILON ||
                fabs (GSL_IMAG (z) - imag) > 100 * BASE_EPSILON)
              {
#ifdef DEBUG
                printf (OUT_FORMAT "\t",
                        GSL_REAL (z) - (ATOMIC) (2 * k * k + 35 * k +
                                                 200) / denom);
                printf (OUT_FORMAT "\n",
                        GSL_IMAG (z) - ((ATOMIC) (50) -
                                        (ATOMIC) (5 * k)) / denom);
#endif
                status = 1;
              }
            k++;
          }
      }
    gsl_test (status, NAME (gsl_matrix) "_div_elements division");
  }

  FUNCTION (gsl_matrix, free) (a);
  FUNCTION (gsl_matrix, free) (b);
  FUNCTION (gsl_matrix, free) (m);

}
