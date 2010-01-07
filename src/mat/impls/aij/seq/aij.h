
#if !defined(__AIJ_H)
#define __AIJ_H

#include "private/matimpl.h"

/*  
    Struct header shared by SeqAIJ, SeqBAIJ and SeqSBAIJ matrix formats
*/
#define SEQAIJHEADER(datatype)	\
  PetscTruth        roworiented;      /* if true, row-oriented input, default */\
  PetscInt          nonew;            /* 1 don't add new nonzeros, -1 generate error on new */\
  PetscInt          nounused;         /* -1 generate error on unused space */\
  PetscTruth        singlemalloc;     /* if true a, i, and j have been obtained with one big malloc */\
  PetscInt          maxnz;            /* allocated nonzeros */\
  PetscInt          *imax;            /* maximum space allocated for each row */\
  PetscInt          *ilen;            /* actual length of each row */\
  PetscTruth        free_imax_ilen;  \
  PetscInt          reallocs;         /* number of mallocs done during MatSetValues() \
                                        as more values are set than were prealloced */\
  PetscInt          rmax;             /* max nonzeros in any row */\
  PetscTruth        keepnonzeropattern;   /* keeps matrix structure same in calls to MatZeroRows()*/\
  PetscTruth        ignorezeroentries; \
  PetscInt          *xtoy,*xtoyB;     /* map nonzero pattern of X into Y's, used by MatAXPY() */\
  Mat               XtoY;             /* used by MatAXPY() */\
  PetscTruth        free_ij;          /* free the column indices j and row offsets i when the matrix is destroyed */ \
  PetscTruth        free_a;           /* free the numerical values when matrix is destroy */ \
  Mat_CompressedRow compressedrow;    /* use compressed row format */                      \
  PetscInt          nz;               /* nonzeros */                                       \
  PetscInt          *i;               /* pointer to beginning of each row */               \
  PetscInt          *j;               /* column values: j + i[k] - 1 is start of row k */  \
  PetscInt          *diag;            /* pointers to diagonal elements */                  \
  PetscTruth        free_diag;         \
  datatype          *a;               /* nonzero elements */                               \
  PetscScalar       *solve_work;      /* work space used in MatSolve */                    \
  IS                row, col, icol;   /* index sets, used for reorderings */ \
  PetscTruth        pivotinblocks;    /* pivot inside factorization of each diagonal block */ \
  Mat               parent             /* set if this matrix was formed with MatDuplicate(...,MAT_SHARE_NONZERO_PATTERN,....); 
                                         means that this shares some data structures with the parent including diag, ilen, imax, i, j */

/*  
  MATSEQAIJ format - Compressed row storage (also called Yale sparse matrix
  format) or compressed sparse row (CSR).  The i[] and j[] arrays start at 0. For example,
  j[i[k]+p] is the pth column in row k.  Note that the diagonal
  matrix elements are stored with the rest of the nonzeros (not separately).
*/

/* Info about i-nodes (identical nodes) helper class for SeqAIJ */
typedef struct {
  MatScalar   *bdiag,*ibdiag,*ssor_work;      /* diagonal blocks of matrix used for MatSOR_SeqAIJ_Inode() */
  PetscInt    bdiagsize;                       /* length of bdiag and ibdiag */
  PetscTruth  ibdiagvalid;                     /* do ibdiag[] and bdiag[] contain the most recent values */

  PetscTruth use;
  PetscInt   node_count;                    /* number of inodes */
  PetscInt   *size;                         /* size of each inode */
  PetscInt   limit;                         /* inode limit */
  PetscInt   max_limit;                     /* maximum supported inode limit */
  PetscTruth checked;                       /* if inodes have been checked for */
} Mat_SeqAIJ_Inode;

