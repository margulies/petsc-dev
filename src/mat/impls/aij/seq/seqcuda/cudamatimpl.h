#ifndef __CUDAMATIMPL 
#define __CUDAMATIMPL

#include "../src/vec/vec/impls/seq/seqcuda/cudavecimpl.h"
#include <cusp/csr_matrix.h>
#include <cusp/multiply.h>

#define CUSPMATRIX cusp::csr_matrix<PetscInt,PetscScalar,cusp::device_memory>

struct SeqAIJCUDA_Container {
  CUSPMATRIX* mat; /* pointer to the matrix on the GPU */
  CUSPARRAY*  indices; /*pointer to an array containing the nonzero row indices, should usecprow be true*/
  CUSPARRAY*  tempvec; /*if usecprow, pointer to a workvector to which we can copy the relevant indices of a vector we want to multiply */
};

#endif
