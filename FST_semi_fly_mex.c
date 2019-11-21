/*
 * FST_semi_fly_mex.c
 *
 * [FSTRep] = FST_semi_fly_mex(Points)
 *
 * Calculate the spherical harmonic transform of a sampled band-limited
 * function using S2Kit.
 *
 * FSTRep is a list of spherical harmonic expansion coefficients in the order
 * described in the S2kit documentation. It contains bandwidth^2 elements.
 *
 * Points is a set of values for each (theta, phi) point on the grid
 * generated by MakeFSTGrid(bandwidth). It contains 2*bandwidth x
 * 2*bandwidth elements.
 *
 * Compile with:
 *   mex -v FST_semi_fly_mex.c -I/home/stieltjes/rodgers/bin/fftw-3.0.1-bin/include -L/home/stieltjes/rodgers/bin/fftw-3.0.1-bin/lib -lfftw3 pmls.o cospmls.o seminaive.o csecond.o primitive.o makeweights.o naive_synthesis.o FST_semi_fly.o
 */

/*
   Copyright: Chris Rodgers, Aug, 2005.

   This file is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This file is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this file; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

   See the accompanying LICENSE file for details.
  
*/

static char cvsid[] = "$Id: FST_semi_fly_mex.c 4052 2011-04-01 17:58:00Z crodgers $";

#include <math.h>
#include "mex.h"

#include "fftw3.h"
#include "makeweights.h"
#include "FST_semi_fly.h"

/* Defines */
#define NOBIN -1
#define PRIVATE static
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))


bool CheckDimensions(const mxArray *x, const int* wanteddimensions, const int wantednumdimensions)
{
  const int *dimensions;
  int numdimensions;
  int i;

  dimensions=mxGetDimensions(x);
  numdimensions=mxGetNumberOfDimensions(x);
  
  if (numdimensions!=wantednumdimensions)
    return false;

  for (i=0;i<numdimensions;i++)
  {
    if (dimensions[i]!=wanteddimensions[i] && wanteddimensions[i]!=-1)
      return false;
  }
  return true;
}

double GetScalarDouble(const mxArray *x)
{
  int wanteddims[]={1,1};
  if (!CheckDimensions(x,wanteddims,2))
    mexErrMsgTxt("Wrong dimensions in argument!");

  if (!mxIsDouble(x) || mxIsComplex(x))
    mexErrMsgTxt("Input must be a scalar double.");

  return mxGetScalar(x);
}

void mexFunction(
    int		  nlhs, 	/* number of expected outputs */
    mxArray	  *plhs[],	/* mxArray output pointer array */
    int		  nrhs, 	/* number of inputs */
    const mxArray *prhs[]	/* mxArray input pointer array */
   )
{
  const mxArray *mxaData;
  mxArray *mxaResult;
  double *ReData, *ImData;
  double *ReResult, *ImResult;
  double *workspace, *weights;

  int bandwidth;
  int size;
  int rank, howmany_rank ;
  int dataformat; /* dataformat =0 -> samples are complex, =1 -> samples real */

  fftw_plan dctPlan ;
  fftw_plan fftPlan ;
  fftw_iodim dims[1], howmany_dims[1];

  if (nlhs > 1)
    mexErrMsgTxt("Too many output arguments.");
  if (nrhs < 1)
    mexErrMsgTxt("Not enough input arguments.");
  if (nrhs > 1)
    mexErrMsgTxt("Too many input arguments.");
  
  /* Fetch input arguments */
  mxaData = prhs[0];
  
  const int *dimensions;

  dimensions=mxGetDimensions(mxaData);
  if (mxGetNumberOfDimensions(mxaData)!=2 || mxGetM(mxaData) != mxGetN(mxaData) || (mxGetM(mxaData)%2)!=0)
    mexErrMsgTxt("Input must be a 2*bandwidth x 2*bandwidth square matrix!");
  
  size=dimensions[0];
  bandwidth=size/2;
  
  ReData=mxGetPr(mxaData);
  ImData=mxGetPi(mxaData); /* ImData will be 0 if input is real */

  if (ImData==NULL)
    {
      dataformat=1;
      ImData=mxCalloc(size*size,sizeof(double)); /* FST_semi_fly's FFT routines aren't currently adapted for pure real data */
    }
  else
    dataformat=0;

  /*  mexPrintf("ReData @ %x, ImData @ %x\n%d: 0 when samples are complex, 1 when real\n",ReData,ImData,dataformat); */

  /* Arguments are the correct type/size */

  /* Create the output arrays */
  mxaResult=mxCreateDoubleMatrix(bandwidth*bandwidth,1,mxCOMPLEX);
  ReResult=mxGetPr(mxaResult);
  ImResult=mxGetPi(mxaResult);

  /* Call the S2kit routine FST_semi_fly */
  
  /*
    allocate memory
  */

  workspace = (double *) mxMalloc(sizeof(double) * 
				  ((10 * (bandwidth*bandwidth)) + 
				   (24 * bandwidth)));

  /* make array for weights */
  weights = (double *) mxMalloc(sizeof(double) * 4 * bandwidth);


  /****
    At this point, check to see if all the memory has been
    allocated. If it has not, there's no point in going further.
    ****/
  if ( (ReData == NULL) || (ImData == NULL) ||
       (ReResult == NULL) || (ImResult == NULL) ||
       (workspace == NULL) || (weights == NULL) )
    {
      /* Free memory */
      if (dataformat==1)
	mxFree(ImData);
      
      mxFree(workspace);
      mxFree(weights);
      
      mexErrMsgTxt("Error in allocating memory");
    }

  /*
    construct fftw plans
  */

  /* make DCT plans -> note that I will be using the GURU
     interface to execute these plans within the routines*/

  /* forward DCT */
  dctPlan = fftw_plan_r2r_1d( 2*bandwidth, weights, ReData,
			      FFTW_REDFT10, FFTW_ESTIMATE ) ;
      
  /*
    fftw "preamble" ;
    note that this plan places the output in a transposed array
  */
  rank = 1 ;
  dims[0].n = 2*bandwidth ;
  dims[0].is = 1 ;
  dims[0].os = 2*bandwidth ;
  howmany_rank = 1 ;
  howmany_dims[0].n = 2*bandwidth ;
  howmany_dims[0].is = 2*bandwidth ;
  howmany_dims[0].os = 1 ;
  
  /* forward fft */
  fftPlan = fftw_plan_guru_split_dft( rank, dims,
				      howmany_rank, howmany_dims,
				      ReData, ImData,
				      workspace, workspace+(4*bandwidth*bandwidth),
				      FFTW_ESTIMATE );

  /* now make the weights */
  makeweights( bandwidth, weights );

  FST_semi_fly(ReData, ImData,
	       ReResult, ImResult,
	       bandwidth,
	       workspace,
	       dataformat,
	       bandwidth, /* Use seminaive for all orders */
	       &dctPlan,
	       &fftPlan,
	       weights ) ;

  plhs[0]=mxaResult;

  /* Free memory */
  if (dataformat==1)
    mxFree(ImData);
  
  mxFree(workspace);
  mxFree(weights);
}