EXTERN PetscErrorCode MatView_SeqAIJ_Inode(Mat,PetscViewer);
EXTERN PetscErrorCode MatAssemblyEnd_SeqAIJ_Inode(Mat,MatAssemblyType);
EXTERN PetscErrorCode MatDestroy_SeqAIJ_Inode(Mat);
EXTERN PetscErrorCode MatCreate_SeqAIJ_Inode(Mat);
EXTERN PetscErrorCode MatSetOption_SeqAIJ_Inode(Mat,MatOption,PetscTruth);
EXTERN PetscErrorCode MatDuplicate_SeqAIJ_Inode(Mat,MatDuplicateOption,Mat*);
EXTERN PetscErrorCode MatILUDTFactor_SeqAIJ_Inode(Mat,IS,IS,const MatFactorInfo*,Mat*);
EXTERN PetscErrorCode MatLUFactorSymbolic_SeqAIJ_Inode(Mat,Mat,IS,IS,const MatFactorInfo*);
EXTERN PetscErrorCode MatILUFactorSymbolic_SeqAIJ_Inode(Mat,Mat,IS,IS,const MatFactorInfo*);

typedef struct {
  SEQAIJHEADER(MatScalar);
  Mat_SeqAIJ_Inode inode;
  MatScalar        *saved_values;             /* location for stashing nonzero values of matrix */

  PetscScalar      *idiag,*mdiag,*ssor_work;  /* inverse of diagonal entries, diagonal values and workspace for Eisenstat trick */
  PetscTruth       idiagvalid;                     /* current idiag[] and mdiag[] are valid */
  PetscScalar      fshift,omega;                   /* last used omega and fshift */

  ISColoring       coloring;                  /* set with MatADSetColoring() used by MatADSetValues() */
} Mat_SeqAIJ;

/*
    Frees the a, i, and j arrays from the XAIJ (AIJ, BAIJ, and SBAIJ) matrix types
*/
#undef __FUNCT__  
#define __FUNCT__ "MatSeqXAIJFreeAIJ"
PETSC_STATIC_INLINE PetscErrorCode MatSeqXAIJFreeAIJ(Mat AA,MatScalar **a,PetscInt **j,PetscInt **i) 
{
                                     PetscErrorCode ierr;
                                     Mat_SeqAIJ     *A = (Mat_SeqAIJ*) AA->data;
                                     if (A->singlemalloc) {
                                       ierr = PetscFree3(*a,*j,*i);CHKERRQ(ierr);
                                     } else {
                                       if (A->free_a  && *a) {ierr = PetscFree(*a);CHKERRQ(ierr);}
                                       if (A->free_ij && *j) {ierr = PetscFree(*j);CHKERRQ(ierr);}
                                       if (A->free_ij && *i) {ierr = PetscFree(*i);CHKERRQ(ierr);}
                                     }
                                     *a = 0; *j = 0; *i = 0;
                                     return 0;
                                   }

