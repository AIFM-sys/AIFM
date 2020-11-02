/*************************************************************************/
/*                                                                       */
/*  Copyright (c) 1994 Stanford University                               */
/*                                                                       */
/*  All rights reserved.                                                 */
/*                                                                       */
/*  Permission is given to use, copy, and modify this software for any   */
/*  non-commercial purpose as long as this copyright notice is not       */
/*  removed.  All other uses, including redistribution in whole or in    */
/*  part, are forbidden without prior written permission.                */
/*                                                                       */
/*  This software is provided with absolutely no warranty and no         */
/*  support.                                                             */
/*                                                                       */
/*************************************************************************/

/*
 * CODE_IO.C:
 */
EXTERN_ENV
#define global extern

#include "stdinc.h"

/*
 * INPUTDATA: read initial conditions from input file.
 */

void inputdata ()
{
   stream instr;
   permanent char headbuf[128];
   long ndim;
   real tnow;
   bodyptr p;
   long i;

   fprintf(stderr,"reading input file : %s\n",infile);
   fflush(stderr);
   instr = fopen(infile, "r");
   if (instr == NULL)
      error("inputdata: cannot find file %s\n", infile);
   sprintf(headbuf, "Hack code: input file %s\n", infile);
   headline = headbuf;
   in_int(instr, &nbody);
   if (nbody < 1)
      error("inputdata: nbody = %ld is absurd\n", nbody);
   in_int(instr, &ndim);
   if (ndim != NDIM)
      error("inputdata: NDIM = %ld ndim = %ld is absurd\n", NDIM, ndim);
   in_real(instr, &tnow);
   for (i = 0; i < MAX_PROC; i++) {
      Local[i].tnow = tnow;
   }
   bodytab = (bodyptr) G_MALLOC(nbody * sizeof(body));
   if (bodytab == NULL)
      error("inputdata: not enuf memory\n");
   for (p = bodytab; p < bodytab+nbody; p++) {
      Type(p) = BODY;
      Cost(p) = 1;
      Phi(p) = 0.0;
      CLRV(Acc(p));
   }
   for (p = bodytab; p < bodytab+nbody; p++)
      in_real(instr, &Mass(p));
   for (p = bodytab; p < bodytab+nbody; p++)
      in_vector(instr, Pos(p));
   for (p = bodytab; p < bodytab+nbody; p++)
      in_vector(instr, Vel(p));
   fclose(instr);
}

/*
 * INITOUTPUT: initialize output routines.
 */


void initoutput()
{
   printf("\n\t\t%s\n\n", headline);
   printf("%10s%10s%10s%10s%10s%10s%10s%10s\n",
	  "nbody", "dtime", "eps", "tol", "dtout", "tstop","fcells","NPROC");
   printf("%10ld%10.5f%10.4f%10.2f%10.3f%10.3f%10.2f%10ld\n\n",
	  nbody, dtime, eps, tol, dtout, tstop, fcells, NPROC);
}

/*
 * STOPOUTPUT: finish up after a run.
 */


/*
 * OUTPUT: compute diagnostics and output data.
 */

void output(long ProcessId)
{
   long nttot, nbavg, ncavg,k;
   vector tempv1,tempv2;

   if ((Local[ProcessId].tout - 0.01 * dtime) <= Local[ProcessId].tnow) {
      Local[ProcessId].tout += dtout;
   }

   diagnostics(ProcessId);

   if (Local[ProcessId].mymtot!=0) {
      LOCK(Global->CountLock);
      Global->n2bcalc += Local[ProcessId].myn2bcalc;
      Global->nbccalc += Local[ProcessId].mynbccalc;
      Global->selfint += Local[ProcessId].myselfint;
      ADDM(Global->keten, Global-> keten, Local[ProcessId].myketen);
      ADDM(Global->peten, Global-> peten, Local[ProcessId].mypeten);
      for (k=0;k<3;k++) Global->etot[k] +=  Local[ProcessId].myetot[k];
      ADDV(Global->amvec, Global-> amvec, Local[ProcessId].myamvec);

      MULVS(tempv1, Global->cmphase[0],Global->mtot);
      MULVS(tempv2, Local[ProcessId].mycmphase[0], Local[ProcessId].mymtot);
      ADDV(tempv1, tempv1, tempv2);
      DIVVS(Global->cmphase[0], tempv1, Global->mtot+Local[ProcessId].mymtot);

      MULVS(tempv1, Global->cmphase[1],Global->mtot);
      MULVS(tempv2, Local[ProcessId].mycmphase[1], Local[ProcessId].mymtot);
      ADDV(tempv1, tempv1, tempv2);
      DIVVS(Global->cmphase[1], tempv1, Global->mtot+Local[ProcessId].mymtot);
      Global->mtot +=Local[ProcessId].mymtot;
      UNLOCK(Global->CountLock);
   }

   BARRIER(Global->Barrier,NPROC);

   if (ProcessId==0) {
      nttot = Global->n2bcalc + Global->nbccalc;
      nbavg = (long) ((real) Global->n2bcalc / (real) nbody);
      ncavg = (long) ((real) Global->nbccalc / (real) nbody);
   }
}



