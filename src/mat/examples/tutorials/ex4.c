/*$Id: ex1.c,v 1.28 2001/04/10 19:35:58 bsmith Exp $*/

static char help[] = "Reads U and V matrices from a file and performs y = V*U'*x.\n\
  -f <input_file> : file to load \n\n";

/* 
  Include "petscmat.h" so that we can use matrices.
  automatically includes:
     petsc.h       - base PETSc routines   petscvec.h    - vectors
     petscsys.h    - system routines       petscmat.h    - matrices
     petscis.h     - index sets            petscviewer.h - viewers               
*/
#include "petscmat.h"
extern int LowRankUpdate(Mat,Mat,Vec,Vec,Vec,Vec,int);


#undef __FUNCT__
#define __FUNCT__ "main"
int main(int argc,char **args)
{
  Mat                   U,V;                /* matrix */
  PetscViewer           fd;               /* viewer */
  char                  file[128];     /* input file name */
  int                   ierr;
  PetscTruth            flg;
  Vec                   x,y,work1,work2;
  int                   i,N,n,M,m;
  PetscScalar           *xx;

  PetscInitialize(&argc,&args,(char *)0,help);

  /* 
     Determine file from which we read the matrix

  */
  ierr = PetscOptionsGetString(PETSC_NULL,"-f",file,127,&flg);CHKERRQ(ierr);
  if (!flg) SETERRQ(1,"Must indicate binary file with the -f option");


  /* 
     Open binary file.  Note that we use PETSC_FILE_RDONLY to indicate
     reading from this file.
  */
  ierr = PetscViewerBinaryOpen(PETSC_COMM_WORLD,file,PETSC_FILE_RDONLY,&fd);CHKERRQ(ierr);

  /*
    Load the matrix; then destroy the viewer.
    Note both U and V are stored as tall skinny matrices 
  */
  ierr = MatLoad(fd,MATMPIDENSE,&U);CHKERRQ(ierr);
  ierr = MatLoad(fd,MATMPIDENSE,&V);CHKERRQ(ierr);
  ierr = PetscViewerDestroy(fd);CHKERRQ(ierr);

  ierr = MatGetLocalSize(U,&N,&n);CHKERRQ(ierr);
  ierr = MatGetLocalSize(V,&M,&m);CHKERRQ(ierr);
  if (N != M) SETERRQ2(1,"U and V matrices must have same number of local rows %d %d",N,M);
  if (n != m) SETERRQ2(1,"U and V matrices must have same number of local columns %d %d",n,m);

  ierr = VecCreateMPI(PETSC_COMM_WORLD,N,PETSC_DETERMINE,&x);CHKERRQ(ierr);
  ierr = VecDuplicate(x,&y);CHKERRQ(ierr);

  ierr = MatGetSize(U,0,&n);CHKERRQ(ierr);
  ierr = VecCreateSeq(PETSC_COMM_SELF,n,&work1);CHKERRQ(ierr);
  ierr = VecDuplicate(work1,&work2);CHKERRQ(ierr);

  /* put some initial values into x for testing */
  ierr = VecGetArray(x,&xx);CHKERRQ(ierr);
  for (i=0; i<N; i++) xx[i] = i;
  ierr = VecRestoreArray(x,&xx);CHKERRQ(ierr);
  ierr = LowRankUpdate(U,V,x,y,work1,work2,n);CHKERRQ(ierr);
  ierr = VecView(x,PETSC_VIEWER_STDOUT_WORLD);CHKERRQ(ierr);
  ierr = VecView(y,PETSC_VIEWER_STDOUT_WORLD);CHKERRQ(ierr);

  /* 
     Free work space.  All PETSc objects should be destroyed when they
     are no longer needed.
  */
  ierr = MatDestroy(U);CHKERRQ(ierr);
  ierr = MatDestroy(V);CHKERRQ(ierr);
  ierr = VecDestroy(x);CHKERRQ(ierr);
  ierr = VecDestroy(y);CHKERRQ(ierr);
  ierr = VecDestroy(work1);CHKERRQ(ierr);
  ierr = VecDestroy(work2);CHKERRQ(ierr);

  ierr = PetscFinalize();CHKERRQ(ierr);
  return 0;
}

#include "src/mat/impls/dense/mpi/mpidense.h"

#undef __FUNCT__  
#define __FUNCT__ "LowRankUpdate"
/*
     Computes y = V*U'*x where U and V are  N by n (N >> n). 

     U and V are stored as PETSc MPIDENSE (parallel) dense matrices with their rows partitioned the
     same way as x and y are partitioned
*/
int LowRankUpdate(Mat U,Mat V,Vec x,Vec y,Vec work1,Vec work2,int nwork)
{
  Mat         Ulocal = ((Mat_MPIDense*)U->data)->A;
  Mat         Vlocal = ((Mat_MPIDense*)V->data)->A;
  int         ierr,Nsave = x->N;
  PetscScalar *w1,*w2;

  PetscFunctionBegin;

  /* First multiply the local part of U with the local part of x */
  x->N = x->n; /* this tricks the silly error checking in MatMultTranspose();*/
  ierr = MatMultTranspose(Ulocal,x,work1);CHKERRQ(ierr);/* note in this call x is treated as a sequential vector  */
  x->N = Nsave;

  /* Form the sum of all the local multiplies : this is work2 = U'*x = sum_{all processors} work1 */
  ierr = VecGetArray(work1,&w1);CHKERRQ(ierr);
  ierr = VecGetArray(work2,&w2);CHKERRQ(ierr);
  ierr = MPI_Allreduce(w1,w2,nwork,MPI_DOUBLE,MPI_SUM,PETSC_COMM_WORLD);CHKERRQ(ierr);
  ierr = VecRestoreArray(work1,&w1);CHKERRQ(ierr);
  ierr = VecRestoreArray(work2,&w2);CHKERRQ(ierr);

  /* multiply y = V*work2 */
  y->N = y->n; /* this tricks the silly error checking in MatMult() */
  ierr = MatMult(Vlocal,work2,y);CHKERRQ(ierr);/* note in this call y is treated as a sequential vector  */
  y->N = Nsave;

  PetscFunctionReturn(0);
}