/*
    Allocates larger a, i, and j arrays for the XAIJ (AIJ, BAIJ, and SBAIJ) matrix types
*/
#define MatSeqXAIJReallocateAIJ(Amat,AM,BS2,NROW,ROW,COL,RMAX,AA,AI,AJ,RP,AP,AIMAX,NONEW,datatype) \
  if (NROW >= RMAX) {\
	Mat_SeqAIJ *Ain = (Mat_SeqAIJ*)Amat->data;\
        /* there is no extra room in row, therefore enlarge */ \
        PetscInt   new_nz = AI[AM] + CHUNKSIZE,len,*new_i=0,*new_j=0; \
        datatype   *new_a; \
 \
        if (NONEW == -2) SETERRQ2(PETSC_ERR_ARG_OUTOFRANGE,"New nonzero at (%D,%D) caused a malloc",ROW,COL); \
        /* malloc new storage space */ \
        ierr = PetscMalloc3(BS2*new_nz,datatype,&new_a,new_nz,PetscInt,&new_j,AM+1,PetscInt,&new_i);CHKERRQ(ierr);\
 \
        /* copy over old data into new slots */ \
        for (ii=0; ii<ROW+1; ii++) {new_i[ii] = AI[ii];} \
        for (ii=ROW+1; ii<AM+1; ii++) {new_i[ii] = AI[ii]+CHUNKSIZE;} \
        ierr = PetscMemcpy(new_j,AJ,(AI[ROW]+NROW)*sizeof(PetscInt));CHKERRQ(ierr); \
        len = (new_nz - CHUNKSIZE - AI[ROW] - NROW); \
        ierr = PetscMemcpy(new_j+AI[ROW]+NROW+CHUNKSIZE,AJ+AI[ROW]+NROW,len*sizeof(PetscInt));CHKERRQ(ierr); \
        ierr = PetscMemcpy(new_a,AA,BS2*(AI[ROW]+NROW)*sizeof(datatype));CHKERRQ(ierr); \
        ierr = PetscMemzero(new_a+BS2*(AI[ROW]+NROW),BS2*CHUNKSIZE*sizeof(datatype));CHKERRQ(ierr);\
        ierr = PetscMemcpy(new_a+BS2*(AI[ROW]+NROW+CHUNKSIZE),AA+BS2*(AI[ROW]+NROW),BS2*len*sizeof(datatype));CHKERRQ(ierr);  \
        /* free up old matrix storage */ \
        ierr = MatSeqXAIJFreeAIJ(A,&Ain->a,&Ain->j,&Ain->i);CHKERRQ(ierr);\
        AA = new_a; \
        Ain->a = (MatScalar*) new_a;		   \
        AI = Ain->i = new_i; AJ = Ain->j = new_j;  \
        Ain->singlemalloc = PETSC_TRUE; \
 \
        RP          = AJ + AI[ROW]; AP = AA + BS2*AI[ROW]; \
        RMAX        = AIMAX[ROW] = AIMAX[ROW] + CHUNKSIZE; \
        Ain->maxnz += CHUNKSIZE; \
        Ain->reallocs++; \
      } \

EXTERN_C_BEGIN
EXTERN PetscErrorCode MatSeqAIJSetPreallocation_SeqAIJ(Mat,PetscInt,PetscInt*);
EXTERN_C_END
EXTERN PetscErrorCode MatILUFactorSymbolic_SeqAIJ(Mat,Mat,IS,IS,const MatFactorInfo*);
EXTERN PetscErrorCode MatILUFactorSymbolic_SeqAIJ_newdatastruct(Mat,Mat,IS,IS,const MatFactorInfo*);
EXTERN PetscErrorCode MatILUFactorSymbolic_SeqAIJ_ilu0_newdatastruct(Mat,Mat,IS,IS,const MatFactorInfo*);

EXTERN PetscErrorCode MatICCFactorSymbolic_SeqAIJ(Mat,Mat,IS,const MatFactorInfo*);
EXTERN PetscErrorCode MatCholeskyFactorSymbolic_SeqAIJ(Mat,Mat,IS,const MatFactorInfo*);
EXTERN PetscErrorCode MatCholeskyFactorNumeric_SeqAIJ(Mat,Mat,const MatFactorInfo*);
EXTERN PetscErrorCode MatDuplicate_SeqAIJ(Mat,MatDuplicateOption,Mat*);
EXTERN PetscErrorCode MatMissingDiagonal_SeqAIJ(Mat,PetscTruth*,PetscInt*);
EXTERN PetscErrorCode MatMarkDiagonal_SeqAIJ(Mat);

EXTERN PetscErrorCode MatMult_SeqAIJ(Mat A,Vec,Vec);
EXTERN PetscErrorCode MatMultAdd_SeqAIJ(Mat A,Vec,Vec,Vec);
EXTERN PetscErrorCode MatMultTranspose_SeqAIJ(Mat A,Vec,Vec);
EXTERN PetscErrorCode MatMultTransposeAdd_SeqAIJ(Mat A,Vec,Vec,Vec);
EXTERN PetscErrorCode MatSOR_SeqAIJ(Mat,Vec,PetscReal,MatSORType,PetscReal,PetscInt,PetscInt,Vec);