/*
 * DIAGNOSTICS: compute set of dynamical diagnostics.
 */

void diagnostics(long ProcessId)
{
   register bodyptr p,*pp;
   real velsq;
   vector tmpv;
   matrix tmpt;

   Local[ProcessId].mymtot = 0.0;
   Local[ProcessId].myetot[1] = Local[ProcessId].myetot[2] = 0.0;
   CLRM(Local[ProcessId].myketen);
   CLRM(Local[ProcessId].mypeten);
   CLRV(Local[ProcessId].mycmphase[0]);
   CLRV(Local[ProcessId].mycmphase[1]);
   CLRV(Local[ProcessId].myamvec);
   for (pp = Local[ProcessId].mybodytab+Local[ProcessId].mynbody -1;
	pp >= Local[ProcessId].mybodytab; pp--) {
      p= *pp;
      Local[ProcessId].mymtot += Mass(p);
      DOTVP(velsq, Vel(p), Vel(p));
      Local[ProcessId].myetot[1] += 0.5 * Mass(p) * velsq;
      Local[ProcessId].myetot[2] += 0.5 * Mass(p) * Phi(p);
      MULVS(tmpv, Vel(p), 0.5 * Mass(p));
      OUTVP(tmpt, tmpv, Vel(p));
      ADDM(Local[ProcessId].myketen, Local[ProcessId].myketen, tmpt);
      MULVS(tmpv, Pos(p), Mass(p));
      OUTVP(tmpt, tmpv, Acc(p));
      ADDM(Local[ProcessId].mypeten, Local[ProcessId].mypeten, tmpt);
      MULVS(tmpv, Pos(p), Mass(p));
      ADDV(Local[ProcessId].mycmphase[0], Local[ProcessId].mycmphase[0], tmpv);
      MULVS(tmpv, Vel(p), Mass(p));
      ADDV(Local[ProcessId].mycmphase[1], Local[ProcessId].mycmphase[1], tmpv);
      CROSSVP(tmpv, Pos(p), Vel(p));
      MULVS(tmpv, tmpv, Mass(p));
      ADDV(Local[ProcessId].myamvec, Local[ProcessId].myamvec, tmpv);
   }
   Local[ProcessId].myetot[0] = Local[ProcessId].myetot[1]
      + Local[ProcessId].myetot[2];
   if (Local[ProcessId].mymtot!=0){
      DIVVS(Local[ProcessId].mycmphase[0], Local[ProcessId].mycmphase[0],
	    Local[ProcessId].mymtot);
      DIVVS(Local[ProcessId].mycmphase[1], Local[ProcessId].mycmphase[1],
	    Local[ProcessId].mymtot);
   }
}



/*
 * Low-level input and output operations.
 */

void in_int(stream str, long *iptr)
{
   if (fscanf(str, "%ld", iptr) != 1)
      error("in_int: input conversion print_error\n");
}

void in_real(stream str, real *rptr)
{
   double tmp;

   if (fscanf(str, "%lf", &tmp) != 1)
      error("in_real: input conversion print_error\n");
   *rptr = tmp;
}

void in_vector(stream str, vector vec)
{
   double tmpx, tmpy, tmpz;

   if (fscanf(str, "%lf%lf%lf", &tmpx, &tmpy, &tmpz) != 3)
      error("in_vector: input conversion print_error\n");
   vec[0] = tmpx;    vec[1] = tmpy;    vec[2] = tmpz;
}

void out_int(stream str, long ival)
{
   fprintf(str, "  %ld\n", ival);
}

void out_real(stream str, real rval)
{
   fprintf(str, " %21.14E\n", rval);
}

void out_vector(stream str, vector vec)
{
   fprintf(str, " %21.14E %21.14E", vec[0], vec[1]);
   fprintf(str, " %21.14E\n",vec[2]);
}

