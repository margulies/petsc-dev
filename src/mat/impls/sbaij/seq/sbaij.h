/* $Id: sbaij.h,v 1.21 2001/08/07 03:03:01 balay Exp $ */

#include "src/mat/matimpl.h"

#if !defined(__SBAIJ_H)
#define __SBAIJ_H

/*  
  MATSEQSBAIJ format - Block compressed row storage. The i[] and j[] 
  arrays start at 0.
*/

typedef struct {
  PetscTruth       sorted;       /* if true, rows are sorted by increasing columns */
  PetscTruth       roworiented;  /* if true, row-oriented input, default */
  int              nonew;        /* 1 don't add new nonzeros, -1 generate error on new */
  PetscTruth       singlemalloc; /* if true a, i, and j have been obtained with
                                        one big malloc */
  int              bs,bs2;       /* block size, square of block size */
  int              mbs,nbs;      /* rows/bs or columns/bs */
  int              s_nz,s_maxnz; /* total diagonal and superdiagonal nonzero blocks, 
                                    total allocated diagonal and superdiagonal nonzero blocks*/                            
  int              *diag;        /* pointers to diagonal elements */
  int              *i;           /* pointer to beginning of each row */
  int              *inew;        /* pointer to beginning of each row of reordered matrix */
  int              *imax;        /* maximum space allocated for each row */
  int              *ilen;        /* actual length of each row */
  int              *j;           /* column values: j + i[k] is start of row k of reordered matrix */
  int              *jnew;        /* column values: jnew + i[k] is start of row k */
  MatScalar        *a;           /* nonzero diagonal and superdiagonal elements */
  MatScalar        *anew;        /* nonzero diagonal and superdiagonal elements of reordered matrix */
  IS               row,icol;     /* index sets, used for reorderings */
  PetscScalar      *solve_work;  /* work space used in MatSolve */
  PetscScalar      *solves_work; /* work space used in MatSolves */
  int              solves_work_n;/* size of solves_work */  
  int              reallocs;     /* number of mallocs done during MatSetValues() 
                                    as more values are set then were preallocated */
  PetscScalar      *mult_work;   /* work array for matrix vector product*/
  PetscScalar      *saved_values; 

  PetscTruth       keepzeroedrows; /* if true, MatZeroRows() will not change nonzero structure */
  int              *a2anew;        /* map used for symm permutation */
  PetscTruth       permute;        /* if true, a non-trivial permutation is used for factorization */
  PetscTruth       pivotinblocks;  /* pivot inside factorization of each diagonal block */

  /* carry MatFactorInfo from symbolic factor to numeric factor */
  int              factor_levels;
  PetscReal        factor_damping;     
  PetscReal        factor_shift;
  PetscReal        factor_zeropivot;

  int              *xtoy,*xtoyB;     /* map nonzero pattern of X into Y's, used by MatAXPY() */
  Mat              XtoY;             /* used by MatAXPY() */
} Mat_SeqSBAIJ;

extern int MatICCFactorSymbolic_SeqSBAIJ(Mat,IS,MatFactorInfo*,Mat *);
extern int MatDuplicate_SeqSBAIJ(Mat,MatDuplicateOption,Mat*);
extern int MatMarkDiagonal_SeqSBAIJ(Mat);

extern int MatCholeskyFactorNumeric_SeqSBAIJ_1_NaturalOrdering(Mat,Mat*);
extern int MatSolve_SeqSBAIJ_1_NaturalOrdering(Mat,Vec,Vec);
extern int MatSolveTranspose_SeqSBAIJ_1_NaturalOrdering(Mat,Vec,Vec);

extern int MatCholeskyFactorNumeric_SeqSBAIJ_2_NaturalOrdering(Mat,Mat*);
extern int MatSolve_SeqSBAIJ_2_NaturalOrdering(Mat,Vec,Vec);
extern int MatSolveTranspose_SeqSBAIJ_2_NaturalOrdering(Mat,Vec,Vec);

extern int MatCholeskyFactorNumeric_SeqSBAIJ_3_NaturalOrdering(Mat,Mat*);
extern int MatSolve_SeqSBAIJ_3_NaturalOrdering(Mat,Vec,Vec);
extern int MatSolveTranspose_SeqSBAIJ_3_NaturalOrdering(Mat,Vec,Vec);

extern int MatCholeskyFactorNumeric_SeqSBAIJ_4_NaturalOrdering(Mat,Mat*);
extern int MatSolve_SeqSBAIJ_4_NaturalOrdering(Mat,Vec,Vec);
extern int MatSolveTranspose_SeqSBAIJ_4_NaturalOrdering(Mat,Vec,Vec);

extern int MatCholeskyFactorNumeric_SeqSBAIJ_5_NaturalOrdering(Mat,Mat*);
extern int MatSolve_SeqSBAIJ_5_NaturalOrdering(Mat,Vec,Vec);
extern int MatSolveTranspose_SeqSBAIJ_5_NaturalOrdering(Mat,Vec,Vec);

extern int MatCholeskyFactorNumeric_SeqSBAIJ_6_NaturalOrdering(Mat,Mat*);
extern int MatSolve_SeqSBAIJ_6_NaturalOrdering(Mat,Vec,Vec);
extern int MatSolveTranspose_SeqSBAIJ_6_NaturalOrdering(Mat,Vec,Vec);

extern int MatCholeskyFactorNumeric_SeqSBAIJ_7_NaturalOrdering(Mat,Mat*);
extern int MatSolve_SeqSBAIJ_7_NaturalOrdering(Mat,Vec,Vec);
extern int MatSolveTranspose_SeqSBAIJ_7_NaturalOrdering(Mat,Vec,Vec);

extern int MatCholeskyFactorNumeric_SeqSBAIJ_N_NaturalOrdering(Mat,Mat*);
extern int MatSolve_SeqSBAIJ_N_NaturalOrdering(Mat,Vec,Vec);

extern int MatRelax_SeqSBAIJ(Mat,Vec,PetscReal,MatSORType,PetscReal,int,int,Vec);

#endif