EXTERN PetscErrorCode MatSetColoring_SeqAIJ(Mat,ISColoring);
EXTERN PetscErrorCode MatSetValuesAdic_SeqAIJ(Mat,void*);
EXTERN PetscErrorCode MatSetValuesAdifor_SeqAIJ(Mat,PetscInt,void*);

EXTERN PetscErrorCode MatGetSymbolicTranspose_SeqAIJ(Mat,PetscInt *[],PetscInt *[]);
EXTERN PetscErrorCode MatGetSymbolicTransposeReduced_SeqAIJ(Mat,PetscInt,PetscInt,PetscInt *[],PetscInt *[]);
EXTERN PetscErrorCode MatRestoreSymbolicTranspose_SeqAIJ(Mat,PetscInt *[],PetscInt *[]);
EXTERN PetscErrorCode MatToSymmetricIJ_SeqAIJ(PetscInt,PetscInt*,PetscInt*,PetscInt,PetscInt,PetscInt**,PetscInt**);
EXTERN PetscErrorCode MatLUFactorSymbolic_SeqAIJ(Mat,Mat,IS,IS,const MatFactorInfo*);
EXTERN PetscErrorCode MatLUFactorSymbolic_SeqAIJ_newdatastruct(Mat,Mat,IS,IS,const MatFactorInfo*);
EXTERN PetscErrorCode MatLUFactorNumeric_SeqAIJ(Mat,Mat,const MatFactorInfo*);
EXTERN PetscErrorCode MatLUFactorNumeric_SeqAIJ_newdatastruct(Mat,Mat,const MatFactorInfo*);
EXTERN PetscErrorCode MatLUFactorNumeric_SeqAIJ_InplaceWithPerm(Mat,Mat,const MatFactorInfo*);
EXTERN PetscErrorCode MatLUFactor_SeqAIJ(Mat,IS,IS,const MatFactorInfo*);
EXTERN PetscErrorCode MatSolve_SeqAIJ(Mat,Vec,Vec);
EXTERN PetscErrorCode MatSolve_SeqAIJ_newdatastruct(Mat,Vec,Vec);
EXTERN PetscErrorCode MatSolve_SeqAIJ_NaturalOrdering(Mat,Vec,Vec);
EXTERN PetscErrorCode MatSolve_SeqAIJ_NaturalOrdering_newdatastruct(Mat,Vec,Vec);
EXTERN PetscErrorCode MatSolve_SeqAIJ_InplaceWithPerm(Mat,Vec,Vec);
EXTERN PetscErrorCode MatSolveAdd_SeqAIJ(Mat,Vec,Vec,Vec);
EXTERN PetscErrorCode MatSolveTranspose_SeqAIJ(Mat,Vec,Vec);
EXTERN PetscErrorCode MatSolveTranspose_SeqAIJ_newdatastruct(Mat,Vec,Vec);
EXTERN PetscErrorCode MatSolveTransposeAdd_SeqAIJ(Mat,Vec,Vec,Vec);
EXTERN PetscErrorCode MatMatSolve_SeqAIJ(Mat,Mat,Mat);
EXTERN PetscErrorCode MatEqual_SeqAIJ(Mat A,Mat B,PetscTruth* flg);
EXTERN PetscErrorCode MatFDColoringCreate_SeqAIJ(Mat,ISColoring,MatFDColoring);
EXTERN PetscErrorCode MatILUDTFactor_SeqAIJ(Mat,IS,IS,const MatFactorInfo*,Mat*);
EXTERN PetscErrorCode MatILUDTFactorSymbolic_SeqAIJ(Mat,Mat,IS,IS,const MatFactorInfo*);
EXTERN PetscErrorCode MatILUDTFactorNumeric_SeqAIJ(Mat,Mat,const MatFactorInfo*);
EXTERN PetscErrorCode MatLoad_SeqAIJ(PetscViewer, const MatType,Mat*);
EXTERN PetscErrorCode RegisterApplyPtAPRoutines_Private(Mat);
EXTERN PetscErrorCode MatMatMultSymbolic_SeqAIJ_SeqAIJ(Mat,Mat,PetscReal,Mat*);
EXTERN PetscErrorCode MatMatMultNumeric_SeqAIJ_SeqAIJ(Mat,Mat,Mat);
EXTERN PetscErrorCode MatPtAPSymbolic_SeqAIJ(Mat,Mat,PetscReal,Mat*);
EXTERN PetscErrorCode MatPtAPNumeric_SeqAIJ(Mat,Mat,Mat);
EXTERN PetscErrorCode MatPtAPSymbolic_SeqAIJ_SeqAIJ(Mat,Mat,PetscReal,Mat*);
EXTERN PetscErrorCode MatPtAPNumeric_SeqAIJ_SeqAIJ(Mat,Mat,Mat);
EXTERN PetscErrorCode MatMatMultTranspose_SeqAIJ_SeqAIJ(Mat,Mat,MatReuse,PetscReal,Mat*);
EXTERN PetscErrorCode MatMatMultTransposeSymbolic_SeqAIJ_SeqAIJ(Mat,Mat,PetscReal,Mat*);
EXTERN PetscErrorCode MatMatMultTransposeNumeric_SeqAIJ_SeqAIJ(Mat,Mat,Mat);
EXTERN PetscErrorCode MatSetValues_SeqAIJ(Mat,PetscInt,const PetscInt[],PetscInt,const PetscInt[],const PetscScalar[],InsertMode);
EXTERN PetscErrorCode MatGetRow_SeqAIJ(Mat,PetscInt,PetscInt*,PetscInt**,PetscScalar**);
EXTERN PetscErrorCode MatRestoreRow_SeqAIJ(Mat,PetscInt,PetscInt*,PetscInt**,PetscScalar**);
EXTERN PetscErrorCode MatAXPY_SeqAIJ(Mat,PetscScalar,Mat,MatStructure);
EXTERN PetscErrorCode MatGetRowIJ_SeqAIJ(Mat,PetscInt,PetscTruth,PetscTruth,PetscInt*,PetscInt *[],PetscInt *[],PetscTruth *);
EXTERN PetscErrorCode MatRestoreRowIJ_SeqAIJ(Mat,PetscInt,PetscTruth,PetscTruth,PetscInt *,PetscInt *[],PetscInt *[],PetscTruth *);
EXTERN PetscErrorCode MatGetColumnIJ_SeqAIJ(Mat,PetscInt,PetscTruth,PetscTruth,PetscInt*,PetscInt *[],PetscInt *[],PetscTruth *);
EXTERN PetscErrorCode MatRestoreColumnIJ_SeqAIJ(Mat,PetscInt,PetscTruth,PetscTruth,PetscInt *,PetscInt *[],PetscInt *[],PetscTruth *);
EXTERN PetscErrorCode MatDestroy_SeqAIJ(Mat);
EXTERN PetscErrorCode MatView_SeqAIJ(Mat,PetscViewer);

EXTERN_C_BEGIN
EXTERN PetscErrorCode PETSCMAT_DLLEXPORT MatConvert_SeqAIJ_SeqSBAIJ(Mat,const MatType,MatReuse,Mat*);
EXTERN PetscErrorCode PETSCMAT_DLLEXPORT MatConvert_SeqAIJ_SeqBAIJ(Mat,const MatType,MatReuse,Mat*);
EXTERN PetscErrorCode PETSCMAT_DLLEXPORT MatConvert_SeqAIJ_SeqCSRPERM(Mat,const MatType,MatReuse,Mat*);
EXTERN PetscErrorCode PETSCMAT_DLLEXPORT MatReorderForNonzeroDiagonal_SeqAIJ(Mat,PetscReal,IS,IS);
EXTERN PetscErrorCode PETSCMAT_DLLEXPORT MatMatMult_SeqAIJ_SeqAIJ(Mat,Mat,MatReuse,PetscReal,Mat*);
EXTERN_C_END

/*
    PetscSparseDenseMinusDot - The inner kernel of triangular solves and Gauss-Siedel smoothing. \sum_i xv[i] * r[xi[i]] for CSR storage

  Input Parameters:
+  nnz - the number of entries
.  r - the array of vector values
.  xv - the matrix values for the row
-  xi - the column indices of the nonzeros in the row

  Output Parameter:
.  sum - negative the sum of results

  PETSc compile flags:
+   PETSC_KERNEL_USE_UNROLL_4 -   don't use this; it changes nnz and hence is WRONG
-   PETSC_KERNEL_USE_UNROLL_2 -

.seealso: PetscSparseDensePlusDot()

*/
#ifdef PETSC_KERNEL_USE_UNROLL_4
#define PetscSparseDenseMinusDot(sum,r,xv,xi,nnz) {\
if (nnz > 0) {\
switch (nnz & 0x3) {\
case 3: sum -= *xv++ * r[*xi++];\
case 2: sum -= *xv++ * r[*xi++];\
case 1: sum -= *xv++ * r[*xi++];\
nnz -= 4;}\
while (nnz > 0) {\
sum -=  xv[0] * r[xi[0]] - xv[1] * r[xi[1]] -\
	xv[2] * r[xi[2]] - xv[3] * r[xi[3]];\
xv  += 4; xi += 4; nnz -= 4; }}}

#elif defined(PETSC_KERNEL_USE_UNROLL_2)
#define PetscSparseDenseMinusDot(sum,r,xv,xi,nnz) {\
PetscInt __i,__i1,__i2;\
for(__i=0;__i<nnz-1;__i+=2) {__i1 = xi[__i]; __i2=xi[__i+1];\
sum -= (xv[__i]*r[__i1] + xv[__i+1]*r[__i2]);}\
if (nnz & 0x1) sum -= xv[__i] * r[xi[__i]];}

#else
#define PetscSparseDenseMinusDot(sum,r,xv,xi,nnz) {\
PetscInt __i;\
for(__i=0;__i<nnz;__i++) sum -= xv[__i] * r[xi[__i]];}
#endif



/*
    PetscSparseDensePlusDot - The inner kernel of matrix-vector product \sum_i xv[i] * r[xi[i]] for CSR storage

  Input Parameters:
+  nnz - the number of entries
.  r - the array of vector values
.  xv - the matrix values for the row
-  xi - the column indices of the nonzeros in the row

  Output Parameter:
.  sum - the sum of results

  PETSc compile flags:
+   PETSC_KERNEL_USE_UNROLL_4 -  don't use this; it changes nnz and hence is WRONG
-   PETSC_KERNEL_USE_UNROLL_2 -

.seealso: PetscSparseDenseMinusDot()

*/
#ifdef PETSC_KERNEL_USE_UNROLL_4
#define PetscSparseDensePlusDot(sum,r,xv,xi,nnz) {\
if (nnz > 0) {\
switch (nnz & 0x3) {\
case 3: sum += *xv++ * r[*xi++];\
case 2: sum += *xv++ * r[*xi++];\
case 1: sum += *xv++ * r[*xi++];\
nnz -= 4;}\
while (nnz > 0) {\
sum +=  xv[0] * r[xi[0]] + xv[1] * r[xi[1]] +\
	xv[2] * r[xi[2]] + xv[3] * r[xi[3]];\
xv  += 4; xi += 4; nnz -= 4; }}}

#elif defined(PETSC_KERNEL_USE_UNROLL_2)
#define PetscSparseDensePlusDot(sum,r,xv,xi,nnz) {\
PetscInt __i,__i1,__i2;\
for(__i=0;__i<nnz-1;__i+=2) {__i1 = xi[__i]; __i2=xi[__i+1];\
sum += (xv[__i]*r[__i1] + xv[__i+1]*r[__i2]);}\
if (nnz & 0x1) sum += xv[__i] * r[xi[__i]];}

#else
#define PetscSparseDensePlusDot(sum,r,xv,xi,nnz) {\
 PetscInt __i;\
for(__i=0;__i<nnz;__i++) sum += xv[__i] * r[xi[__i]];}
#endif

#endif
